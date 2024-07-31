#include "GameObject.h"

// Commented to avoid warning when building for emscripten
// constexpr float PI = 3.14159265358979323846f;

GameObject::GameObject()
{
	
}

GameObject::GameObject(std::shared_ptr<wgpu::Device> device,
	std::string name,
	std::string path,
	glm::vec3 position,
	std::shared_ptr<wgpu::Buffer> uniformBuffer,
	std::shared_ptr<wgpu::Buffer> lightingBuffer,
	std::shared_ptr<wgpu::Sampler> sampler,
	std::shared_ptr<wgpu::BindGroupLayout> bindGroupLayout)
{
	m_device = device;

	m_name = name;
	m_path = path;

	bool success = Loader::loadGeometryFromObj(m_path, m_vertexData);
	if (!success) {
		std::cerr << "Could not load geometry!" << std::endl;
		return;
	}

	m_position = position;

	m_uniformBuffer = uniformBuffer;
	m_lightingUniformBuffer = lightingBuffer;
	m_sampler = sampler;
	m_bindGroupLayout = bindGroupLayout;
}


void GameObject::Initialize(int index)
{
	m_bufferIndex = index;

	InitBuffer();
	InitBindGroup();
}

wgpu::Buffer GameObject::GetVertexBuffer()
{
	if (m_vertexBuffer == nullptr)
	{
		std::cout << "No Vertex Buffer assigned for this GameObject: " << m_name << std::endl;
		return nullptr;
	}

	return m_vertexBuffer;
}

std::vector<VertexAttributes> GameObject::GetVertexData()
{
	return m_vertexData;
}

wgpu::BindGroup GameObject::GetBindGroup()
{
	return m_bindGroup;
}

uint32_t GameObject::GetIndexCount()
{
	return m_indexCount;
}


void GameObject::InitBuffer()
{
	// Create vertex buffer
	BufferDescriptor bufferDesc;
	bufferDesc.label = m_name.c_str();
	bufferDesc.size = m_vertexData.size() * sizeof(VertexAttributes); // changed
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
	bufferDesc.mappedAtCreation = false;
	m_vertexBuffer = m_device->createBuffer(bufferDesc);
	m_device->getQueue().writeBuffer(m_vertexBuffer, 0, m_vertexData.data(), bufferDesc.size); // changed

	m_indexCount = static_cast<int>(m_vertexData.size()); // changed
}


void GameObject::SetAlbedoTexture(std::string path)
{
	m_baseColorTextureView = nullptr;
	m_baseColorTexture = Loader::loadTexture(path, *m_device, &m_baseColorTextureView);
	
	if (!m_baseColorTexture) {
		std::cerr << "Could not load baseColor texture!" << std::endl;
	}
}

void GameObject::SetNormalTexture(std::string path)
{
	m_normalTextureView = nullptr;
	m_normalTexture = Loader::loadTexture(path, *m_device, &m_normalTextureView);

	if (!m_normalTexture) {
		std::cerr << "Could not load normal texture!" << std::endl;
	}
}

void GameObject::Terminate()
{
	m_baseColorTexture.destroy();
	m_baseColorTexture.release();
	m_normalTexture.destroy();
	m_normalTexture.release();
}


void GameObject::InitBindGroup()
{
	// Create a binding
	std::vector<BindGroupEntry> bindings(5);
	//                                   ^ This was a 4

	bindings[0].binding = 0;
	bindings[0].buffer = *m_uniformBuffer;
	bindings[0].offset = 0;
	bindings[0].size = sizeof(MyUniforms);

	bindings[1].binding = 1;
	bindings[1].textureView = m_baseColorTextureView;

	bindings[2].binding = 2;
	bindings[2].textureView = m_normalTextureView;

	bindings[3].binding = 3;
	bindings[3].sampler = *m_sampler;

	bindings[4].binding = 4;
	bindings[4].buffer = *m_lightingUniformBuffer;
	bindings[4].offset = 0;
	bindings[4].size = sizeof(LightingUniforms);

	BindGroupDescriptor bindGroupDesc;
	bindGroupDesc.layout = *m_bindGroupLayout;
	bindGroupDesc.entryCount = (uint32_t)bindings.size();
	bindGroupDesc.entries = bindings.data();
	m_bindGroup = m_device.get()->createBindGroup(bindGroupDesc);
}
