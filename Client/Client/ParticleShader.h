#pragma once
#include "Shader.h"

class ParticleShader : public Shader
{
public:
    ParticleShader() = default;
    virtual ~ParticleShader() = default;

    virtual const std::string& pso_name() const override;
    virtual std::string required_root_signature() const override;

    // [핵심 1] 입력 레이아웃 없음 (SV_VertexID 사용)
    virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() override;

    virtual D3D12_PRIMITIVE_TOPOLOGY_TYPE primitive_topology_type() const override {
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }

    virtual D3D12_BLEND_DESC create_blend_state() override;
    virtual D3D12_DEPTH_STENCIL_DESC create_depth_stencil_state() override;
    virtual D3D12_RASTERIZER_DESC create_rasterizer_state() override;

    virtual D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;
    virtual D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) override;

    virtual void update_per_object(ID3D12GraphicsCommandList* command_list, class Renderer* renderer, GameObject* object) override;
};