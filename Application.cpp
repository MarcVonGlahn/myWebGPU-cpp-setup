#include "Application.h"

bool Application::Initialize() {
    // Move the whole initialization here
    // [...] Initialize
	// Move the whole initialization here
	// Open window
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // <-- extra info for glfwCreateWindow
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);

	// Create instance
	WGPUInstance instance = wgpuCreateInstance(nullptr);

	// Get adapter
	std::cout << "Requesting adapter..." << std::endl;
	surface = glfwGetWGPUSurface(instance, window);

	WGPURequestAdapterOptions adapterOpts = {};
	adapterOpts.nextInChain = nullptr;
	adapterOpts.compatibleSurface = surface;
	//                              ^^^^^^^ Use the surface here

	WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;

	// We no longer need to access the instance
	wgpuInstanceRelease(instance);

	// Get device
	std::cout << "Requesting device..." << std::endl;
	WGPUDeviceDescriptor deviceDesc = {};
	deviceDesc.nextInChain = nullptr;
	deviceDesc.label = "My Device"; // anything works here, that's your call
	deviceDesc.requiredFeatureCount = 0; // we do not require any specific feature
	deviceDesc.requiredLimits = nullptr; // we do not require any specific limit
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "The default queue";
	// A function that is invoked whenever the device stops being available.
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* /* pUserData */) {
		std::cout << "Device lost: reason " << reason;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
		};
	device = requestDeviceSync(adapter, &deviceDesc);
	std::cout << "Got device: " << device << std::endl;

	// We no longer need to access the adapter
	wgpuAdapterRelease(adapter);

	// Device error callback
	auto onDeviceError = [](WGPUErrorType type, char const* message, void* /* pUserData */) {
		std::cout << "Uncaptured device error: type " << type;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
		};
	wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr /* pUserData */);

	queue = wgpuDeviceGetQueue(device);
	
	// Surface configuration
	WGPUSurfaceConfiguration config = {};
	config.nextInChain = nullptr;

	// [...] Describe Surface Configuration
	// Configuration of the textures created for the underlying swap chain
	config.width = 640;
	config.height = 480;

	// [...] Describe Surface Usage
	config.usage = WGPUTextureUsage_RenderAttachment;

	// [...] Describe Surface Format
	WGPUTextureFormat surfaceFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);
	config.format = surfaceFormat;

	// And we do not need any particular view format:
	config.viewFormatCount = 0;
	config.viewFormats = nullptr;

	config.device = device;

	config.presentMode = WGPUPresentMode_Fifo;

	config.alphaMode = WGPUCompositeAlphaMode_Auto;

	wgpuSurfaceConfigure(surface, &config);

	return true;
}

void Application::Terminate() {
	// Move all the release/destroy/terminate calls here
	wgpuQueueRelease(queue);
	wgpuSurfaceRelease(surface);
	wgpuDeviceRelease(device);
	wgpuSurfaceUnconfigure(surface);
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Application::MainLoop() {
	glfwPollEvents();

	// Also move here the tick/poll but NOT the emscripten sleep
#if defined(WEBGPU_BACKEND_DAWN)
	wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
	wgpuDevicePoll(device, false, nullptr);
#endif


	// In Application::MainLoop()
	// [...] Get the next target texture view
	// Get the next target texture view
	WGPUTextureView targetView = GetNextSurfaceTextureView();
	if (!targetView) return;

	// [...] Draw things
	// [...] Create Command Encoder
	WGPUCommandEncoderDescriptor encoderDesc = {};
	encoderDesc.nextInChain = nullptr;
	encoderDesc.label = "My command encoder";
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);
	
	// [...] Encode Render Pass
	WGPURenderPassDescriptor renderPassDesc = {};
	renderPassDesc.nextInChain = nullptr;

	// [...] Describe Render Pass
	WGPURenderPassColorAttachment renderPassColorAttachment = {};
	renderPassDesc.depthStencilAttachment = nullptr;
	renderPassDesc.timestampWrites = nullptr;

	// [...] Describe the attachment
	renderPassColorAttachment.view = targetView;
	renderPassColorAttachment.resolveTarget = nullptr;

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;

	renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
	renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
	renderPassColorAttachment.clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 };

#ifndef WEBGPU_BACKEND_WGPU
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU


	WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
	// [...] Use Render Pass
	wgpuRenderPassEncoderEnd(renderPass);
	wgpuRenderPassEncoderRelease(renderPass);
	// [...] Finish encoding and submit

	WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.nextInChain = nullptr;
	cmdBufferDescriptor.label = "Command buffer";
	WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
	wgpuCommandEncoderRelease(encoder);

	std::cout << "Submitting command..." << std::endl;
	wgpuQueueSubmit(queue, 1, &command);
	wgpuCommandBufferRelease(command);
	std::cout << "Command submitted." << std::endl;




	// [...] Present the surface onto the window
	
	// At the end of the frame
	wgpuTextureViewRelease(targetView);

	wgpuSurfacePresent(surface);

}

bool Application::IsRunning() {
    return !glfwWindowShouldClose(window);
}

WGPUTextureView Application::GetNextSurfaceTextureView()
{
	// [...] Get the next surface texture
	WGPUSurfaceTexture surfaceTexture;
	wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);

	if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
		return nullptr;
	}

    // [...] Create surface texture view
	WGPUTextureViewDescriptor viewDescriptor;
	viewDescriptor.nextInChain = nullptr;
	viewDescriptor.label = "Surface texture view";
	viewDescriptor.format = wgpuTextureGetFormat(surfaceTexture.texture);
	viewDescriptor.dimension = WGPUTextureViewDimension_2D;
	viewDescriptor.baseMipLevel = 0;
	viewDescriptor.mipLevelCount = 1;
	viewDescriptor.baseArrayLayer = 0;
	viewDescriptor.arrayLayerCount = 1;
	viewDescriptor.aspect = WGPUTextureAspect_All;
	WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDescriptor);

	return targetView;
}
