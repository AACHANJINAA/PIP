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
    for (UINT i = 0; i < _textures.size(); ++i)
    {
         if (_textures[i] && _textures[i]->gpu_srv_handle.ptr != 0)
         {
             // 루트 파라미터 4번부터 텍스처 테이블이 시작됩니다.
            command_list->SetGraphicsRootDescriptorTable(4 + i, _textures[i]->gpu_srv_handle);
         }
     }
}