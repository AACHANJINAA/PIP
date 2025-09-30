#pragma once
#include "Shader.h"

// [역할] 디버깅용 라인, 박스 등을 그리기 위한 셰이더 클래스
class DebugShader : public Shader
{
public:
    DebugShader() = default;
    virtual ~DebugShader() = default;

    virtual const std::string& pso_name() const override;

protected:
    virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() override;
    virtual D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;
    virtual D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) override;

    // [재정의] 디버그 셰이더는 삼각형이 아닌 라인으로 그려야 하므로, 토폴로지 타입을 재정의합니다.
    virtual D3D12_PRIMITIVE_TOPOLOGY_TYPE primitive_topology_type() const override;
};
