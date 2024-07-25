#include "Application.h"

#include "glm/gtx/polar_coordinates.hpp"

// Custom ImGui widgets
namespace ImGui {
	bool DragDirection(const char* label, glm::vec4& direction) {
		glm::vec2 angles = glm::degrees(glm::polar(glm::vec3(direction)));
		bool changed = ImGui::DragFloat2(label, glm::value_ptr(angles));
		direction = glm::vec4(glm::euclidean(glm::radians(angles)), direction.w);
		return changed;
	}
} // namespace ImGui

constexpr float PI = 3.14159265358979323846f;

bool Application::Initialize() {
	InitWindow();
	InitInstanceAndSurface();
	InitDevice();
	InitQueue();

	ConfigureSurface();

	if (!InitLightingUniforms()) return false;

	// At the end of Initialize()
	InitPipeline();
	InitBuffers();

	if (!InitGameObjects()) return false;

	if (!InitGui()) return false;
	
	return true;
}


void Application::Terminate() {
	TerminateGui();

	m_depthTextureView.release();
	m_depthTexture.destroy();
	m_depthTexture.release();

	m_baseColorTexture.destroy();
	m_baseColorTexture.release();
	m_normalTexture.destroy();
	m_normalTexture.release();
	m_pipeline.release();
	m_surface.unconfigure();
	m_queue.release();
	m_surface.release();
	m_device.release();
	glfwDestroyWindow(m_window);
	glfwTerminate();
}


void Application::MainLoop() {
	glfwPollEvents();

	UpdateDragInertia();
	UpdateUniforms();
	UpdateLightingUniforms();

	// Get the next target texture view
	TextureView targetView = GetNextSurfaceTextureView();
	if (!targetView) return;

	// Create a command encoder for the draw call
	CommandEncoderDescriptor encoderDesc = {};
	encoderDesc.label = "My command encoder";
	CommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &encoderDesc);


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
	depthStencilAttachment.view = m_depthTextureView;
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
	renderPass.setPipeline(m_pipeline);

	// Set both vertex and index buffers
	// renderPass.setVertexBuffer(0, m_vertexBuffer, 0, m_vertexData.size() * sizeof(VertexAttributes));
	GameObject testGameObject = m_gameObjects[m_gameObjects.size() - 1];
	renderPass.setVertexBuffer(0, testGameObject.GetVertexBuffer(), 0, testGameObject.GetVertexData().size() * sizeof(VertexAttributes));
	// The second argument must correspond to the choice of uint16_t or uint32_t

	// Replace `draw()` with `drawIndexed()` and `vertexCount` with `indexCount`
	// The extra argument is an offset within the index buffer.
	// Set binding group
	renderPass.setBindGroup(0, testGameObject.GetBindGroup(), 0, nullptr);

	renderPass.draw(testGameObject.GetIndexCount(), 1, 0, 0);

	// We add the GUI drawing commands to the render pass
	UpdateGui(renderPass);

	renderPass.end();
	renderPass.release();

	// Finally encode and submit the render pass
	CommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.label = "Command buffer";
	CommandBuffer command = encoder.finish(cmdBufferDescriptor);
	encoder.release();

	std::cout << "Submitting command..." << std::endl;
	m_queue.submit(1, &command);
	command.release();
	std::cout << "Command submitted." << std::endl;

	// At the enc of the frame
	targetView.release();
#ifndef __EMSCRIPTEN__
	m_surface.present();
#endif

#if defined(WEBGPU_BACKEND_DAWN)
	m_device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
	device.poll(false);
#endif
}

bool Application::IsRunning() {
	return !glfwWindowShouldClose(m_window);
}


void Application::OnResize()
{
	// Terminate in reverse order
	// terminateDepthBuffer();
	m_depthTextureView.release();
	m_depthTexture.destroy();
	m_depthTexture.release();

	// terminateSwapChain();
	m_surface.unconfigure();
	m_surface.release();

	// Re-init

	// InitDepthBuffer();
	UpdateWindowDimensions();

	InitInstanceAndSurface();
	ConfigureSurface();

	// Create the depth texture
	InitDepthTextureView();
	

	UpdateProjectionMatrix();
}


