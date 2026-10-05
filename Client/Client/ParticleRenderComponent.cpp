#include "stdafx.h"
#include "ParticleRenderComponent.h"
#include "ParticleSystemComponent.h"
#include "GameObject.h"

void ParticleRenderComponent::render(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
    auto ps = game_object()->get_component<ParticleSystemComponent>();
    if (!ps || ps->particle_count() == 0) return;

    // DX12 렌더링 무시(Drop) 에러를 막기 위해 빈 데이터라도 b0에 바인딩
    if (_cbGameObjectInfo[frame_index]) {
        commandList->SetGraphicsRootConstantBufferView(0, _cbGameObjectInfo[frame_index]->GetGPUVirtualAddress());
    }

    ps->draw(commandList, frame_index);
}
