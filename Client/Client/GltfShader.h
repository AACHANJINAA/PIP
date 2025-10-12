#pragma once
#include "Shader.h"
class GltfShader : public Shader
{
public:
	GltfShader() = default;
	virtual ~GltfShader() = default;

	// Shader을(를) 통해 상속됨
	const std::string& pso_name() const override;
	D3D12_INPUT_LAYOUT_DESC create_input_layout() override;
	D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;
	D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) override;

	virtual std::string required_root_signature() const override;
};

