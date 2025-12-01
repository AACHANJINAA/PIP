#include "stdafx.h"
#include "TerrainShader.h"

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
             "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,  // ← 12에서 24로 변경!
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