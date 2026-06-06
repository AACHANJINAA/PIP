#pragma once
#include "Shader.h"

class UIShader : public Shader
{
public:
    UIShader() = default;
    virtual ~UIShader() = default;

    // 셰이더 이름
    virtual const std::string& pso_name() const override;

    // 필요한 루트 시그니처
    virtual std::string required_root_signature() const override;

protected:
    // PSO 생성을 위한 필수 함수들
    virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() override;
    virtual D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;
    virtual D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) override;

    // UI 전용 렌더 스테이트
    virtual D3D12_BLEND_DESC create_blend_state() override;
    virtual D3D12_DEPTH_STENCIL_DESC create_depth_stencil_state() override;
    virtual D3D12_RASTERIZER_DESC create_rasterizer_state() override;
};