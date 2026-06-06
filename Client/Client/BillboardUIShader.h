#pragma once
#include "Shader.h"

struct BillboardUIVertex {
    DirectX::XMFLOAT3 pos; // 마커의 월드 좌표 위치
};

class BillboardUIShader : public Shader
{
public:
    BillboardUIShader();
    virtual ~BillboardUIShader();

    virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() override;
    virtual D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& byte_code) override;
    virtual D3D12_SHADER_BYTECODE create_geometry_shader(ComPtr<ID3DBlob>& byte_code) override;
    virtual D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& byte_code) override;

    virtual D3D12_PRIMITIVE_TOPOLOGY_TYPE primitive_topology_type() const override;

    virtual D3D12_BLEND_DESC create_blend_state() override;
    virtual D3D12_DEPTH_STENCIL_DESC create_depth_stencil_state() override;
    virtual D3D12_RASTERIZER_DESC create_rasterizer_state() override;

    // PSO 이름: "billboard_ui"
    virtual const std::string& pso_name() const override;
    virtual std::string required_root_signature() const override;
};
