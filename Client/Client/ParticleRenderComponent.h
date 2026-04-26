#pragma once
#include "RenderComponent.h"
#include "ParticleSystemComponent.h"

class ParticleRenderComponent : public RenderComponent
{
public:
    ParticleRenderComponent() = default;
    virtual ~ParticleRenderComponent() = default;

    void set_particle_system(std::shared_ptr<ParticleSystemComponent> ps) { _particleSystem = ps; }

    // 메쉬 렌더링 대신 인스턴싱 렌더링을 오버라이드
    virtual void render(ID3D12GraphicsCommandList* commandList, UINT frame_index) override;

private:
    std::shared_ptr<ParticleSystemComponent> _particleSystem;
};