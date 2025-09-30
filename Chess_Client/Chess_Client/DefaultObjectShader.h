#pragma once
#include "Shader.h"

// [역할] 일반적인 3D 오브젝트를 그리기 위한 모든 정보를 제공하는 셰이더 클래스
class DefaultObjectShader : public Shader
{
public:
    DefaultObjectShader() = default;
    virtual ~DefaultObjectShader() = default;

    virtual const std::string& pso_name() const override;

protected:
    virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() override;
    virtual D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;
    virtual D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) override;
};

