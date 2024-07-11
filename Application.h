#include "webgpu-utils.h"

#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#include <webgpu/wgpu.h>
#endif // WEBGPU_BACKEND_WGPU

#include "GLFW/glfw3.h"
#include <glfw3webgpu.h>

#include <iostream>
#include <cassert>
#include <vector>

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
    // We put here all the variables that are shared between init and main loop
    GLFWwindow* window;
    WGPUDevice device;
    WGPUQueue queue;

    // Surface is the link between the OS window(managed by GLFW) and the WebGPU instance
    WGPUSurface surface;


    WGPUTextureView GetNextSurfaceTextureView();
};