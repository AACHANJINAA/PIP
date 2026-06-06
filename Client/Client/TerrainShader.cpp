#include "stdafx.h"
#include "TerrainShader.h"

#include "ShadowManager.h"
#include "LightManager.h"

const std::string& TerrainShader::pso_name() const
{
    static const std::string name = "terrain";
    return name;
}

D3D12_INPUT_LAYOUT_DESC TerrainShader::create_input_layout()
{
    static const D3D12_INPUT_ELEMENT_DESC d3d_input_element_descs[] =
    {
       {
             "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
         },
         {
             "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
         },
         {
             "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
         },
      {
             "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
         }
    };

    return D3D12_INPUT_LAYOUT_DESC{ d3d_input_element_descs, _countof(d3d_input_element_descs) };
}

D3D12_SHADER_BYTECODE TerrainShader::create_vertex_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"Terrain_Shader.hlsl", "VS_Main", "vs_5_1", shader_blob);
}

D3D12_SHADER_BYTECODE TerrainShader::create_pixel_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"Terrain_Shader.hlsl", "PS_Main", "ps_5_1", shader_blob);
}

std::string TerrainShader::required_root_signature() const
{
    return "terrain";
}

void TerrainShader::update_per_object(ID3D12GraphicsCommandList* commandList, Renderer* renderer, GameObject* gameObject)
{
    // 기본 오브젝트별 업데이트 (Transform, Material 등)
    Shader::update_per_object(commandList, renderer, gameObject);

    // [조명 바인딩 추가] - RootParameter[3]에 b3로 바인딩
    LightManager::instance()->bind(commandList, 3);

    // 그림자 리소스 바인딩 (Terrain 루트 시그니처: Param 6 == b5, Param 7 == t11)
    ShadowManager::instance()->bind_for_lighting(commandList, 6, 7, renderer);
}