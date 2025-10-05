#include "stdafx.h"
#include "PlayerShader.h"
const std::string& PlayerShader::pso_name() const
{
    static const std::string name = "player";
    return name;
}

D3D12_INPUT_LAYOUT_DESC PlayerShader::create_input_layout()
{
    // 기존 CPlayerShader의 Input Layout 정보를 가져옵니다.
    static const D3D12_INPUT_ELEMENT_DESC input_layout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    return { input_layout, _countof(input_layout) };
}

D3D12_SHADER_BYTECODE PlayerShader::create_vertex_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"Shaders.hlsl", "VSLighting", "vs_5_1", shader_blob);
}

D3D12_SHADER_BYTECODE PlayerShader::create_pixel_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"Shaders.hlsl", "PSLighting", "ps_5_1", shader_blob);
}