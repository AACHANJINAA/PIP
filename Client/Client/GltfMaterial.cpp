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
	if (_textures.empty()) return;

	D3D12_GPU_DESCRIPTOR_HANDLE firstGpuHandle = {};
    bool bFoundFirst = false;
    for (int i = 0; i < 4; ++i) // 텍스처 슬롯은 4개라고 가정
    { 
        if (_textures[i] && _textures.at(i)) // 맵에 키가 존재하고, 텍스처가 유효한지 확인
        {
            firstGpuHandle = _textures.at(i)->gpuSrvHandle;
            bFoundFirst = true;
            break;
        }
    }

	// GltfRootSignatureGenerator에서 4번 파라미터에 넣어줬으니까
    if (bFoundFirst)
    {
        // GltfRootSignatureGenerator에서 4번 파라미터에 테이블을 설정했으므로 인덱스 4를 사용
        command_list->SetGraphicsRootDescriptorTable(4, firstGpuHandle);
    }
}