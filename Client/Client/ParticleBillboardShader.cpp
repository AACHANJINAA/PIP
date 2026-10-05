#include "stdafx.h"
#include "ParticleBillboardShader.h"

D3D12_BLEND_DESC ParticleBillboardShader::create_blend_state()
{
    D3D12_BLEND_DESC desc = {};
    auto& rt = desc.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend = _additive ? D3D12_BLEND_ONE : D3D12_BLEND_INV_SRC_ALPHA; // 가산: 겹칠수록 밝아짐
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ZERO;
    rt.DestBlendAlpha = D3D12_BLEND_ONE;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    return desc;
}

D3D12_DEPTH_STENCIL_DESC ParticleBillboardShader::create_depth_stencil_state()
{
    D3D12_DEPTH_STENCIL_DESC desc = {};
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 파티클끼리 가리지 않게
    desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    return desc;
}

D3D12_RASTERIZER_DESC ParticleBillboardShader::create_rasterizer_state()
{
    D3D12_RASTERIZER_DESC desc = Shader::create_rasterizer_state();
    desc.CullMode = D3D12_CULL_MODE_NONE; // 늘린 사각형은 뒤집힐 수 있음
    return desc;
}

D3D12_SHADER_BYTECODE ParticleBillboardShader::create_vertex_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"Particle_Billboard.hlsl", "VS_Billboard", "vs_5_1", shader_blob);
}

D3D12_SHADER_BYTECODE ParticleBillboardShader::create_pixel_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"Particle_Billboard.hlsl", "PS_Billboard", "ps_5_1", shader_blob);
}
