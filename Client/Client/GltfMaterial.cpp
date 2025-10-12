#include "stdafx.h"
#include "GltfMaterial.h"
#include "Shader.h"

GltfMaterial::GltfMaterial(const std::string& name) : _name(name)
{
}

void GltfMaterial::set_shader(std::shared_ptr<Shader> shader)
{
	_shader = shader;
}

void GltfMaterial::set_texture(std::shared_ptr<Texture> texture, UINT index)
{
	if (index >= _textures.size()) {
		_textures.resize(index + 1); // 인덱스에 맞게 벡터 크기 조정
	}
	_textures[index] = texture;
}

void GltfMaterial::bind(ID3D12GraphicsCommandList* command_list) const
{
	if (_textures.empty() || !_textures[0]) return;

	// GltfRootSignatureGenerator에서 4번 파라미터에 넣어줬으니까
	command_list->SetGraphicsRootDescriptorTable(4, _textures[0]->gpuSrvHandle);
}