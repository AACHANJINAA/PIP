#pragma once
#include "Shader.h"

class PlayerShader : public Shader
{
public:
    PlayerShader() = default;
    virtual ~PlayerShader() = default;

    virtual const std::string& pso_name() const override;

protected:
    virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() override;
    virtual D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;
    virtual D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) override;
};