void Application::OnMouseMove(double xpos, double ypos)
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		// Don't rotate the camera if the mouse is already captured by an ImGui
		// interaction at this frame.
		return;
	}

	if (m_drag.active) {
		glm::vec2 currentMouse = glm::vec2(-(float)xpos, (float)ypos);

		// glm::vec2 delta = (m_drag.startMouse - currentMouse) * m_drag.sensitivity;		// Inverted
		glm::vec2 delta = (currentMouse - m_drag.startMouse) * m_drag.sensitivity;		// Not Inverted

		m_cameraState.angles = m_drag.startCameraState.angles + delta;
		// Clamp to avoid going too far when orbitting up/down
		m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
		UpdateViewMatrix();

		// Inertia
		m_drag.velocity = delta - m_drag.previousDelta;
		m_drag.previousDelta = delta;
	}
}


void Application::OnMouseButton(int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		switch (action) {
		case GLFW_PRESS:
			m_drag.active = true;
			double xpos, ypos;
			glfwGetCursorPos(m_window, &xpos, &ypos);
			m_drag.startMouse = glm::vec2(-(float)xpos, (float)ypos);
			m_drag.startCameraState = m_cameraState;
			break;
		case GLFW_RELEASE:
			m_drag.active = false;
			break;
		}
	}
}


void Application::OnScroll(double xoffset, double yoffset)
{
	m_cameraState.zoom += m_drag.scrollSensitivity * static_cast<float>(yoffset);
	m_cameraState.zoom = glm::clamp(m_cameraState.zoom, -2.0f, 2.0f);
	UpdateViewMatrix();
}


// /////////////////////////////////////////////////////////////////////
// private:

void Application::InitWindow()
{
	// Open window
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_window = glfwCreateWindow((int)m_windowDimensions.x, (int)m_windowDimensions.y, "Learn WebGPU", nullptr, nullptr);
}


void Application::InitInstanceAndSurface()
{
	Instance instance = wgpuCreateInstance(nullptr);

	m_surface = glfwGetWGPUSurface(instance, m_window);

	std::cout << "Requesting adapter..." << std::endl;
	m_surface = glfwGetWGPUSurface(instance, m_window);
	RequestAdapterOptions adapterOpts = {};
	adapterOpts.compatibleSurface = m_surface;
	m_adapter = instance.requestAdapter(adapterOpts);
	std::cout << "Got adapter: " << m_adapter << std::endl;

	instance.release();
}


void Application::InitDevice()
{
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
	RequiredLimits requiredLimits = GetRequiredLimits(m_adapter);
	deviceDesc.requiredLimits = &requiredLimits;

	m_device = m_adapter.requestDevice(deviceDesc);
	std::cout << "Got device: " << m_device << std::endl;

	uncapturedErrorCallbackHandle = m_device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
		std::cout << "Uncaptured device error: type " << type;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
		});
}

void Application::InitQueue()
{
	m_queue = m_device.getQueue();
}


void Application::ConfigureSurface()
{
	// Configure the surface
	SurfaceConfiguration config = {};

	// Configuration of the textures created for the underlying swap chain
	config.width = static_cast<uint32_t>(m_windowDimensions.x);
	config.height = static_cast<uint32_t>(m_windowDimensions.y);
	config.usage = TextureUsage::RenderAttachment;
	m_surfaceFormat = m_surface.getPreferredFormat(m_adapter);
	config.format = m_surfaceFormat;

	// And we do not need any particular view format:
	config.viewFormatCount = 0;
	config.viewFormats = nullptr;
	config.device = m_device;
	config.presentMode = PresentMode::Fifo;
	config.alphaMode = CompositeAlphaMode::Auto;

	m_surface.configure(config);

	// Set the user pointer to be "this"
	glfwSetWindowUserPointer(m_window, this);
	// Use a non-capturing lambda as resize callback
	glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int, int) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->OnResize();
		});
	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->OnMouseMove(xpos, ypos);
		});
	glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->OnMouseButton(button, action, mods);
		});
	glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->OnScroll(xoffset, yoffset);
		});


	m_adapter.release();
}

