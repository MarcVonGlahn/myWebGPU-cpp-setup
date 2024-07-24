#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp> // all types inspired from GLSL
#include <glm/ext.hpp> // --> Warning Level needs to be put on W3, otherwise it gives me error in type_quat.hpp


using namespace wgpu;
namespace fs = std::filesystem;

#include "tiny_obj_loader.h"

#include "stb_image.h"

class Loader
{
public:
	struct VertexAttributes {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec3 color;
		glm::vec2 uv;
	};

	static bool loadGeometry(const fs::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData, int dimensions);
	static bool loadGeometryFromObj(const fs::path& path, std::vector<VertexAttributes>& thisVertexData);
	static ShaderModule loadShaderModule(const fs::path& path, Device device);
	static Texture loadTexture(const fs::path& path, Device device, TextureView* pTextureView);
	
private:
	static uint32_t bit_width(uint32_t m); 

	static void writeMipMaps(
		Device device,
		Texture texture,
		Extent3D textureSize,
		[[maybe_unused]] uint32_t mipLevelCount, // not used yet
		const unsigned char* pixelData);
};

