// Include the C++ wrapper instead of the raw header(s)
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>


#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp> // all types inspired from GLSL
// #include <glm/ext.hpp> --> Can't use this, give me error in type_quat.hpp


#define TINYOBJLOADER_IMPLEMENTATION // add this to exactly 1 of your C++ files
#include "tiny_obj_loader.h"

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <iostream>
#include <cassert>
#include <vector>

#include <array>

#include <cmath>


// Avoid the "wgpu::" prefix in front of all WebGPU symbols
using namespace wgpu;

constexpr float PI = 3.14159265358979323846f;



// We define a function that hides implementation-specific variants of device polling:
void wgpuPollEvents([[maybe_unused]] Device device, [[maybe_unused]] bool yieldToWebBrowser) {
#if defined(WEBGPU_BACKEND_DAWN)
	device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
	device.poll(false);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
	if (yieldToWebBrowser) {
		emscripten_sleep(100);
	}
#endif
}





#pragma region Uniforms

struct MyUniforms {
	glm::mat4x4 projectionMatrix;
	glm::mat4x4 viewMatrix;
	glm::mat4x4 modelMatrix;
	glm::vec4 color;
	float time;
	float _pad[3]; // Total size must be a multiple of the alignment size of its largest field (16 Bytes in this case), so add padding
};

// Have the compiler check byte alignment
static_assert(sizeof(MyUniforms) % 16 == 0);

#pragma endregion


#pragma region Vertex Attributes

/**
 * A structure that describes the data layout in the vertex buffer
 * We do not instantiate it but use it in `sizeof` and `offsetof`
 */
struct VertexAttributes {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
};

#pragma endregion


#pragma region Parser

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

class Parser {
public:
	static bool loadGeometry(const fs::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData, int dimensions);

	static bool loadGeometryFromObj(const fs::path& path, std::vector<VertexAttributes>& vertexData);

	static ShaderModule loadShaderModule(const fs::path& path, Device device);
};

bool Parser::loadGeometry(const fs::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData, int dimensions)
{
	std::ifstream file(path);
	if (!file.is_open()) {
		return false;
	}

	pointData.clear();
	indexData.clear();

	enum class Section {
		None,
		Points,
		Indices,
	};
	Section currentSection = Section::None;

	float value;
	uint16_t index;
	std::string line;
	while (!file.eof()) {
		getline(file, line);

		// overcome the `CRLF` problem
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		if (line == "[points]") {
			currentSection = Section::Points;
		}
		else if (line == "[indices]") {
			currentSection = Section::Indices;
		}
		else if (line[0] == '#' || line.empty()) {
			// Do nothing, this is a comment
		}
		else if (currentSection == Section::Points) {
			std::istringstream iss(line);
			// Get x, y, r, g, b
			for (int i = 0; i < dimensions + 3; ++i) {
				iss >> value;
				pointData.push_back(value);
			}
		}
		else if (currentSection == Section::Indices) {
			std::istringstream iss(line);
			// Get corners #0 #1 and #2
			for (int i = 0; i < 3; ++i) {
				iss >> index;
				indexData.push_back(index);
			}
		}
	}
	return true;
}

