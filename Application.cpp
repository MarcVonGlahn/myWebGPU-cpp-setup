#include "Application.h"


constexpr float PI = 3.14159265358979323846f;

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

	m_texture.destroy();
	m_texture.release();

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
	renderPass.setVertexBuffer(0, vertexBuffer, 0, m_vertexData.size() * sizeof(VertexAttributes));
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

void Application::DoTextureCreation()
{
	m_textureView = nullptr;
	m_texture = Loader::loadTexture(RESOURCE_DIR "/texture_flatspot.png", device, &m_textureView);
	if (!m_texture) {
		std::cerr << "Could not load texture!" << std::endl;
	}

	// [...] setup descriptor
	/*m_textureDesc.dimension = TextureDimension::_2D;
	m_textureDesc.size = { 256, 256, 1 };
	                             ^ ignored because it is a 2D texture

	m_textureDesc.mipLevelCount = 8;
	m_textureDesc.sampleCount = 1;

	m_textureDesc.format = TextureFormat::RGBA8Unorm;

	m_textureDesc.usage = TextureUsage::TextureBinding | TextureUsage::CopyDst;

	m_textureDesc.viewFormatCount = 0;
	m_textureDesc.viewFormats = nullptr;*/

	//m_texture = device.createTexture(m_textureDesc);

	//TextureViewDescriptor textureViewDesc;
	//textureViewDesc.aspect = TextureAspect::All;
	//textureViewDesc.baseArrayLayer = 0;
	//textureViewDesc.arrayLayerCount = 1;
	//textureViewDesc.baseMipLevel = 0;
	//textureViewDesc.mipLevelCount = m_textureDesc.mipLevelCount;
	//textureViewDesc.dimension = TextureViewDimension::_2D;
	//textureViewDesc.format = m_textureDesc.format;
	//m_textureView = m_texture.createView(textureViewDesc);


	//// Create image data
	//std::vector<uint8_t> pixels(4 * m_textureDesc.size.width * m_textureDesc.size.height);
	//for (uint32_t i = 0; i < m_textureDesc.size.width; ++i) {
	//	for (uint32_t j = 0; j < m_textureDesc.size.height; ++j) {
	//		uint8_t* p = &pixels[4 * (j * m_textureDesc.size.width + i)];
	//		p[0] = (i / 16) % 2 == (j / 16) % 2 ? 255 : 0; // r
	//		p[1] = ((i - j) / 16) % 2 == 0 ? 255 : 0; // g
	//		p[2] = ((i + j) / 16) % 2 == 0 ? 255 : 0; // b
	//		p[3] = 255; // a
	//	}
	//}

	//// Arguments telling which part of the texture we upload to
	//// (together with the last argument of writeTexture)
	//ImageCopyTexture destination;
	//destination.texture = m_texture;
	//destination.mipLevel = 0;
	//destination.origin = { 0, 0, 0 }; // equivalent of the offset argument of Queue::writeBuffer
	//destination.aspect = TextureAspect::All; // only relevant for depth/Stencil textures

	//// Arguments telling how the C++ side pixel memory is laid out
	//TextureDataLayout source;
	//source.offset = 0;
	//source.bytesPerRow = 4 * m_textureDesc.size.width;
	//source.rowsPerImage = m_textureDesc.size.height;

	/*queue.writeTexture(destination, pixels.data(), pixels.size(), source, m_textureDesc.size);*/
}

