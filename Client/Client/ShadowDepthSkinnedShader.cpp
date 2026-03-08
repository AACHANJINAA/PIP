#include "stdafx.h"
#include "ShadowDepthSkinnedShader.h"

D3D12_INPUT_LAYOUT_DESC ShadowDepthSkinnedShader::create_input_layout()
{
    // GltfSkinnedShader::create_input_layout()과 완전히 동일한 레이아웃
    static const D3D12_INPUT_ELEMENT_DESC inputElements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    return { inputElements, _countof(inputElements) };
}

D3D12_SHADER_BYTECODE ShadowDepthSkinnedShader::create_vertex_shader(ComPtr<ID3DBlob>&
    shader_blob)
{
    return compile_shader_from_file(L"Shadow_Depth_Skinned.hlsl", "VS_ShadowDepthSkinned", "vs_5_1", shader_blob);
}

D3D12_SHADER_BYTECODE
ShadowDepthSkinnedShader::create_geometry_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"Shadow_Depth_Skinned.hlsl", "GS_ShadowDepthSkinned", "gs_5_1", shader_blob);
}