bool Parser::loadGeometryFromObj(const fs::path& path, std::vector<VertexAttributes>& vertexData)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	// Call the core loading procedure of TinyOBJLoader
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str());

	// Check errors
	if (!warn.empty()) {
		std::cout << warn << std::endl;
	}

	if (!err.empty()) {
		std::cerr << err << std::endl;
	}

	if (!ret) {
		return false;
	}

	// Filling in vertexData:
	vertexData.clear();
	for (const auto& shape : shapes) {
		size_t offset = vertexData.size();
		vertexData.resize(offset + shape.mesh.indices.size());

		for (size_t i = 0; i < shape.mesh.indices.size(); ++i) {
			const tinyobj::index_t& idx = shape.mesh.indices[i];

			vertexData[offset + i].position = {
				attrib.vertices[3 * idx.vertex_index + 0],
				-attrib.vertices[3 * idx.vertex_index + 2], // Add a minus to avoid mirroring
				attrib.vertices[3 * idx.vertex_index + 1]
			};

			// Also apply the transform to normals!!
			vertexData[offset + i].normal = {
				attrib.normals[3 * idx.normal_index + 0],
				-attrib.normals[3 * idx.normal_index + 2],
				attrib.normals[3 * idx.normal_index + 1]
			};

			vertexData[offset + i].color = {
				attrib.colors[3 * idx.vertex_index + 0],
				attrib.colors[3 * idx.vertex_index + 1],
				attrib.colors[3 * idx.vertex_index + 2]
			};
		}
	}

	return true;
}

ShaderModule Parser::loadShaderModule(const fs::path& path, Device device)
{
	std::ifstream file(path);
	if (!file.is_open()) {
		return nullptr;
	}
	file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	std::string shaderSource(size, ' ');
	file.seekg(0);
	file.read(shaderSource.data(), size);

	ShaderModuleWGSLDescriptor shaderCodeDesc{};
	shaderCodeDesc.chain.next = nullptr;
	shaderCodeDesc.chain.sType = SType::ShaderModuleWGSLDescriptor;
	shaderCodeDesc.code = shaderSource.c_str();
	ShaderModuleDescriptor shaderDesc{};
	shaderDesc.nextInChain = &shaderCodeDesc.chain;
	return device.createShaderModule(shaderDesc);
}

#pragma endregion


class Application {
public:
	// Initialize everything and return true if it went all right
	bool Initialize();

	// Uninitialize everything that was initialized
	void Terminate();

	// Draw a frame and handle events
	void MainLoop();

	// Return true as long as the main loop should keep on running
	bool IsRunning();

private:
	TextureView GetNextSurfaceTextureView();
	void SetupDepthTextureView();

	void InitializePipeline();
	void InitializeBuffers();

	void InitializeUniforms();
	void UpdateUniforms();

	void PlayWithBuffers();

	RequiredLimits GetRequiredLimits(Adapter adapter) const;

private:
	// We put here all the variables that are shared between init and main loop
	GLFWwindow *window;
	Device device;
	Queue queue;
	Surface surface;
	std::unique_ptr<ErrorCallback> uncapturedErrorCallbackHandle;

	RenderPipeline pipeline;
	TextureFormat surfaceFormat = TextureFormat::Undefined;
	TextureFormat depthTextureFormat = TextureFormat::Undefined;

	Texture depthTexture;
	TextureView depthTextureView;

	std::vector<VertexAttributes> vertexData;

	Buffer pointBuffer;
	Buffer indexBuffer;
	Buffer vertexBuffer;
	Buffer uniformBuffer;
	uint32_t indexCount;

	BindGroup m_bindGroup;

	MyUniforms uniforms;

	WGPUColor m_backgroundScreenColor = { 0.7, 0.7, 0.7, 1.0 };

	// Object Matrices
	glm::mat4x4 R1;
	glm::mat4x4 T1;
	glm::mat4x4 S;
};

int main() {
	Application app;

	if (!app.Initialize()) {
		return 1;
	}

#ifdef __EMSCRIPTEN__
	// Equivalent of the main loop when using Emscripten:
	auto callback = [](void *arg) {
		Application* pApp = reinterpret_cast<Application*>(arg);
		pApp->MainLoop(); // 4. We can use the application object
	};
	emscripten_set_main_loop_arg(callback, &app, 0, true);
#else // __EMSCRIPTEN__
	while (app.IsRunning()) {
		app.MainLoop();
	}
#endif // __EMSCRIPTEN__

	return 0;
}

