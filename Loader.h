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
		
		// Texture mapping attributes represent the local frame in which
		// normals sampled from the normal map are expressed.
		glm::vec3 tangent; // T = local X axis
		glm::vec3 bitangent; // B = local Y axis
		glm::vec3 normal; // N = local Z axis

		glm::vec3 color;
		glm::vec2 uv;
	};

	static bool loadGeometry(const fs::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData, int dimensions);
	static bool loadGeometryFromObj(const fs::path& path, std::vector<VertexAttributes>& thisVertexData);
	static ShaderModule loadShaderModule(const fs::path& path, Device device);
	static Texture loadTexture(const fs::path& path, Device device, TextureView* pTextureView);

	static glm::mat3x3 computeTBN(const VertexAttributes corners[3], const glm::vec3& expectedN);
	
private:
	static uint32_t bit_width(uint32_t m); 

	static void writeMipMaps(
		Device device,
		Texture texture,
		Extent3D textureSize,
		[[maybe_unused]] uint32_t mipLevelCount, // not used yet
		const unsigned char* pixelData);

	static void populateTextureFrameAttributes(std::vector<VertexAttributes>& vertexData);
};

