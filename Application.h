#ifndef APPLICATION_H
#define APPLICATION_H

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>


// Include the C++ wrapper instead of the raw header(s)
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>


#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp> // all types inspired from GLSL
#include <glm/ext.hpp> // --> Can't use this, give me error in type_quat.hpp


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

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;


// Avoid the "wgpu::" prefix in front of all WebGPU symbols
using namespace wgpu;

constexpr float PI = 3.14159265358979323846f;

/**
 * The same structure as in the shader, replicated in C++
 */
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

/**
 * A structure that describes the data layout in the vertex buffer
 * We do not instantiate it but use it in `sizeof` and `offsetof`
 */
struct VertexAttributes {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 uv;
};




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

	Sampler m_sampler;

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

#endif // APPLICATION_H