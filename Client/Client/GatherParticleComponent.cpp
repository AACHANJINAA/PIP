#include "stdafx.h"
#include "GatherParticleComponent.h"
#include "GameFramework.h"
#include "Renderer.h"

GatherParticleComponent::GatherParticleComponent() : ParticleSystemComponent("GatherParticleComponent")
{
}

void GatherParticleComponent::update(float deltaTime)
{
    if (_isDying && !_deathTimerEnd) {
        _deathTimer += deltaTime;
        if (_deathTimer >= _deathDuration) {
            _deathTimer = _deathDuration;
            _deathTimerEnd = true;
        }
    }
}

void GatherParticleComponent::init_particles(const std::vector<DirectX::XMFLOAT3>& targets, DirectX::XMFLOAT4 _set_color, float particle_size, float burst_radius)
{
    _particleSize = particle_size;
    _burstRadius = burst_radius;
    if (targets.empty()) return;
    _particleCount = static_cast<UINT>(targets.size());
    auto device = GameFramework::instance()->device();

    UINT bufferSize = _particleCount * sizeof(DirectX::XMFLOAT3);

    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_targetBuffer));

    void* mappedData = nullptr;
    _targetBuffer->Map(0, nullptr, &mappedData);
    memcpy(mappedData, targets.data(), bufferSize);
    _targetBuffer->Unmap(0, nullptr);

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&_currentBuffer));
    _bufferInitialized = false;

    _particleColor = _set_color;
}

void GatherParticleComponent::set_compute_data(const DirectX::XMFLOAT4X4& weapon_world, const DirectX::XMFLOAT3& player_pos, float skill_progress)
{
    _weaponWorld = weapon_world;
    _playerPos = player_pos;
    _skillProgress = skill_progress;
}

void GatherParticleComponent::dispatch_compute(ID3D12GraphicsCommandList* command_list)
{
    // 컴퓨트 PSO는 모든 컴포넌트가 하나를 공유 (처음 쓸 때 렌더러가 한 번 만듦)
    ID3D12PipelineState* pso = Renderer::instance()->get_or_create_compute_pso("compute_particle", L"Particle_CS.hlsl", "CS_Main", "compute_particle");
    if (!pso || _particleCount == 0) return;

    // 처음에는 생성 상태(COMMON), 이후에는 직전 프레임 그리기 상태(PIXEL_SHADER_RESOURCE)에서 전환
    CD3DX12_RESOURCE_BARRIER barrierUAV = CD3DX12_RESOURCE_BARRIER::Transition(
        _currentBuffer.Get(),
        _bufferInitialized ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    _bufferInitialized = true;
    command_list->ResourceBarrier(1, &barrierUAV);

    command_list->SetPipelineState(pso);
    command_list->SetComputeRootSignature(Renderer::instance()->get_root_signature("compute_particle"));

    // Particle_CS.hlsl의 cbUpdateInfo와 같은 배치 (루트 상수 24개)
    struct ComputeConstants {
        DirectX::XMFLOAT4X4 WorldMatrix; // 64 bytes
        DirectX::XMFLOAT3 PlayerPos;     // 12 bytes
        float SkillProgress;             // 4 bytes
        float DyingProgress;             // 4 bytes
        float BurstRadius;               // 4 bytes
        UINT ParticleCount;              // 4 bytes (셰이더가 이 개수 이후 스레드는 건너뜀)
        float Padding;                   // 4 bytes (총 96바이트 = 24 * 4바이트)
    } constants;
    static_assert(sizeof(ComputeConstants) == 24 * 4);

    DirectX::XMStoreFloat4x4(&constants.WorldMatrix, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&_weaponWorld)));
    constants.PlayerPos = _playerPos;
    constants.SkillProgress = _skillProgress;
    constants.DyingProgress = get_dying_progress();
    constants.BurstRadius = _burstRadius;
    constants.ParticleCount = _particleCount;
    constants.Padding = 0.0f;

    command_list->SetComputeRoot32BitConstants(0, 24, &constants, 0);

    command_list->SetComputeRootShaderResourceView(1, _targetBuffer->GetGPUVirtualAddress());
    command_list->SetComputeRootUnorderedAccessView(2, _currentBuffer->GetGPUVirtualAddress());

    UINT threadGroups = (_particleCount + 255) / 256;
    command_list->Dispatch(threadGroups, 1, 1);

    CD3DX12_RESOURCE_BARRIER barrierSRV = CD3DX12_RESOURCE_BARRIER::Transition(
        _currentBuffer.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    command_list->ResourceBarrier(1, &barrierSRV);
}

void GatherParticleComponent::draw(ID3D12GraphicsCommandList* command_list, UINT frame_index)
{
    if (!_currentBuffer || _particleCount == 0) return;

    // 컴퓨트 셰이더가 연산한 위치 버퍼를 SRV(t0)로 바인딩 (particle_draw 루트 파라미터 3번)
    command_list->SetGraphicsRootShaderResourceView(3, _currentBuffer->GetGPUVirtualAddress());

    // 버텍스 버퍼 없이 4개의 점(Triangle Strip)을 파티클 개수만큼 그림
    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    command_list->DrawInstanced(4, _particleCount, 0, 0);
}
