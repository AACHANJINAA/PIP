#include "stdafx.h"
#include "ShadowDepthShader.h"

D3D12_INPUT_LAYOUT_DESC ShadowDepthShader::create_input_layout()
{
    static const D3D12_INPUT_ELEMENT_DESC inputElements[] = 
    {
        { 
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 
        }
    };
    return { inputElements, _countof(inputElements) };
}

D3D12_SHADER_BYTECODE ShadowDepthShader::create_vertex_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"Shadow_Depth.hlsl", "VS_ShadowDepth", "vs_5_1", shader_blob);
}

D3D12_SHADER_BYTECODE ShadowDepthShader::create_pixel_shader(ComPtr<ID3DBlob>& shader_blob)
{
    // Depth-only 패스는 픽셀 셰이더가 필요 없습니다.
    return { nullptr, 0 };
}

D3D12_SHADER_BYTECODE ShadowDepthShader::create_geometry_shader(ComPtr<ID3DBlob>& shader_blob)
{
	return compile_shader_from_file(L"Shadow_Depth.hlsl", "GS_ShadowDepth", "gs_5_1", shader_blob);
}

D3D12_RASTERIZER_DESC ShadowDepthShader::create_rasterizer_state()
{
    D3D12_RASTERIZER_DESC desc = Shader::create_rasterizer_state();
    // 그림자 여드름(Shadow Acne) 방지를 위한 Depth Bias 설정 (필요 시 나중에 튜닝)
    desc.CullMode = D3D12_CULL_MODE_FRONT;
    desc.DepthBias = 0;
    desc.DepthBiasClamp = 0.0f;
    desc.SlopeScaledDepthBias = 0.0f;
    return desc;
}

D3D12_DEPTH_STENCIL_DESC ShadowDepthShader::create_depth_stencil_state()
{
    D3D12_DEPTH_STENCIL_DESC desc = Shader::create_depth_stencil_state();
    desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    return desc;
}