#include "stdafx.h"
#include "BillboardUIShader.h"

BillboardUIShader::BillboardUIShader()
{
}

BillboardUIShader::~BillboardUIShader()
{
}

const std::string& BillboardUIShader::pso_name() const
{
    static const std::string name = "billboard_ui";
    return name;
}

std::string BillboardUIShader::required_root_signature() const
{
    return "billboard_ui";
}

D3D12_INPUT_LAYOUT_DESC BillboardUIShader::create_input_layout()
{
    static D3D12_INPUT_ELEMENT_DESC elements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    return { elements, _countof(elements) };
}

D3D12_SHADER_BYTECODE BillboardUIShader::create_vertex_shader(ComPtr<ID3DBlob>& byte_code)
{
    return compile_shader_from_file(L"BillboardUI_Shader.hlsl", "VS", "vs_5_1", byte_code);
}

D3D12_SHADER_BYTECODE BillboardUIShader::create_geometry_shader(ComPtr<ID3DBlob>& byte_code)
{
    return compile_shader_from_file(L"BillboardUI_Shader.hlsl", "GS", "gs_5_1", byte_code);
}

D3D12_SHADER_BYTECODE BillboardUIShader::create_pixel_shader(ComPtr<ID3DBlob>& byte_code)
{
    return compile_shader_from_file(L"BillboardUI_Shader.hlsl", "PS", "ps_5_1", byte_code);
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE BillboardUIShader::primitive_topology_type() const
{
    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
}

D3D12_BLEND_DESC BillboardUIShader::create_blend_state()
{
    D3D12_BLEND_DESC desc = Shader::create_blend_state();
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    return desc;
}

D3D12_DEPTH_STENCIL_DESC BillboardUIShader::create_depth_stencil_state()
{
    D3D12_DEPTH_STENCIL_DESC desc = Shader::create_depth_stencil_state();
    // 벽을 뚫고 렌더링되도록 깊이 테스트 비활성화
    desc.DepthEnable = FALSE;
    desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    return desc;
}

D3D12_RASTERIZER_DESC BillboardUIShader::create_rasterizer_state()
{
    D3D12_RASTERIZER_DESC desc = Shader::create_rasterizer_state();
    desc.CullMode = D3D12_CULL_MODE_NONE; // 빌보드가 양면에서 보이도록 컬링 비활성화
    return desc;
}



