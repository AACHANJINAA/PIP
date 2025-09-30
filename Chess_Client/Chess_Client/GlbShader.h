#pragma once
#include "Shader.h"

// [역할] GLB/glTF 파일의 스키닝 애니메이션 메시를 렌더링하는 데 필요한 PSO 정보를 제공합니다.
class GlbShader : public Shader
{
public:
    GlbShader() = default;
    virtual ~GlbShader() = default;

    // --- Shader 클래스의 순수 가상 함수들을 오버라이드합니다. ---

    // 이 셰이더가 생성할 PSO의 이름을 "skinned"로 지정합니다.
    virtual const std::string& pso_name() const override;

protected:
    // 스키닝된 정점(SkinnedVertex) 구조에 맞는 Input Layout을 정의합니다.
    virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() override;

    // 스키닝을 처리하는 Vertex Shader 정보를 반환합니다.
    virtual D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;

    // Pixel Shader 정보를 반환합니다.
    virtual D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) override;

    // 스키닝 셰이더는 별도의 루트 시그니처가 필요하므로, 이 함수를 오버라이드합니다.
    virtual std::string required_root_signature() const override;
};