void Application::DoSamplerCreation()
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
	m_sampler = device.createSampler(samplerDesc);



	//// Create and upload texture data, one mip level at a time
	//ImageCopyTexture destination;
	//destination.texture = m_texture;
	//destination.origin = { 0, 0, 0 };
	//destination.aspect = TextureAspect::All;

	//TextureDataLayout source;
	//source.offset = 0;

	//Extent3D mipLevelSize = m_textureDesc.size;
	//std::vector<uint8_t> previousLevelPixels;
	//for (uint32_t level = 0; level < m_textureDesc.mipLevelCount; ++level) {
	//	// Create image data
	//	std::vector<uint8_t> pixels(4 * mipLevelSize.width * mipLevelSize.height);
	//	for (uint32_t i = 0; i < mipLevelSize.width; ++i) {
	//		for (uint32_t j = 0; j < mipLevelSize.height; ++j) {
	//			uint8_t* p = &pixels[4 * (j * mipLevelSize.width + i)];
	//			if (level == 0) {
	//				p[0] = (i / 16) % 2 == (j / 16) % 2 ? 255 : 0; // r
	//				p[1] = ((i - j) / 16) % 2 == 0 ? 255 : 0; // g
	//				p[2] = ((i + j) / 16) % 2 == 0 ? 255 : 0; // b
	//			}
	//			else {
	//				// Get the corresponding 4 pixels from the previous level
	//				uint8_t* p00 = &previousLevelPixels[4 * ((2 * j + 0) * (2 * mipLevelSize.width) + (2 * i + 0))];
	//				uint8_t* p01 = &previousLevelPixels[4 * ((2 * j + 0) * (2 * mipLevelSize.width) + (2 * i + 1))];
	//				uint8_t* p10 = &previousLevelPixels[4 * ((2 * j + 1) * (2 * mipLevelSize.width) + (2 * i + 0))];
	//				uint8_t* p11 = &previousLevelPixels[4 * ((2 * j + 1) * (2 * mipLevelSize.width) + (2 * i + 1))];
	//				// Average
	//				p[0] = (p00[0] + p01[0] + p10[0] + p11[0]) / 4;
	//				p[1] = (p00[1] + p01[1] + p10[1] + p11[1]) / 4;
	//				p[2] = (p00[2] + p01[2] + p10[2] + p11[2]) / 4;
	//			}
	//			p[3] = 255; // a
	//		}
	//	}

	//	// Change this to the current level
	//	destination.mipLevel = level;

	//	// Compute from the mip level size
	//	source.bytesPerRow = 4 * mipLevelSize.width;
	//	source.rowsPerImage = mipLevelSize.height;

	//	queue.writeTexture(destination, pixels.data(), pixels.size(), source, mipLevelSize);

	//	// The size of the next mip level:
	//	// (see https://www.w3.org/TR/webgpu/#logical-miplevel-specific-texture-extent)
	//	mipLevelSize.width /= 2;
	//	mipLevelSize.height /= 2;
	//	previousLevelPixels = std::move(pixels);
	//}
}


void Application::InitializePipeline()
{
	std::cout << "Creating shader module..." << std::endl;
	ShaderModule shaderModule = Loader::loadShaderModule(RESOURCE_DIR "/shader.wgsl", device);
	std::cout << "Shader module: " << shaderModule << std::endl;

	RenderPipelineDescriptor pipelineDesc;

	// [...] Describe vertex pipeline state
	// Configure 'pipelineDesc.vertex'
	// [...] Describe vertex buffers
	// Vertex fetch
	VertexBufferLayout vertexBufferLayout;
	// [...] Describe the vertex buffer layout
	// We now have 2 attributes
	std::vector<VertexAttribute> vertexAttribs(4);

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
	// Create binding layouts
	// Since we now have 2 bindings, we use a vector to store them
	std::vector<BindGroupLayoutEntry> bindingLayoutEntries(3, Default);
	//                                                     ^ This was a 2

	// The uniform buffer binding that we already had
	BindGroupLayoutEntry& bindingLayout = bindingLayoutEntries[0];
	bindingLayout.binding = 0;
	bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
	bindingLayout.buffer.type = BufferBindingType::Uniform;
	bindingLayout.buffer.minBindingSize = sizeof(MyUniforms);

	// The texture binding
	BindGroupLayoutEntry& textureBindingLayout = bindingLayoutEntries[1];
	// Setup texture binding
	textureBindingLayout.binding = 1;
	textureBindingLayout.visibility = ShaderStage::Fragment;
	textureBindingLayout.texture.sampleType = TextureSampleType::Float;
	textureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;

	// The texture sampler binding
	BindGroupLayoutEntry& samplerBindingLayout = bindingLayoutEntries[2];
	samplerBindingLayout.binding = 2;
	samplerBindingLayout.visibility = ShaderStage::Fragment;
	samplerBindingLayout.sampler.type = SamplerBindingType::Filtering;


	// Create a bind group layout
	BindGroupLayoutDescriptor bindGroupLayoutDesc{};
	bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
	bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
	BindGroupLayout bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutDesc);



	// Create the pipeline layout
	PipelineLayoutDescriptor layoutDesc;
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
	PipelineLayout layout = device.createPipelineLayout(layoutDesc);

	// Assign the PipelineLayout to the RenderPipelineDescriptor's layout field
	pipelineDesc.layout = layout;

	SetupDepthTextureView();

	DoTextureCreation();

	DoSamplerCreation();

	// Create a binding
	std::vector<BindGroupEntry> bindings(3);
	//                                   ^ This was a 2

	bindings[0].binding = 0;
	bindings[0].buffer = uniformBuffer;
	bindings[0].offset = 0;
	bindings[0].size = sizeof(MyUniforms);

	bindings[1].binding = 1;
	bindings[1].textureView = m_textureView;

	bindings[2].binding = 2;
	bindings[2].sampler = m_sampler;

	BindGroupDescriptor bindGroupDesc;
	bindGroupDesc.layout = bindGroupLayout;
	bindGroupDesc.entryCount = (uint32_t)bindings.size();
	bindGroupDesc.entries = bindings.data();
	m_bindGroup = device.createBindGroup(bindGroupDesc);

	pipeline = device.createRenderPipeline(pipelineDesc);

	// We no longer need to access the shader module
	shaderModule.release();
}

