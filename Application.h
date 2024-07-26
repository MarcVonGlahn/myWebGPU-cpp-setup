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

#include "GameObject.h"


// ImGUI
#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>


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

	// CameraControl
	void OnMouseMove(double xpos, double ypos);
	void OnMouseButton(int button, int action, int mods);
	void OnScroll(double xoffset, double yoffset);

private:
	void InitWindow();
	void InitInstanceAndSurface();
	void InitDevice();
	void InitQueue();

	void ConfigureSurface();

	void InitDepthTextureView();
	void InitSampler();

	bool InitGameObjects();

	bool InitPipeline();
	void InitBuffers();

	void InitUniforms();

	void UpdateUniforms();

	void UpdateWindowDimensions();

	void UpdateProjectionMatrix();
	void UpdateViewMatrix();

	void UpdateDragInertia();

	// ImGUI
	bool InitGui();
	void TerminateGui();
	void UpdateGui(wgpu::RenderPassEncoder renderPass);

	// Lighting Uniforms
	bool InitLightingUniforms(); // called in onInit()
	void TerminateLightingUniforms(); // called in onFinish()
	void UpdateLightingUniforms(); // called when GUI is tweaked

	TextureView GetNextSurfaceTextureView();
	RequiredLimits GetRequiredLimits(Adapter adapter) const;

private:
	

	struct CameraState {
		// angles.x is the rotation of the camera around the global vertical axis, affected by mouse.x
		// angles.y is the rotation of the camera around its local horizontal axis, affected by mouse.y
		glm::vec2 angles = { -0.5f, 0.5f };
		// zoom is the position of the camera along its local forward axis, affected by the scroll wheel
		float zoom = -1.2f;
	};

	struct DragState {
		// Whether a drag action is ongoing (i.e., we are between mouse press and mouse release)
		bool active = false;
		// The position of the mouse at the beginning of the drag action
		glm::vec2 startMouse;
		// The camera state at the beginning of the drag action
		CameraState startCameraState;

		// Constant settings
		float sensitivity = 0.01f;
		float scrollSensitivity = 0.1f;

		// Inertia
		glm::vec2 velocity = { 0.0, 0.0 };
		glm::vec2 previousDelta;
		float intertia = 0.9f;
	};


	// We put here all the variables that are shared between init and main loop
	GLFWwindow *m_window;
	Adapter m_adapter;
	Device m_device;
	Queue m_queue;
	Surface m_surface;
	std::unique_ptr<ErrorCallback> uncapturedErrorCallbackHandle;

	BindGroupLayout m_bindGroupLayout = nullptr;

	RenderPipeline m_pipeline;
	TextureFormat m_surfaceFormat = TextureFormat::Undefined;
	TextureFormat m_depthTextureFormat = TextureFormat::Undefined;

	Texture m_depthTexture;
	TextureView m_depthTextureView;

	std::vector<GameObject> m_gameObjects;

	TextureDescriptor m_textureDesc;

	Sampler m_sampler;

	Buffer m_tempBuffer;
	Buffer m_uniformBuffer;
	Buffer m_lightingUniformBuffer = nullptr;
	GameObject::LightingUniforms m_lightingUniforms;

	GameObject::MyUniforms m_uniforms;

	WGPUColor m_backgroundScreenColor = { 0.7, 0.7, 0.7, 1.0 };

	glm::vec2 m_windowDimensions = glm::vec2(1080.f, 720.f);

	CameraState m_cameraState;
	DragState m_drag;

	bool m_lightingUniformsChanged = false;

	// Object Matrices
	glm::mat4x4 R1;
	glm::mat4x4 T1;
	glm::mat4x4 S;
};

#endif // APPLICATION_H