bool Application::Initialize() {
	// Open window
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);
	
	Instance instance = wgpuCreateInstance(nullptr);
	
	surface = glfwGetWGPUSurface(instance, window);
	
	std::cout << "Requesting adapter..." << std::endl;
	surface = glfwGetWGPUSurface(instance, window);
	RequestAdapterOptions adapterOpts = {};
	adapterOpts.compatibleSurface = surface;
	Adapter adapter = instance.requestAdapter(adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;
	
	instance.release();
	
	std::cout << "Requesting device..." << std::endl;
	DeviceDescriptor deviceDesc = {};
	deviceDesc.label = "My Device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "The default queue";
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* /* pUserData */) {
		std::cout << "Device lost: reason " << reason;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
	};

	// Before adapter.requestDevice(deviceDesc)
	RequiredLimits requiredLimits = GetRequiredLimits(adapter);
	deviceDesc.requiredLimits = &requiredLimits;

	device = adapter.requestDevice(deviceDesc);
	std::cout << "Got device: " << device << std::endl;
	
	uncapturedErrorCallbackHandle = device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
		std::cout << "Uncaptured device error: type " << type;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
	});
	
	queue = device.getQueue();

	// Configure the surface
	SurfaceConfiguration config = {};
	
	// Configuration of the textures created for the underlying swap chain
	config.width = 640;
	config.height = 480;
	config.usage = TextureUsage::RenderAttachment;
	surfaceFormat = surface.getPreferredFormat(adapter);
	config.format = surfaceFormat;

	// And we do not need any particular view format:
	config.viewFormatCount = 0;
	config.viewFormats = nullptr;
	config.device = device;
	config.presentMode = PresentMode::Fifo;
	config.alphaMode = CompositeAlphaMode::Auto;

	surface.configure(config);

	// Release the adapter only after it has been fully utilized
	adapter.release();

	// At the end of Initialize()
	InitializePipeline();

	// Initialize Buffers
	InitializeBuffers();

	// Buffer experiments
	//PlayWithBuffers();
	
	return true;
}

void Application::Terminate() {
	depthTextureView.release();
	depthTexture.destroy();
	depthTexture.release();

	pointBuffer.release();
	indexBuffer.release();
	pipeline.release();
	surface.unconfigure();
	queue.release();
	surface.release();
	device.release();
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Application::MainLoop() {
	glfwPollEvents();

	UpdateUniforms();

	// Get the next target texture view
	TextureView targetView = GetNextSurfaceTextureView();
	if (!targetView) return;

	SetupDepthTextureView();

	// Create a command encoder for the draw call
	CommandEncoderDescriptor encoderDesc = {};
	encoderDesc.label = "My command encoder";
	CommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);


	// Create the render pass that clears the screen with our color
	RenderPassDescriptor renderPassDesc = {};

	// The attachment part of the render pass descriptor describes the target texture of the pass
	RenderPassColorAttachment renderPassColorAttachment = {};
	renderPassColorAttachment.view = targetView;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = LoadOp::Clear;
	renderPassColorAttachment.storeOp = StoreOp::Store;
	renderPassColorAttachment.clearValue = m_backgroundScreenColor;
#ifndef WEBGPU_BACKEND_WGPU
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	// We now add a depth/stencil attachment:
	RenderPassDepthStencilAttachment depthStencilAttachment;
	depthStencilAttachment.view = depthTextureView;
	depthStencilAttachment.depthClearValue = 1.0f;
	depthStencilAttachment.depthLoadOp = LoadOp::Clear;
	depthStencilAttachment.depthStoreOp = StoreOp::Store;
	depthStencilAttachment.depthReadOnly = false;
	depthStencilAttachment.stencilClearValue = 0;
#ifdef WEBGPU_BACKEND_WGPU
	depthStencilAttachment.stencilLoadOp = LoadOp::Clear;
	depthStencilAttachment.stencilStoreOp = StoreOp::Store;
#else
	depthStencilAttachment.stencilLoadOp = LoadOp::Undefined;
	depthStencilAttachment.stencilStoreOp = StoreOp::Undefined;
#endif
	depthStencilAttachment.stencilReadOnly = true;

	renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
	renderPassDesc.timestampWrites = nullptr;

	// Create the render pass and end it immediately (we only clear the screen but do not draw anything)
	RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);

	// Select which render pipeline to use
	renderPass.setPipeline(pipeline);

	// Set both vertex and index buffers
	renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexData.size() * sizeof(VertexAttributes));
	// The second argument must correspond to the choice of uint16_t or uint32_t

	// Replace `draw()` with `drawIndexed()` and `vertexCount` with `indexCount`
	// The extra argument is an offset within the index buffer.
	// Set binding group
	renderPass.setBindGroup(0, m_bindGroup, 0, nullptr);

	renderPass.draw(indexCount, 1, 0, 0);

	renderPass.end();
	renderPass.release();

	// Finally encode and submit the render pass
	CommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.label = "Command buffer";
	CommandBuffer command = encoder.finish(cmdBufferDescriptor);
	encoder.release();

	std::cout << "Submitting command..." << std::endl;
	queue.submit(1, &command);
	command.release();
	std::cout << "Command submitted." << std::endl;

	// At the enc of the frame
	targetView.release();
