#pragma once
#include "Shader.h"

class SkyboxShader : public Shader
{
public:
	SkyboxShader() = default;
	virtual ~SkyboxShader() = default;

	// 이 pso의 이름은 skybox

	virtual const std::string& pso_name() const override;
	virtual std::string required_root_signature() const override { return "skybox"; }

protected:
	virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() override;
	virtual D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;
	virtual D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) override;

	virtual D3D12_RASTERIZER_DESC create_rasterizer_state() override;
	virtual D3D12_DEPTH_STENCIL_DESC create_depth_stencil_state() override;
};