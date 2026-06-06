#pragma once
#include "Shader.h"

class OcclusionQueryShader : public Shader {
public:
    virtual const std::string& pso_name() const override { static std::string name = "occlusion_query"; return name; }
    virtual std::string required_root_signature() const override { return "occlusion_sig"; }

protected:
    virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() override; // POSITION만 포함
	virtual D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;
	virtual D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) override;
    virtual ComPtr<ID3D12PipelineState> create_pso(ID3D12Device* device, ID3D12RootSignature* root_sig) override;
};