#ifndef __EMSCRIPTEN__
	surface.present();
#endif

#if defined(WEBGPU_BACKEND_DAWN)
	device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
	device.poll(false);
#endif
}

bool Application::IsRunning() {
	return !glfwWindowShouldClose(window);
}

TextureView Application::GetNextSurfaceTextureView() {
	// Get the surface texture
	SurfaceTexture surfaceTexture;
	surface.getCurrentTexture(&surfaceTexture);
	if (surfaceTexture.status != SurfaceGetCurrentTextureStatus::Success) {
		return nullptr;
	}
	Texture texture = surfaceTexture.texture;

	// Create a view for this surface texture
	TextureViewDescriptor viewDescriptor;
	viewDescriptor.label = "Surface texture view";
	viewDescriptor.format = texture.getFormat();
	viewDescriptor.dimension = TextureViewDimension::_2D;
	viewDescriptor.baseMipLevel = 0;
	viewDescriptor.mipLevelCount = 1;
	viewDescriptor.baseArrayLayer = 0;
	viewDescriptor.arrayLayerCount = 1;
	viewDescriptor.aspect = TextureAspect::All;
	TextureView targetView = texture.createView(viewDescriptor);

	return targetView;
}

void Application::SetupDepthTextureView()
{
	// Create the depth texture
	TextureDescriptor depthTextureDesc;
	depthTextureDesc.dimension = TextureDimension::_2D;
	depthTextureDesc.format = depthTextureFormat;
	depthTextureDesc.mipLevelCount = 1;
	depthTextureDesc.sampleCount = 1;
	depthTextureDesc.size = { 640, 480, 1 };
	depthTextureDesc.usage = TextureUsage::RenderAttachment;
	depthTextureDesc.viewFormatCount = 1;
	depthTextureDesc.viewFormats = (WGPUTextureFormat*)&depthTextureFormat;
	depthTexture = device.createTexture(depthTextureDesc);

	// Create the view of the depth texture manipulated by the rasterizer
	TextureViewDescriptor depthTextureViewDesc;
	depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
	depthTextureViewDesc.baseArrayLayer = 0;
	depthTextureViewDesc.arrayLayerCount = 1;
	depthTextureViewDesc.baseMipLevel = 0;
	depthTextureViewDesc.mipLevelCount = 1;
	depthTextureViewDesc.dimension = TextureViewDimension::_2D;
	depthTextureViewDesc.format = depthTextureFormat;
	depthTextureView = depthTexture.createView(depthTextureViewDesc);
}


