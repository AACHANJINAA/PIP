#pragma once
#include "RenderComponent.h"

// 파티클을 렌더 목록에 올리기 위한 렌더 컴포넌트
// 같은 오브젝트의 ParticleSystemComponent(또는 파생)가 required_components로 자동으로 붙이며, 실제 그리기는 그 컴포넌트의 draw로 넘긴다.
class ParticleRenderComponent : public RenderComponent
{
public:
    ParticleRenderComponent() = default;
    virtual ~ParticleRenderComponent() = default;

    // 메쉬 렌더링 대신 파티클 컴포넌트의 draw 호출
    virtual void render(ID3D12GraphicsCommandList* commandList, UINT frame_index) override;
};
