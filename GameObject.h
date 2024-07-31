#include <iostream>

#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp> // all types inspired from GLSL
#include <glm/ext.hpp> 

#include <array>

#include "Loader.h"


using VertexAttributes = Loader::VertexAttributes;

class GameObject {
public:
	GameObject();
	GameObject(std::shared_ptr<wgpu::Device> device,
		std::string name, std::string path,
		glm::vec3 position,
		std::shared_ptr<wgpu::Buffer> uniformBuffer,
		std::shared_ptr<wgpu::Buffer> lightingBuffer,
		std::shared_ptr<wgpu::Sampler> sampler,
		std::shared_ptr<wgpu::BindGroupLayout> bindGroupLayout);

	// Call after all attributes are set. Calls all init methods.
	void Initialize(int index);

	wgpu::Buffer GetVertexBuffer();

	std::vector<VertexAttributes> GetVertexData();

	wgpu::BindGroup GetBindGroup();

	uint32_t GetIndexCount();



	void SetAlbedoTexture(std::string path);
	void SetNormalTexture(std::string path);

	void Terminate();
private:
	void InitBuffer();
	
	void InitBindGroup();

public:
	// Uniforms for each GameObject. For this app, all objects have the same uniforms. Not ideal, but sufficient for this project.
	struct MyUniforms {
		// We add transform matrices
		glm::mat4x4 projectionMatrix;
		glm::mat4x4 viewMatrix;
		glm::mat4x4 modelMatrix;
		std::array<float, 4> color;
		glm::vec3 cameraWorldPosition;
		float time;
		// float _pad[1];
	};
	// Have the compiler check byte alignment
	static_assert(sizeof(MyUniforms) % 16 == 0);


	// Before Application's private attributes
	struct LightingUniforms {
		std::array<glm::vec4, 2> directions;
		std::array<glm::vec4, 2> colors;

		// Material properties
		float hardness = 32.0f;
		float kd = 1.0f;
		float ks = 0.5f;

		float _pad[1];
	};
	static_assert(sizeof(LightingUniforms) % 16 == 0);

private:

	std::string m_name;
	std::string m_path;

	std::shared_ptr<wgpu::Device> m_device = nullptr;

	int m_bufferIndex = 0;

	wgpu::Buffer m_vertexBuffer;
	std::vector<VertexAttributes> m_vertexData;

	// MyUniforms m_uniforms;
	std::shared_ptr<wgpu::Buffer> m_uniformBuffer;

	LightingUniforms m_lightingUniforms;
	std::shared_ptr<wgpu::Buffer> m_lightingUniformBuffer;

	std::shared_ptr<wgpu::Sampler> m_sampler;

	std::shared_ptr<wgpu::BindGroupLayout> m_bindGroupLayout;

	uint32_t m_indexCount;

	wgpu::BindGroup m_bindGroup;

	wgpu::Texture m_baseColorTexture = nullptr;
	wgpu::TextureView m_baseColorTextureView = nullptr;
	wgpu::Texture m_normalTexture = nullptr;
	wgpu::TextureView m_normalTextureView = nullptr;

	// World Position of the GameObject
	glm::vec3 m_position;
};