void Application::InitializePipeline()
{
	std::cout << "Creating shader module..." << std::endl;
	ShaderModule shaderModule = Parser::loadShaderModule(RESOURCE_DIR "/shader.wgsl", device);
	std::cout << "Shader module: " << shaderModule << std::endl;

	RenderPipelineDescriptor pipelineDesc;

	// [...] Describe vertex pipeline state
	// Configure 'pipelineDesc.vertex'
	// [...] Describe vertex buffers
	// Vertex fetch
	VertexBufferLayout vertexBufferLayout;
	// [...] Describe the vertex buffer layout
	// We now have 2 attributes
	std::vector<VertexAttribute> vertexAttribs(3);

	// Describe the position attribute
	vertexAttribs[0].shaderLocation = 0; // @location(0)
	vertexAttribs[0].format = VertexFormat::Float32x3;
	vertexAttribs[0].offset = offsetof(VertexAttributes, position);

	// Normal attribute
	vertexAttribs[1].shaderLocation = 1;
	vertexAttribs[1].format = VertexFormat::Float32x3;
	vertexAttribs[1].offset = offsetof(VertexAttributes, normal);

	// Describe the color attribute
	vertexAttribs[2].shaderLocation = 2; // @location(2)
	vertexAttribs[2].format = VertexFormat::Float32x3; // different type!
	vertexAttribs[2].offset = offsetof(VertexAttributes, color);; // adjusted for 3D

	vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
	vertexBufferLayout.attributes = vertexAttribs.data();

	// [...] Describe buffer stride and step mode

	vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
	//                               ^^^^^^^^^^^^^^^^^^^^^^^^ This was 6 * sizeof(float)

	vertexBufferLayout.stepMode = VertexStepMode::Vertex;


	pipelineDesc.vertex.bufferCount = 1;
	pipelineDesc.vertex.buffers = &vertexBufferLayout;
	// [...] Describe vertex shader
	// NB: We define the 'shaderModule' in the second part of this chapter.
	// Here we tell that the programmable vertex shader stage is described
	// by the function called 'vs_main' in that module.
	pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;
	
	// [...] Describe primitive pipeline state
	// Each sequence of 3 vertices is considered as a triangle
	pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;

	// We'll see later how to specify the order in which vertices should be
	// connected. When not specified, vertices are considered sequentially.
	pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;

	// The face orientation is defined by assuming that when looking
	// from the front of the face, its corner vertices are enumerated
	// in the counter-clockwise (CCW) order.
	pipelineDesc.primitive.frontFace = FrontFace::CCW;

	// But the face orientation does not matter much because we do not
	// cull (i.e. "hide") the faces pointing away from us (which is often
	// used for optimization).
	pipelineDesc.primitive.cullMode = CullMode::None;

	
	// [...] Describe fragment pipeline state
	// We tell that the programmable fragment shader stage is described
	// by the function called 'fs_main' in the shader module.
	FragmentState fragmentState;
	fragmentState.module = shaderModule;
	fragmentState.entryPoint = "fs_main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;

	// [...] We'll configure the blending stage here
	BlendState blendState;
	// [...] Configure color blending equation
	blendState.color.srcFactor = BlendFactor::SrcAlpha;
	blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
	blendState.color.operation = BlendOperation::Add;
	// [...] Configure alpha blending equation
	blendState.alpha.srcFactor = BlendFactor::Zero;
	blendState.alpha.dstFactor = BlendFactor::One;
	blendState.alpha.operation = BlendOperation::Add;

	ColorTargetState colorTarget;
	colorTarget.format = surfaceFormat;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = ColorWriteMask::All; // We could write to only some of the color channels.

	// We have only one target because our render pass has only one output color
	// attachment.
	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;

	pipelineDesc.fragment = &fragmentState;



	// [...] Describe stencil/depth pipeline state
	DepthStencilState depthStencilState = Default;

	// A fragment is blended only if its depth is less than the current value of the Z-Buffer
	depthStencilState.depthCompare = CompareFunction::Less;

	// We want to write the new depth each time a fragment is blended
	depthStencilState.depthWriteEnabled = true;

	// Store the format in a variable as later parts of the code depend on it
	depthTextureFormat = TextureFormat::Depth24Plus;
	depthStencilState.format = depthTextureFormat;

	// Deactivate the stencil alltogether
	depthStencilState.stencilReadMask = 0;
	depthStencilState.stencilWriteMask = 0;

	// Setup depth state
	pipelineDesc.depthStencil = &depthStencilState;

	// [...] Describe multi-sampling state
	// Samples per pixel
	pipelineDesc.multisample.count = 1;
	// Default value for the mask, meaning "all bits on"
	pipelineDesc.multisample.mask = ~0u;
	// Default value as well (irrelevant for count = 1 anyways)
	pipelineDesc.multisample.alphaToCoverageEnabled = false;



	// Create uniform buffer
	// The buffer will now contain MyUniforms
	BufferDescriptor bufferDesc;
	bufferDesc.size = sizeof(MyUniforms);
	// Make sure to flag the buffer as BufferUsage::Uniform
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	uniformBuffer = device.createBuffer(bufferDesc);

	InitializeUniforms();

	queue.writeBuffer(uniformBuffer, 0, &uniforms, sizeof(MyUniforms));



	// [...] Define bindingLayout
	// Create binding layout (don't forget to = Default)
	BindGroupLayoutEntry bindingLayout = Default;

	// The binding index as used in the @binding attribute in the shader
	bindingLayout.binding = 0;

	// The stage that needs to access this resource
	bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;

	bindingLayout.buffer.type = BufferBindingType::Uniform;
	bindingLayout.buffer.minBindingSize = sizeof(MyUniforms);
	bindingLayout.buffer.hasDynamicOffset = 0;

	// Create a bind group layout
	BindGroupLayoutDescriptor bindGroupLayoutDesc;
	bindGroupLayoutDesc.entryCount = 1;
	bindGroupLayoutDesc.entries = &bindingLayout;
	BindGroupLayout bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutDesc);



	// Create the pipeline layout
	PipelineLayoutDescriptor layoutDesc;
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
	PipelineLayout layout = device.createPipelineLayout(layoutDesc);

	// Assign the PipelineLayout to the RenderPipelineDescriptor's layout field
	pipelineDesc.layout = layout;



	// Create a binding
	BindGroupEntry binding;
	
	// The index of the binding (the entries in bindGroupDesc can be in any order)
	binding.binding = 0;
	// The buffer it is actually bound to
	binding.buffer = uniformBuffer;
	// We can specify an offset within the buffer, so that a single buffer can hold
	// multiple uniform blocks.
	binding.offset = 0;
	// And we specify again the size of the buffer.
	binding.size = sizeof(MyUniforms);



	// A bind group contains one or multiple bindings
	BindGroupDescriptor bindGroupDesc;
	bindGroupDesc.label = "Uniform Bind Group 1";
	bindGroupDesc.layout = bindGroupLayout;
	// There must be as many bindings as declared in the layout!
	bindGroupDesc.entryCount = bindGroupLayoutDesc.entryCount;
	bindGroupDesc.entries = &binding;
	m_bindGroup = device.createBindGroup(bindGroupDesc);

	pipeline = device.createRenderPipeline(pipelineDesc);



	// We no longer need to access the shader module
	shaderModule.release();
}

