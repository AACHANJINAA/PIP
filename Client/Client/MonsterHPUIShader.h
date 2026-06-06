#pragma once
#include "Shader.h"
class MonsterHPUIShader : public Shader
{
	public:
	MonsterHPUIShader() = default;
	virtual ~MonsterHPUIShader() = default;

	// Shader을(를) 통해 상속됨
	virtual  ComPtr<ID3D12PipelineState> create_pso(ID3D12Device* device, ID3D12RootSignature* root_signature) override;

	virtual const std::string& pso_name() const override;
	virtual std::string required_root_signature() const override;
	virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() override;
	virtual D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;
	virtual D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) override;
	virtual D3D12_SHADER_BYTECODE create_geometry_shader(ComPtr<ID3DBlob>& shader_blob);
	virtual D3D12_BLEND_DESC create_blend_state() override;
	virtual D3D12_RASTERIZER_DESC create_rasterizer_state() override;
	virtual D3D12_DEPTH_STENCIL_DESC create_depth_stencil_state() override;
};

