#include "stdafx.h"
#include "UIShader.h"

const std::string& UIShader::pso_name() const
{
    static const std::string name = "ui";
    return name;
}

std::string UIShader::required_root_signature() const
{
    return "ui";
}

D3D12_INPUT_LAYOUT_DESC UIShader::create_input_layout()
{
    // UI는 Position(float3) + TexCoord(float2)만 필요
    static const D3D12_INPUT_ELEMENT_DESC input_elements[] =
    {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        }
    };

    return D3D12_INPUT_LAYOUT_DESC{ input_elements, _countof(input_elements) };
}

D3D12_SHADER_BYTECODE UIShader::create_vertex_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"UI_Shader.hlsl", "VS_UI", "vs_5_1", shader_blob);
}

D3D12_SHADER_BYTECODE UIShader::create_pixel_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"UI_Shader.hlsl", "PS_UI", "ps_5_1", shader_blob);
}

D3D12_BLEND_DESC UIShader::create_blend_state()
{
    D3D12_BLEND_DESC blend_desc = {};
    blend_desc.AlphaToCoverageEnable = FALSE;
    blend_desc.IndependentBlendEnable = FALSE;

    // 알파 블렌딩 활성화
    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].LogicOpEnable = FALSE;

    // SrcAlpha * Src + (1-SrcAlpha) * Dest
    blend_desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

    blend_desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    return blend_desc;
}

D3D12_DEPTH_STENCIL_DESC UIShader::create_depth_stencil_state()
{
    D3D12_DEPTH_STENCIL_DESC depth_stencil_desc = {};

    // UI는 항상 최상위에 그려지도록 depth test 비활성화
    depth_stencil_desc.DepthEnable = FALSE;
    depth_stencil_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depth_stencil_desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    depth_stencil_desc.StencilEnable = FALSE;

    return depth_stencil_desc;
}

D3D12_RASTERIZER_DESC UIShader::create_rasterizer_state()
{
    D3D12_RASTERIZER_DESC rasterizer_desc = {};
    rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer_desc.CullMode = D3D12_CULL_MODE_NONE; // UI는 양면 렌더링
    rasterizer_desc.FrontCounterClockwise = FALSE;
    rasterizer_desc.DepthBias = 0;
    rasterizer_desc.DepthBiasClamp = 0.0f;
    rasterizer_desc.SlopeScaledDepthBias = 0.0f;
    rasterizer_desc.DepthClipEnable = TRUE;
    rasterizer_desc.MultisampleEnable = FALSE;
    rasterizer_desc.AntialiasedLineEnable = FALSE;
    rasterizer_desc.ForcedSampleCount = 0;
    rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    return rasterizer_desc;
}