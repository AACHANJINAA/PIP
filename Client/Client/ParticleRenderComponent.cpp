#include "stdafx.h"
#include "ParticleRenderComponent.h"

void ParticleRenderComponent::render(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
    auto ps = _particleSystem;
    if (!ps || ps->get_particle_count() == 0) return;

    // DX12 렌더링 무시(Drop) 에러를 막기 위해 빈 데이터라도 b0에 바인딩
    if (_cbGameObjectInfo[frame_index]) {
        commandList->SetGraphicsRootConstantBufferView(0, _cbGameObjectInfo[frame_index]->GetGPUVirtualAddress());
    }

    // 1. 컴퓨트 셰이더가 연산한 CurrentBuffer를 SRV(t0)로 바인딩 (Root Parameter Index 3번)
    commandList->SetGraphicsRootShaderResourceView(3, ps->get_current_buffer_address());

    // 2. 버텍스 버퍼 없이 4개의 점(Triangle Strip)을 파티클 개수만큼 복사하여 그립니다.
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    commandList->DrawInstanced(4, ps->get_particle_count(), 0, 0);
}