void Application::InitDepthTextureView()
{
	// Create the depth texture
	TextureDescriptor depthTextureDesc;
	depthTextureDesc.dimension = TextureDimension::_2D;
	depthTextureDesc.format = m_depthTextureFormat;
	depthTextureDesc.mipLevelCount = 1;
	depthTextureDesc.sampleCount = 1;
	depthTextureDesc.size = { static_cast<uint32_t>(m_windowDimensions.x), static_cast<uint32_t>(m_windowDimensions.y), 1 };
	depthTextureDesc.usage = TextureUsage::RenderAttachment;
	depthTextureDesc.viewFormatCount = 1;
	depthTextureDesc.viewFormats = (WGPUTextureFormat*)&m_depthTextureFormat;
	m_depthTexture = m_device.createTexture(depthTextureDesc);

	// Create the view of the depth texture manipulated by the rasterizer
	TextureViewDescriptor depthTextureViewDesc;
	depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
	depthTextureViewDesc.baseArrayLayer = 0;
	depthTextureViewDesc.arrayLayerCount = 1;
	depthTextureViewDesc.baseMipLevel = 0;
	depthTextureViewDesc.mipLevelCount = 1;
	depthTextureViewDesc.dimension = TextureViewDimension::_2D;
	depthTextureViewDesc.format = m_depthTextureFormat;
	m_depthTextureView = m_depthTexture.createView(depthTextureViewDesc);
}


void Application::InitTexture()
{
	m_baseColorTextureView = nullptr;
	m_baseColorTexture = Loader::loadTexture(RESOURCE_DIR "/cobblestone_floor_08_diff_4k.jpg", m_device, &m_baseColorTextureView);
	m_normalTextureView = nullptr;
	m_normalTexture = Loader::loadTexture(RESOURCE_DIR "/cobblestone_floor_08_nor_gl_4k.png", m_device, &m_normalTextureView);
	if (!m_baseColorTexture) {
		std::cerr << "Could not load baseColor texture!" << std::endl;
	}
	if (!m_normalTexture) {
		std::cerr << "Could not load normal texture!" << std::endl;
	}
}


void Application::InitSampler()
{
	SamplerDescriptor samplerDesc;
	samplerDesc.addressModeU = AddressMode::Repeat;
	samplerDesc.addressModeV = AddressMode::Repeat;
	samplerDesc.addressModeW = AddressMode::ClampToEdge;
	// samplerDesc.magFilter = FilterMode::Nearest;
	samplerDesc.magFilter = FilterMode::Linear;
	// Also setup the sampler to use these mip levels
	samplerDesc.minFilter = FilterMode::Linear;
	samplerDesc.mipmapFilter = MipmapFilterMode::Linear;
	samplerDesc.lodMinClamp = 0.0f;
	samplerDesc.lodMaxClamp = 8.0f;
	samplerDesc.compare = CompareFunction::Undefined;
	samplerDesc.maxAnisotropy = 1;
	m_sampler = m_device.createSampler(samplerDesc);
}


bool Application::InitGameObjects()
{
	m_gameObjects.push_back(
		GameObject(
			std::make_shared<Device>(m_device),
			"Flat Spot Car",
			RESOURCE_DIR "/flatspot_car_2.obj",
			glm::vec3(0),
			std::make_shared<Buffer>(m_uniformBuffer),
			std::make_shared<Buffer>(m_lightingUniformBuffer),
			std::make_shared<Sampler>(m_sampler),
			std::make_shared<BindGroupLayout>(m_bindGroupLayout)
		)
	);

	m_gameObjects[m_gameObjects.size() - 1].SetAlbedoTexture(RESOURCE_DIR "/cobblestone_floor_08_diff_4k.jpg");
	m_gameObjects[m_gameObjects.size() - 1].SetNormalTexture(RESOURCE_DIR "/cobblestone_floor_08_nor_gl_4k.png");

	m_gameObjects[m_gameObjects.size() - 1].Initialize();

	return true;
}