void Application::InitializeBuffers()
{
	// Define point data
	// The de-duplicated list of point positions
	std::vector<float> pointData;
	std::vector<uint16_t> indexData;

	// Load mesh data from OBJ file
	bool success = Parser::loadGeometryFromObj(RESOURCE_DIR "/flatspot_car.obj", vertexData);
	if (!success) {
		std::cerr << "Could not load geometry!" << std::endl;
		return;
	}

	// Create vertex buffer
	BufferDescriptor bufferDesc;
	bufferDesc.size = vertexData.size() * sizeof(VertexAttributes); // changed
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
	bufferDesc.mappedAtCreation = false;
	vertexBuffer = device.createBuffer(bufferDesc);
	queue.writeBuffer(vertexBuffer, 0, vertexData.data(), bufferDesc.size); // changed

	indexCount = static_cast<uint16_t>(vertexData.size()); // changed

	// Create index buffer
	// (we reuse the bufferDesc initialized for the pointBuffer)
	// indexBuffer = device.createBuffer(bufferDesc);

	// queue.writeBuffer(indexBuffer, 0, indexData.data(), bufferDesc.size);

	// # Creation of Uniform Buffer is done in InitializePipeline(), because uniform Buffer is assigned earlier there already ###
}


void Application::InitializeUniforms()
{
	// Upload the initial value of the uniforms
	uniforms = MyUniforms();

	// Option A: Manually define matrices
	// Scale the object
	S = transpose(glm::mat4x4(1.0f));

	// Translate the object
	T1 = transpose(glm::mat4x4(
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	));

	// Translate the view
	glm::vec3 focalPoint(0.0, 0.0, -3.0);
	glm::mat4x4 T2 = transpose(glm::mat4x4(
		1.0, 0.0, 0.0, -focalPoint.x,
		0.0, 1.0, 0.0, -focalPoint.y,
		0.0, 0.0, 1.0, -focalPoint.z,
		0.0, 0.0, 0.0, 1.0
	));

	// Rotate the object
	float angle1 = 1.0f; // arbitrary time
	float c1 = cos(angle1);
	float s1 = sin(angle1);
	R1 = transpose(glm::mat4x4(
		c1, s1, 0.0, 0.0,
		-s1, c1, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	));

	// Rotate the view point
	float angle2 = 3.0f * PI / 4.0f;
	float c2 = cos(angle2);
	float s2 = sin(angle2);
	glm::mat4x4 R2 = transpose(glm::mat4x4(
		1.0, 0.0, 0.0, 0.0,
		0.0, c2, s2, 0.0,
		0.0, -s2, c2, 0.0,
		0.0, 0.0, 0.0, 1.0
	));

	uniforms.modelMatrix = R1 * T1 * S;
	uniforms.viewMatrix = T2 * R2;

	float ratio = 640.0f / 480.0f;
	float focalLength = 1.5f;
	float near = 0.01f;
	float far = 100.0f;
	float divider = 1 / (focalLength * (far - near));
	uniforms.projectionMatrix = transpose(glm::mat4x4(
		1.0, 0.0, 0.0, 0.0,
		0.0, ratio, 0.0, 0.0,
		0.0, 0.0, far * divider, -far * near * divider,
		0.0, 0.0, 1.0 / focalLength, 0.0
	));

	uniforms.color = { 0.0f, 0.0f, 0.0f, 1.0f };
	uniforms.time = 1.0f;
}



