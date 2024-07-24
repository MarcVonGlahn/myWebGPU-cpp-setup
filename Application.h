#ifndef APPLICATION_H
#define APPLICATION_H

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp> // all types inspired from GLSL
#include <glm/ext.hpp> // --> Warning Level needs to be put on W3, otherwise it gives me error in type_quat.hpp

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <iostream>
#include <cassert>
#include <vector>

#include <array>

#include <cmath>

#include "Loader.h"


using VertexAttributes = Loader::VertexAttributes;
using namespace wgpu;


// We define a function that hides implementation-specific variants of device polling:
inline void wgpuPollEvents([[maybe_unused]] Device device, [[maybe_unused]] bool yieldToWebBrowser) {
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
	void DoTextureCreation();
	void DoSamplerCreation();

	void InitializePipeline();
	void InitializeBuffers();

	void InitializeUniforms();
	void UpdateUniforms();

	void PlayWithBuffers();

	RequiredLimits GetRequiredLimits(Adapter adapter) const;

private:
	struct MyUniforms {
		// We add transform matrices
		glm::mat4x4 projectionMatrix;
		glm::mat4x4 viewMatrix;
		glm::mat4x4 modelMatrix;
		std::array<float, 4> color;
		float time;
		float _pad[3];
	};
	// Have the compiler check byte alignment
	static_assert(sizeof(MyUniforms) % 16 == 0);


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

	Texture m_texture;
	TextureView m_textureView;
	TextureDescriptor m_textureDesc;

	Sampler m_sampler;

	std::vector<VertexAttributes> m_vertexData;

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

#endif // APPLICATION_H