void Application::InitPipeline()
{
	std::cout << "Creating shader module..." << std::endl;
	ShaderModule shaderModule = Loader::loadShaderModule(RESOURCE_DIR "/shader.wgsl", m_device);
	std::cout << "Shader module: " << shaderModule << std::endl;

	RenderPipelineDescriptor pipelineDesc;

	// [...] Describe vertex pipeline state
	// Configure 'pipelineDesc.vertex'
	// [...] Describe vertex buffers
	// Vertex fetch
	VertexBufferLayout vertexBufferLayout;
	// [...] Describe the vertex buffer layout
	// We now have 2 attributes
	std::vector<VertexAttribute> vertexAttribs(6);
	//                                         ^ was 4 

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

	// UV attribute
	vertexAttribs[3].shaderLocation = 3;
	vertexAttribs[3].format = VertexFormat::Float32x2;
	vertexAttribs[3].offset = offsetof(VertexAttributes, uv);

	// Tangent attribute
	vertexAttribs[4].shaderLocation = 4;
	vertexAttribs[4].format = VertexFormat::Float32x3;
	vertexAttribs[4].offset = offsetof(VertexAttributes, tangent);

	// Bitangent attribute
	vertexAttribs[5].shaderLocation = 5;
	vertexAttribs[5].format = VertexFormat::Float32x3;
	vertexAttribs[5].offset = offsetof(VertexAttributes, bitangent);

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
	colorTarget.format = m_surfaceFormat;
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
	m_depthTextureFormat = TextureFormat::Depth24Plus;
	depthStencilState.format = m_depthTextureFormat;

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
	bufferDesc.size = sizeof(GameObject::MyUniforms);
	// Make sure to flag the buffer as BufferUsage::Uniform
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	m_uniformBuffer = m_device.createBuffer(bufferDesc);

	InitUniforms();

	m_queue.writeBuffer(m_uniformBuffer, 0, &m_uniforms, sizeof(GameObject::MyUniforms));



	// [...] Define bindingLayout
	// Create binding layouts
	// Since we now have 2 bindings, we use a vector to store them
	std::vector<BindGroupLayoutEntry> bindingLayoutEntries(5, Default);
	//                                                     ^ This was a 4

	// The uniform buffer binding that we already had
	BindGroupLayoutEntry& bindingLayout = bindingLayoutEntries[0];
	bindingLayout.binding = 0;
	bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
	bindingLayout.buffer.type = BufferBindingType::Uniform;
	bindingLayout.buffer.minBindingSize = sizeof(GameObject::MyUniforms);

	// The texture binding
	BindGroupLayoutEntry& textureBindingLayout = bindingLayoutEntries[1];
	// Setup texture binding
	textureBindingLayout.binding = 1;
	textureBindingLayout.visibility = ShaderStage::Fragment;
	textureBindingLayout.texture.sampleType = TextureSampleType::Float;
	textureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;

	// The normal map binding
	BindGroupLayoutEntry& normalTextureBindingLayout = bindingLayoutEntries[2];
	normalTextureBindingLayout.binding = 2;
	normalTextureBindingLayout.visibility = ShaderStage::Fragment;
	normalTextureBindingLayout.texture.sampleType = TextureSampleType::Float;
	normalTextureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;

	// The texture sampler binding
	BindGroupLayoutEntry& samplerBindingLayout = bindingLayoutEntries[3];
	samplerBindingLayout.binding = 3;
	samplerBindingLayout.visibility = ShaderStage::Fragment;
	samplerBindingLayout.sampler.type = SamplerBindingType::Filtering;

	// The lighting uniform buffer binding
	BindGroupLayoutEntry& lightingUniformLayout = bindingLayoutEntries[4];
	lightingUniformLayout.binding = 4;
	lightingUniformLayout.visibility = ShaderStage::Fragment; // only Fragment is needed
	lightingUniformLayout.buffer.type = BufferBindingType::Uniform;
	lightingUniformLayout.buffer.minBindingSize = sizeof(GameObject::LightingUniforms);


	// Create a bind group layout
	BindGroupLayoutDescriptor bindGroupLayoutDesc{};
	bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
	bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
	m_bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);



	// Create the pipeline layout
	PipelineLayoutDescriptor layoutDesc;
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_bindGroupLayout;
	PipelineLayout layout = m_device.createPipelineLayout(layoutDesc);

	// Assign the PipelineLayout to the RenderPipelineDescriptor's layout field
	pipelineDesc.layout = layout;

	m_pipeline = m_device.createRenderPipeline(pipelineDesc);

	InitDepthTextureView();

	InitTexture();

	InitSampler();

	// Create a binding
	std::vector<BindGroupEntry> bindings(5);
	//                                   ^ This was a 4

	bindings[0].binding = 0;
	bindings[0].buffer = m_uniformBuffer;
	bindings[0].offset = 0;
	bindings[0].size = sizeof(GameObject::MyUniforms);

	bindings[1].binding = 1;
	bindings[1].textureView = m_baseColorTextureView;

	bindings[2].binding = 2;
	bindings[2].textureView = m_normalTextureView;

	bindings[3].binding = 3;
	bindings[3].sampler = m_sampler;

	bindings[4].binding = 4;
	bindings[4].buffer = m_lightingUniformBuffer;
	bindings[4].offset = 0;
	bindings[4].size = sizeof(GameObject::LightingUniforms);

	BindGroupDescriptor bindGroupDesc;
	bindGroupDesc.layout = m_bindGroupLayout;
	bindGroupDesc.entryCount = (uint32_t)bindings.size();
	bindGroupDesc.entries = bindings.data();
	m_bindGroup = m_device.createBindGroup(bindGroupDesc);

	// We no longer need to access the shader module
	shaderModule.release();
}


