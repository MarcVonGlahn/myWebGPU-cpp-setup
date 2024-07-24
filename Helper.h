class Helper {
public:
	// We define a function that hides implementation-specific variants of device polling:
	static void wgpuPollEvents([[maybe_unused]] Device device, [[maybe_unused]] bool yieldToWebBrowser) {
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
};