void Application::InitializeBuffers()
{
	// Load mesh data from OBJ file
	bool success = Loader::loadGeometryFromObj(RESOURCE_DIR "/flatspot_car_2.obj", m_vertexData);
	if (!success) {
		std::cerr << "Could not load geometry!" << std::endl;
		return;
	}

	// Create vertex buffer
	BufferDescriptor bufferDesc;
	bufferDesc.size = m_vertexData.size() * sizeof(VertexAttributes); // changed
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
	bufferDesc.mappedAtCreation = false;
	vertexBuffer = device.createBuffer(bufferDesc);
	queue.writeBuffer(vertexBuffer, 0, m_vertexData.data(), bufferDesc.size); // changed

	indexCount = static_cast<int>(m_vertexData.size()); // changed

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

	// Matrices
	uniforms.modelMatrix = glm::mat4x4(1.0);
	uniforms.viewMatrix = glm::lookAt(glm::vec3(-1.0f, -2.0f, 1.0f), glm::vec3(0.0f), glm::vec3(0, 0, 1));
	uniforms.projectionMatrix = glm::perspective(45 * PI / 180, 640.0f / 480.0f, 0.01f, 100.0f);

	uniforms.color = { 0.0f, 0.0f, 0.0f, 1.0f };
	uniforms.time = 1.0f;
}



void Application::UpdateUniforms()
{
	uniforms.time = static_cast<float>(glfwGetTime());
	// uniforms.color = { 5.0f * cos(uniforms.time), sin(uniforms.time), -sin(uniforms.time), 1.0f};

	// Upload only the time, whichever its order in the struct
	queue.writeBuffer(uniformBuffer, offsetof(MyUniforms, time), &uniforms.time, sizeof(MyUniforms::time));

	// In the main loop
	/*float viewZ = glm::mix(0.0f, 0.25f, cos(2 * PI * uniforms.time / 4) * 0.5 + 0.5);
	uniforms.viewMatrix = glm::lookAt(glm::vec3(-0.5f, -1.5f, viewZ + 0.25f), glm::vec3(0.0f), glm::vec3(0, 0, 1));
	queue.writeBuffer(uniformBuffer, offsetof(MyUniforms, viewMatrix), &uniforms.viewMatrix, sizeof(MyUniforms::viewMatrix));*/
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

	/*CHECK--> std::cout << "adapter.maxBufferSize: " << supportedLimits.limits.maxBufferSize << std::endl; */


	RequiredLimits requiredLimits = Default;
	requiredLimits.limits.maxVertexAttributes = 4;
	requiredLimits.limits.maxVertexBuffers = 1;
	requiredLimits.limits.maxBufferSize = 150000 * sizeof(VertexAttributes);
	requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.maxInterStageShaderComponents = 8;
	requiredLimits.limits.maxBindGroups = 1;
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
	requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
	// Allow textures up to 2K
	requiredLimits.limits.maxTextureDimension1D = 2048;
	requiredLimits.limits.maxTextureDimension2D = 2048;
	requiredLimits.limits.maxTextureArrayLayers = 1;
	requiredLimits.limits.maxSampledTexturesPerShaderStage = 1;
	requiredLimits.limits.maxSamplersPerShaderStage = 1;

	return requiredLimits;
}
