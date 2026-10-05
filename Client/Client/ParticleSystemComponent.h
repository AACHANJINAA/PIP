#pragma once
#include "Behavior.h"
#include "ParticleRenderComponent.h"

// 파티클 컴포넌트 기반 (유니티 ParticleSystem에 해당, ParticleSystem_Plan_KR.md)
// 붙이면 ParticleRenderComponent가 자동으로 같이 붙고(required_components), 렌더러는 아래 가상 함수만 부른다.
// 범용 방출·갱신(GPU 컴퓨트)은 P2에서 이 클래스에 구현하고, 목표점으로 모이는 연출은 GatherParticleComponent가 맡는다.
class ParticleSystemComponent : public Behavior
{
public:
    using required_components = std::tuple<ParticleRenderComponent>;

    ParticleSystemComponent() : Behavior("ParticleSystemComponent") {}
    explicit ParticleSystemComponent(const std::string& name) : Behavior(name) {}
    ~ParticleSystemComponent() override = default;

    // 렌더러가 그리기 전에 호출: 컴퓨트 실행 (리소스 상태 전환 포함, 그래픽 상태는 렌더러가 다시 묶음)
    virtual void dispatch_compute(ID3D12GraphicsCommandList* command_list) {}
    // ParticleRenderComponent가 호출: PSO·루트 시그니처·카메라·b0가 묶인 상태에서 그리기
    virtual void draw(ID3D12GraphicsCommandList* command_list, UINT frame_index) {}
    // 그릴 파티클 수 (0이면 그리지 않음)
    virtual UINT particle_count() const { return 0; }
};