void Application::InitBuffers()
{
	std::string myPath = RESOURCE_DIR "/flatspot_car_2.obj";
	// Load mesh data from OBJ file
	bool success = Loader::loadGeometryFromObj(myPath, m_vertexData);
	if (!success) {
		std::cerr << "Could not load geometry!" << std::endl;
		return;
	}

	// Create vertex buffer
	BufferDescriptor bufferDesc;
	bufferDesc.size = m_vertexData.size() * sizeof(VertexAttributes); // changed
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
	bufferDesc.mappedAtCreation = false;
	m_vertexBuffer = m_device.createBuffer(bufferDesc);
	m_queue.writeBuffer(m_vertexBuffer, 0, m_vertexData.data(), bufferDesc.size); // changed

	m_indexCount = static_cast<int>(m_vertexData.size()); // changed

	// Create index buffer
	// (we reuse the bufferDesc initialized for the pointBuffer)
	// indexBuffer = device.createBuffer(bufferDesc);

	// queue.writeBuffer(indexBuffer, 0, indexData.data(), bufferDesc.size);

	// # Creation of Uniform Buffer is done in InitializePipeline(), because uniform Buffer is assigned earlier there already ###
}


void Application::InitUniforms()
{
	// Upload the initial value of the uniforms
	m_uniforms = GameObject::MyUniforms();

	// Matrices
	m_uniforms.modelMatrix = glm::mat4x4(1.0);
	m_uniforms.viewMatrix = glm::lookAt(glm::vec3(-1.0f, -2.0f, 1.0f), glm::vec3(0.0f), glm::vec3(0, 0, 1));
	m_uniforms.projectionMatrix = glm::perspective(45 * PI / 180, m_windowDimensions.x / m_windowDimensions.y, 0.01f, 100.0f);

	m_uniforms.color = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_uniforms.time = 1.0f;

	UpdateViewMatrix();
}



