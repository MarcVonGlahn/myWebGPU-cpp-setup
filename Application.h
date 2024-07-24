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
#include "Helper.h"


using VertexAttributes = Loader::VertexAttributes;
using namespace wgpu;

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

	// A function called when the window is resized.
	void OnResize();

private:
	void InitWindow();
	void InitInstanceAndSurface();
	void InitDevice();
	void InitQueue();

	void ConfigureSurface();

	void InitDepthTextureView();
	void InitTexture();
	void InitSampler();

	void InitPipeline();
	void InitBuffers();

	void InitUniforms();
	void UpdateUniforms();

	void UpdateWindowDimensions();

	void UpdateProjectionMatrix();

	TextureView GetNextSurfaceTextureView();
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
	GLFWwindow *m_window;
	Adapter m_adapter;
	Device m_device;
	Queue m_queue;
	Surface m_surface;
	std::unique_ptr<ErrorCallback> uncapturedErrorCallbackHandle;

	RenderPipeline m_pipeline;
	TextureFormat m_surfaceFormat = TextureFormat::Undefined;
	TextureFormat m_depthTextureFormat = TextureFormat::Undefined;

	Texture m_depthTexture;
	TextureView m_depthTextureView;

	Texture m_texture;
	TextureView m_textureView;
	TextureDescriptor m_textureDesc;

	Sampler m_sampler;

	std::vector<VertexAttributes> m_vertexData;

	Buffer m_vertexBuffer;
	Buffer m_uniformBuffer;
	uint32_t m_indexCount;

	BindGroup m_bindGroup;

	MyUniforms m_uniforms;

	WGPUColor m_backgroundScreenColor = { 0.7, 0.7, 0.7, 1.0 };

	glm::vec2 m_windowDimensions = glm::vec2(640.f, 480.f);

	// Object Matrices
	glm::mat4x4 R1;
	glm::mat4x4 T1;
	glm::mat4x4 S;
};

#endif // APPLICATION_H