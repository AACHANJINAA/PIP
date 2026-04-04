#pragma once
#include "Shader.h"

class ShadowDepthShader : public Shader
{
public:
    ShadowDepthShader() = default;
    virtual ~ShadowDepthShader() = default;

    virtual const std::string& pso_name() const override {
        static const std::string name = "csm_depth";
        return name;
    }

    virtual std::string required_root_signature() const override {
        return "csm_depth";
    }

protected:
    virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() override;
    virtual D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;
    virtual D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) override;

    // Depth-only이므로 Rasterizer 및 DepthStencil 설정 변경
    virtual D3D12_RASTERIZER_DESC create_rasterizer_state() override;
    virtual D3D12_DEPTH_STENCIL_DESC create_depth_stencil_state() override;

    virtual DXGI_FORMAT get_dsv_format() const override {
        return DXGI_FORMAT_D32_FLOAT;
    }
    virtual UINT get_num_render_targets() const override { return 0; }
    // 뎁스 전용은 RTV 0개
};