void Application::UpdateUniforms()
{
	m_uniforms.time = static_cast<float>(glfwGetTime());
	// uniforms.color = { 5.0f * cos(uniforms.time), sin(uniforms.time), -sin(uniforms.time), 1.0f};

	// Upload only the time, whichever its order in the struct
	m_queue.writeBuffer(m_uniformBuffer, offsetof(GameObject::MyUniforms, time), &m_uniforms.time, sizeof(GameObject::MyUniforms::time));

	// In the main loop
	/*float viewZ = glm::mix(0.0f, 0.25f, cos(2 * PI * uniforms.time / 4) * 0.5 + 0.5);
	uniforms.viewMatrix = glm::lookAt(glm::vec3(-0.5f, -1.5f, viewZ + 0.25f), glm::vec3(0.0f), glm::vec3(0, 0, 1));
	queue.writeBuffer(uniformBuffer, offsetof(MyUniforms, viewMatrix), &uniforms.viewMatrix, sizeof(MyUniforms::viewMatrix));*/
}


void Application::UpdateWindowDimensions()
{
	// Get the current size of the window's framebuffer:
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	m_windowDimensions = glm::vec2(width, height);
}


void Application::UpdateProjectionMatrix()
{
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);
	float ratio = width / (float)height;
	m_uniforms.projectionMatrix = glm::perspective(45 * PI / 180, ratio, 0.01f, 100.0f);
	m_queue.writeBuffer(
		m_uniformBuffer,
		offsetof(GameObject::MyUniforms, projectionMatrix),
		&m_uniforms.projectionMatrix,
		sizeof(GameObject::MyUniforms::projectionMatrix)
	);
}


void Application::UpdateViewMatrix()
{
	float cx = cos(m_cameraState.angles.x);
	float sx = sin(m_cameraState.angles.x);
	float cy = cos(m_cameraState.angles.y);
	float sy = sin(m_cameraState.angles.y);
	glm::vec3 position = glm::vec3(cx * cy, sx * cy, sy) * std::exp(-m_cameraState.zoom);
	m_uniforms.viewMatrix = glm::lookAt(position, glm::vec3(0.0f), glm::vec3(0, 0, 1));
	m_queue.writeBuffer(
		m_uniformBuffer,
		offsetof(GameObject::MyUniforms, viewMatrix),
		&m_uniforms.viewMatrix,
		sizeof(GameObject::MyUniforms::viewMatrix)
	);

	m_uniforms.cameraWorldPosition = position;
	m_device.getQueue().writeBuffer(
		m_uniformBuffer,
		offsetof(GameObject::MyUniforms, cameraWorldPosition),
		&m_uniforms.cameraWorldPosition,
		sizeof(GameObject::MyUniforms::cameraWorldPosition)
	);
}


void Application::UpdateDragInertia()
{
	constexpr float eps = 1e-4f;
	// Apply inertia only when the user released the click.
	if (!m_drag.active) {
		// Avoid updating the matrix when the velocity is no longer noticeable
		if (std::abs(m_drag.velocity.x) < eps && std::abs(m_drag.velocity.y) < eps) {
			return;
		}
		m_cameraState.angles += m_drag.velocity;
		m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
		// Dampen the velocity so that it decreases exponentially and stops
		// after a few frames.
		m_drag.velocity *= m_drag.intertia;
		UpdateViewMatrix();
	}
}

bool Application::InitGui() {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOther(m_window, true);
	ImGui_ImplWGPU_Init(m_device, 3, m_surfaceFormat, m_depthTextureFormat);
	return true;
}

void Application::TerminateGui() {
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplWGPU_Shutdown();
}

