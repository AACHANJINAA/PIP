#pragma once
#include "ShadowDepthShader.h"

class ShadowDepthSkinnedShader : public ShadowDepthShader
{
public:
    ShadowDepthSkinnedShader() = default;
    virtual ~ShadowDepthSkinnedShader() = default;

    virtual const std::string& pso_name() const override {
        static const std::string name = "csm_depth_skinned";
        return name;
    }

    virtual std::string required_root_signature() const override {
        return "csm_depth_skinned";
    }

protected:
    // POSITION + NORMAL + TEXCOORD + TANGENT + BLENDINDICES + BLENDWEIGHT
    // GltfSkinnedShader의 입력 레이아웃과 완전히 동일해야 함
    virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() override;
    virtual D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;
    virtual D3D12_RASTERIZER_DESC create_rasterizer_state() override;
};