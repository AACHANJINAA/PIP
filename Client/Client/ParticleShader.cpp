#include "stdafx.h"
#include "ParticleShader.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "GameObject.h"
#include "ParticleSystemComponent.h"

const std::string& ParticleShader::pso_name() const {
    static const std::string name = "particle_draw";
    return name;
}

std::string ParticleShader::required_root_signature() const {
    return "particle_draw";
}

// 버텍스 버퍼를 안 쓰므로 비워둡니다. (엄청난 최적화)
D3D12_INPUT_LAYOUT_DESC ParticleShader::create_input_layout() {
    return D3D12_INPUT_LAYOUT_DESC{ nullptr, 0 };
}

D3D12_RASTERIZER_DESC ParticleShader::create_rasterizer_state() {
    D3D12_RASTERIZER_DESC desc = Shader::create_rasterizer_state();
    desc.CullMode = D3D12_CULL_MODE_NONE; // 빌보드라 어느 방향이든 보이게 함
    return desc;
}

D3D12_BLEND_DESC ParticleShader::create_blend_state() {
    D3D12_BLEND_DESC desc = {};
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA; // 겹쳐도 안밝아지게
    desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
    desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    return desc;
}

D3D12_DEPTH_STENCIL_DESC ParticleShader::create_depth_stencil_state() {
    D3D12_DEPTH_STENCIL_DESC desc = {};
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 파티클 끼리는 겹쳐도 렌더링되도록 깊이 쓰기 끄기
    desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // 깊이검사 끄는 디버깅 용
    // desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    return desc;
}

D3D12_SHADER_BYTECODE ParticleShader::create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) {
    return compile_shader_from_file(L"Particle_Draw.hlsl", "VS_Particle", "vs_5_1", shader_blob);
}

D3D12_SHADER_BYTECODE ParticleShader::create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) {
    return compile_shader_from_file(L"Particle_Draw.hlsl", "PS_Particle", "ps_5_1", shader_blob);
}

void ParticleShader::update_per_object(ID3D12GraphicsCommandList* command_list, class Renderer* renderer, GameObject* object) {
    Shader::update_per_object(command_list, renderer, object);

    // 파티클 색상(푸른 마력)과 크기를 b2 루트 상수로 전달
    struct ParticleInfo {
        DirectX::XMFLOAT4 Color;
        float Size;
		float progress; // 0~1 사이의 값으로, 파티클이 다 모였는지?
		float dying_progress; // 0~1 사이의 값으로, 파티클이 사라지는 중인지?
    } pInfo;

    static const DirectX::XMFLOAT3 PlayerColors[4] =
    {
        DirectX::XMFLOAT3(0.863f, 0.078f, 0.235f), // crimson red
        DirectX::XMFLOAT3(0.0f, 1.0f, 0.498f), // spring green
        DirectX::XMFLOAT3(1.0f, 0.843f, 0.0f), // gold
        DirectX::XMFLOAT3(0.541f, 0.169f, 0.886f), // violet
    };

    pInfo.Color = { 0.1f, 0.5f, 1.0f, 0.5f }; // 카리아 대검 파티클 색상

    auto particleComponent = object->get_component<ParticleSystemComponent>();
    if (particleComponent) {
        pInfo.Color = particleComponent->get_particle_color();
    }

    pInfo.Size = 0.03f; // 파티클 입자 하나의 크기 (수정하며 테스트)

    pInfo.dying_progress = particleComponent->get_dying_progress();
    pInfo.progress = particleComponent->get_progress();


    command_list->SetGraphicsRoot32BitConstants(2, 7, &pInfo, 0);


    // 텍스쳐 버림 그냥 바인딩도 하지마 셰이더에서 호출 하지도 마
    
    // 반짝이는 빛 텍스처 (미리 로드해둔 파티클 텍스처 이름)
    //auto particle_tex = ResourceManager::instance()->get_texture("Resource/UI/particle/particle.dds");

    // 만약 경로가 틀렸거나 로딩이 안 됐을 때 튕기지 않게 기본 흰색 텍스처로 대체
   // if (!particle_tex) {
   //     particle_tex = ResourceManager::instance()->get_texture("__DEFAULT_WHITE__");
    //}

   // if (particle_tex) {
        // 루트 파라미터 인덱스 4번(t1)에 디스크립터 테이블(텍스처)을 바인딩합니다.
  //      renderer->bind_texture_table(command_list, 4, { particle_tex->cpu_handle });
   // }
}