void Application::UpdateGui(RenderPassEncoder renderPass) {
	// Start the Dear ImGui frame
	ImGui_ImplWGPU_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();


	bool changed = false;
	ImGui::Begin("Lighting");
	changed = ImGui::ColorEdit3("Color #0", glm::value_ptr(m_lightingUniforms.colors[0])) || changed;
	changed = ImGui::DragDirection("Direction #0", m_lightingUniforms.directions[0]) || changed;
	changed = ImGui::ColorEdit3("Color #1", glm::value_ptr(m_lightingUniforms.colors[1])) || changed;
	changed = ImGui::DragDirection("Direction #1", m_lightingUniforms.directions[1]) || changed;
	changed = ImGui::SliderFloat("Hardness", &m_lightingUniforms.hardness, 1.0f, 100.0f) || changed;
	changed = ImGui::SliderFloat("K Diffuse", &m_lightingUniforms.kd, 0.0f, 1.0f) || changed;
	changed = ImGui::SliderFloat("K Specular", &m_lightingUniforms.ks, 0.0f, 1.0f) || changed;
	ImGui::End();
	m_lightingUniformsChanged = changed;

	// Draw the UI
	ImGui::EndFrame();
	// Convert the UI defined above into low-level drawing commands
	ImGui::Render();
	// Execute the low-level drawing commands on the WebGPU backend
	ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
}


bool Application::InitLightingUniforms()
{
	// Create uniform buffer
	BufferDescriptor bufferDesc;
	bufferDesc.size = sizeof(GameObject::LightingUniforms);
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	m_lightingUniformBuffer = m_device.createBuffer(bufferDesc);

	// Initial values
	m_lightingUniforms.directions[0] = { 0.5f, -0.9f, 0.1f, 0.0f };
	m_lightingUniforms.directions[1] = { 0.2f, 0.4f, 0.3f, 0.0f };
	m_lightingUniforms.colors[0] = { 1.0f, 0.9f, 0.6f, 1.0f };
	m_lightingUniforms.colors[1] = { 0.6f, 0.9f, 1.0f, 1.0f };

	UpdateLightingUniforms();

	return m_lightingUniformBuffer != nullptr;
}


void Application::TerminateLightingUniforms()
{
	m_lightingUniformBuffer.destroy();
	m_lightingUniformBuffer.release();
}


void Application::UpdateLightingUniforms()
{
	if (m_lightingUniformsChanged) {
		m_queue.writeBuffer(m_lightingUniformBuffer, 0, &m_lightingUniforms, sizeof(GameObject::LightingUniforms));
		m_lightingUniformsChanged = false;
	}
}


TextureView Application::GetNextSurfaceTextureView() {
	// Get the surface texture
	SurfaceTexture surfaceTexture;
	m_surface.getCurrentTexture(&surfaceTexture);
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


RequiredLimits Application::GetRequiredLimits(Adapter adapter) const
{
	// Get adapter supported limits, in case we need them
	SupportedLimits supportedLimits;
	adapter.getLimits(&supportedLimits);

	/*CHECK--> std::cout << "adapter.maxBufferSize: " << supportedLimits.limits.maxBufferSize << std::endl; */


	RequiredLimits requiredLimits = Default;
	requiredLimits.limits.maxVertexAttributes = 6;
	requiredLimits.limits.maxVertexBuffers = 1;
	requiredLimits.limits.maxBufferSize = 150000 * sizeof(VertexAttributes);
	requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.maxInterStageShaderComponents = 17;
	requiredLimits.limits.maxBindGroups = 2;
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 2;
	requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
	// Allow textures up to 2K
	int num = 0;

	GLFWmonitor** some = glfwGetMonitors(&num);

	int xPos, yPos, width, height;
	glfwGetMonitorWorkarea(*some, &xPos, &yPos, &width, &height);

	requiredLimits.limits.maxTextureDimension1D = width;
	requiredLimits.limits.maxTextureDimension2D = width;
	requiredLimits.limits.maxTextureArrayLayers = 1;
	requiredLimits.limits.maxSampledTexturesPerShaderStage = 2;
	requiredLimits.limits.maxSamplersPerShaderStage = 1;

	return requiredLimits;
}