void Application::UpdateUniforms()
{
	uniforms.time = static_cast<float>(glfwGetTime());
	// uniforms.color = { 5.0f * cos(uniforms.time), sin(uniforms.time), -sin(uniforms.time), 1.0f};

	// Upload only the time, whichever its order in the struct
	queue.writeBuffer(uniformBuffer, offsetof(MyUniforms, time), &uniforms.time, sizeof(MyUniforms::time));

	// Update view matrix
	float angle1 = uniforms.time; // arbitrary time
	float c1 = cos(angle1);
	float s1 = sin(angle1);
	R1 = transpose(glm::mat4x4(
		c1, s1, 0.0, 0.0,
		-s1, c1, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	));

	uniforms.modelMatrix = R1 * T1 * S;
	queue.writeBuffer(uniformBuffer, offsetof(MyUniforms, modelMatrix), &uniforms.modelMatrix, sizeof(MyUniforms::modelMatrix));
}


void Application::PlayWithBuffers()
{
	// Experimentation for the "Playing with buffer" chapter

	// Create a First Buffer
	BufferDescriptor bufferDesc;
	bufferDesc.label = "Some GPU-side data buffer";
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::CopySrc;
	bufferDesc.size = 16;
	bufferDesc.mappedAtCreation = false;
	Buffer buffer1 = device.createBuffer(bufferDesc);

	// Create a second buffer
	bufferDesc.label = "Output buffer";
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::MapRead;
	Buffer buffer2 = device.createBuffer(bufferDesc);

	// Write input data
	// Create some CPU-side data buffer (of size 16 bytes)
	std::vector<uint8_t> numbers(16);
	for (uint8_t i = 0; i < 16; ++i) numbers[i] = i;
	// `numbers` now contains [ 0, 1, 2, ... ]

	// Copy this from `numbers` (RAM) to `buffer1` (VRAM)
	queue.writeBuffer(buffer1, 0, numbers.data(), numbers.size());

	// Encode and submit the buffer to buffer copy
	CommandEncoder encoder = device.createCommandEncoder(Default);

	// After creating the command encoder
	encoder.copyBufferToBuffer(buffer1, 0, buffer2, 0, 16);

	CommandBuffer command = encoder.finish(Default);
	encoder.release();
	queue.submit(1, &command);
	command.release();

	// Read buffer data back
	// The context shared between this main function and the callback.
	struct Context {
		bool ready;
		Buffer buffer;
	};

	auto onBuffer2Mapped = [](WGPUBufferMapAsyncStatus status, void* pUserData) {
		Context* context = reinterpret_cast<Context*>(pUserData);
		context->ready = true;
		std::cout << "Buffer 2 mapped with status " << status << std::endl;
		if (status != BufferMapAsyncStatus::Success) return;

		// Use context->buffer here
		// Get a pointer to wherever the driver mapped the GPU memory to the RAM
		uint8_t* bufferData = (uint8_t*)context->buffer.getConstMappedRange(0, 16);

		// Do stuff with bufferData
		std::cout << "bufferData = [";
		for (int i = 0; i < 16; ++i) {
			if (i > 0) std::cout << ", ";
			std::cout << (int)bufferData[i];
		}
		std::cout << "]" << std::endl;

		// Then do not forget to unmap the memory
		context->buffer.unmap();
		};

	// Create the Context instance
	Context context = { false, buffer2 };

	wgpuBufferMapAsync(buffer2, MapMode::Read, 0, 16, onBuffer2Mapped, (void*)&context);
	//                   Pass the address of the Context instance here: ^^^^^^^^^^^^^^

	while (!context.ready) {
		//  ^^^^^^^^^^^^^ Use context.ready here instead of ready
		wgpuPollEvents(device, true /* yieldToBrowser */);
	}


	// [...] Release buffers
	buffer1.release();
	buffer2.release();
}


RequiredLimits Application::GetRequiredLimits(Adapter adapter) const
{
	// Get adapter supported limits, in case we need them
	SupportedLimits supportedLimits;
	adapter.getLimits(&supportedLimits);

	//CHECK --> std::cout << "adapter.maxVertexAttributes: " << supportedLimits.limits.maxVertexAttributes << std::endl;*/


	RequiredLimits requiredLimits = Default;
	requiredLimits.limits.maxVertexAttributes = 3;
	requiredLimits.limits.maxVertexBuffers = 1;
	requiredLimits.limits.maxBufferSize = 10000 * sizeof(VertexAttributes);
	requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.maxInterStageShaderComponents = 6;
	// We use at most 1 bind group for now
	requiredLimits.limits.maxBindGroups = 1;
	// We use at most 1 uniform buffer per stage
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
	// Uniform structs have a size of maximum 16 float
	requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);

	// For the depth buffer, we enable textures (up to the size of the window):
	requiredLimits.limits.maxTextureDimension1D = 480;
	requiredLimits.limits.maxTextureDimension2D = 640;
	requiredLimits.limits.maxTextureArrayLayers = 1;

	return requiredLimits;
}
