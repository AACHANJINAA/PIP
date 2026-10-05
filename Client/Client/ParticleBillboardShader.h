#pragma once
#include "Shader.h"

// 범용 파티클 빌보드 PSO (Particle_Billboard.hlsl). 블렌딩만 다른 두 프로토타입을 등록한다:
// "particle_additive"(가산), "particle_alpha"(알파). 깊이 테스트는 하고 깊이 쓰기는 끈다.
// 방출기 상수(b2)와 파티클 풀(t0)은 ParticleSystemComponent::draw가 묶는다.
class ParticleBillboardShader : public Shader
{
public:
    ParticleBillboardShader(std::string pso_name, bool additive) : _psoName(std::move(pso_name)), _additive(additive) {}
    ~ParticleBillboardShader() override = default;

    const std::string& pso_name() const override { return _psoName; }
    std::string required_root_signature() const override { return "particle_billboard"; }

    D3D12_INPUT_LAYOUT_DESC create_input_layout() override { return { nullptr, 0 }; } // SV_VertexID·SV_InstanceID만 사용
    D3D12_PRIMITIVE_TOPOLOGY_TYPE primitive_topology_type() const override { return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; }

    D3D12_BLEND_DESC create_blend_state() override;
    D3D12_DEPTH_STENCIL_DESC create_depth_stencil_state() override;
    D3D12_RASTERIZER_DESC create_rasterizer_state() override;

    D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;
    D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) override;

private:
    std::string _psoName;
    bool _additive;
};
