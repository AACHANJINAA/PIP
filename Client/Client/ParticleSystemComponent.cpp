#include "stdafx.h"
#include "ParticleSystemComponent.h"
#include "GameFramework.h"
#include "Renderer.h"
#include "ResourceManager.h"

ParticleSystemComponent::ParticleSystemComponent() : Behavior("ParticleSystemComponent")
{
    create_compute_pso();
}

ParticleSystemComponent::~ParticleSystemComponent()
{
}

void ParticleSystemComponent::create_compute_pso()
{
    auto device = GameFramework::instance()->device();

    ComPtr<ID3DBlob> computeShader;
    ComPtr<ID3DBlob> errorBlob;
#if defined(_DEBUG)
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    HRESULT hr = D3DCompileFromFile(L"Particle_CS.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "CS_Main", "cs_5_1", compileFlags, 0, &computeShader, &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = Renderer::instance()->get_root_signature("compute_particle");
    psoDesc.CS = { reinterpret_cast<BYTE*>(computeShader->GetBufferPointer()), computeShader->GetBufferSize() };
    psoDesc.NodeMask = 0;
    psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&_computePSO));
}

void ParticleSystemComponent::init_particles(const std::vector<DirectX::XMFLOAT3>& targets, DirectX::XMFLOAT4 _set_color)
{
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

    // ResourceManager::instance()->load_texture("Resource/UI/particle/particle.dds", true);
	_particleColor = _set_color;
}

void ParticleSystemComponent::set_compute_data(const DirectX::XMFLOAT4X4& weapon_world, const DirectX::XMFLOAT3& player_pos, float skill_progress)
{
    _weaponWorld = weapon_world;
    _playerPos = player_pos;
    _skillProgress = skill_progress;
}

void ParticleSystemComponent::dispatch_compute(ID3D12GraphicsCommandList* command_list)
{
    if (!_computePSO || _particleCount == 0) return;

    CD3DX12_RESOURCE_BARRIER barrierUAV = CD3DX12_RESOURCE_BARRIER::Transition(
        _currentBuffer.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    command_list->ResourceBarrier(1, &barrierUAV);

    command_list->SetPipelineState(_computePSO.Get());
    command_list->SetComputeRootSignature(Renderer::instance()->get_root_signature("compute_particle"));

    struct ComputeConstants {
        DirectX::XMFLOAT4X4 WorldMatrix;
        DirectX::XMFLOAT3 PlayerPos;
        float SkillProgress;
    } constants;

    DirectX::XMStoreFloat4x4(&constants.WorldMatrix, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&_weaponWorld)));
    constants.PlayerPos = _playerPos;
    constants.SkillProgress = _skillProgress;

    command_list->SetComputeRoot32BitConstants(0, 20, &constants, 0);

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