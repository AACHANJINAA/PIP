#include "stdafx.h"
#include "GltfMaterial.h"
#include "Shader.h"
#include "Texture.h"
#include "TextureManager.h"

GltfMaterial::GltfMaterial(const std::string& name) : _name(name)
{
    // 텍스쳐 슬롯은 예약 가능
	_textures.resize(4); // 텍스쳐 슬롯 4개 예약
}

void GltfMaterial::set_shader(std::shared_ptr<Shader> shader)
{
	_shader = shader;
}

std::shared_ptr<Shader> GltfMaterial::shader() const
{
    return _shader;
}

void GltfMaterial::set_texture(std::shared_ptr<Texture> texture, UINT index)
{
	if (index >= _textures.size()) _textures.resize(index + 1);
	_textures[index] = texture;
}


void GltfMaterial::bind(ID3D12GraphicsCommandList* command_list) const
{
    // 첫 번째 유효한 텍스쳐의 GPU 핸들을 찾아서 바인딩
    // Gltf 셰이더는 여러 텍스쳐를 DescriptorTable로 한 번에 받습니다.
    // 테이블의 시작 주소만 넘겨줘랑

	D3D12_GPU_DESCRIPTOR_HANDLE first_gpu_handle = {};
	bool found_first = false;

    for (const auto& tex : _textures)
    {
        if (tex)
        {
			first_gpu_handle = tex->gpu_srv_handle;
            found_first = true;
            break;
        }
	}

    if (found_first)
    {
        command_list->SetGraphicsRootDescriptorTable(4, first_gpu_handle);
	}
}