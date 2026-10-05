#include "stdafx.h"
#include "ParticleSystemComponent.h"
#include "GameFramework.h"
#include "GameObject.h"
#include "Renderer.h"
#include "TransformComponent.h"

namespace
{
    // Particle_Common.hlsli의 Particle과 같은 크기
    constexpr UINT kParticleStride = 48;

    // Particle_Common.hlsli의 cbuffer ParticleEmitter와 같은 배치
    struct EmitterConstants
    {
        XMFLOAT4 startColorA;
        XMFLOAT4 startColorB;
        XMFLOAT4 endColor;
        XMFLOAT2 lifetime;
        XMFLOAT2 speed;
        XMFLOAT2 size;
        XMFLOAT2 rotation;
        float gravity;
        float drag;
        float endSizeMultiplier;
        float lengthScale;
        UINT shape;
        float radius;
        float angle;
        float edgeLength;
        UINT maxParticles;
        UINT renderMode;
        float deltaTime;
        UINT requestCount;
        UINT newParticleCount;
        UINT clearAll;
        UINT pad[2];
    };
    static_assert(sizeof(EmitterConstants) == 144);

    constexpr float kGravity = 9.81f;

    UINT dispatch_groups(UINT count) { return (count + 63) / 64; }
}

ParticleSystemComponent::ParticleSystemComponent() : ParticleSystemComponent("ParticleSystemComponent")
{
}

ParticleSystemComponent::ParticleSystemComponent(const std::string& name)
    : Behavior(name), _rng(std::random_device{}())
{
}

ParticleSystemComponent::~ParticleSystemComponent() = default;

void ParticleSystemComponent::awake()
{
    apply_render_pso();
    if (_settings.play_on_awake) play();
}

void ParticleSystemComponent::apply_render_pso()
{
    auto owner = game_object();
    if (!owner) return;
    auto render = owner->get_component<ParticleRenderComponent>();
    if (!render) return;
    const char* pso = _settings.blend == ParticleBlend::Additive ? "particle_additive" : "particle_alpha";
    if (render->pso_name() != pso) render->set_pso_name(pso);
}

void ParticleSystemComponent::play()
{
    _playing = true;
    _playTime = 0.0f;
    _emitAccumulator = 0.0f;
    _cycleStarted = false; // 다음 update에서 0초 버스트부터 처리
}

void ParticleSystemComponent::stop()
{
    _playing = false;
}

void ParticleSystemComponent::clear()
{
    _batches.clear();
    _aliveEstimate = 0;
    _pendingRequests.clear();
    _pendingParticles = 0;
    _clearRequested = true;
}

void ParticleSystemComponent::release_expired()
{
    while (!_batches.empty() && _batches.front().expireTime <= _time)
    {
        _aliveEstimate -= _batches.front().count;
        _batches.pop_front();
    }
}

void ParticleSystemComponent::emit(int count)
{
    auto owner = game_object();
    if (!owner || !owner->transform()) return;
    XMFLOAT3 pos = owner->transform()->get_world_position();
    XMFLOAT3 dir = owner->transform()->forward();
    emit(count, pos, dir);
}

void ParticleSystemComponent::emit(int count, const XMFLOAT3& world_pos, const XMFLOAT3& world_dir)
{
    if (count <= 0) return;

    const UINT capacity = _poolCapacity > 0 ? _poolCapacity : _settings.max_particles;
    if (capacity == 0) return;

    // 꽉 차면 새로 만들지 않음 (유니티 Max Particles, VFX Graph Capacity, Niagara FixedCount와 같음)
    release_expired();
    const UINT free = capacity - std::min(capacity, _aliveEstimate);
    const UINT accepted = std::min<UINT>(static_cast<UINT>(count), free);
    if (accepted < static_cast<UINT>(count) && !_overflowLogged)
    {
        CLOG("[Particle] " << (game_object() ? game_object()->name() : std::string("?")) << ": 최대 개수(" << capacity
            << ") 초과로 " << (count - static_cast<int>(accepted)) << "개 방출 안 함 (이후 같은 경고 생략)");
        _overflowLogged = true;
    }
    if (accepted == 0) return;

    EmitRequest req = {};
    req.pos = world_pos;
    req.dir = world_dir;
    req.poolStart = _head;
    req.count = accepted;
    req.seed = _rng();
    req.firstThread = _pendingParticles;
    _pendingRequests.push_back(req);
    _pendingParticles += accepted;

    _head = (_head + accepted) % capacity;
    _aliveEstimate += accepted;
    _batches.push_back({ accepted, _time + std::max(_settings.start_lifetime.x, _settings.start_lifetime.y) });
}

void ParticleSystemComponent::update(float deltaTime)
{
    apply_render_pso();

    _time += deltaTime;
    _pendingDeltaTime += deltaTime;
    release_expired();

    if (!_playing) return;

    // 버스트: 주기 시작 프레임은 [0, 현재], 이후는 (이전, 현재] 구간에 든 것
    // (히트스톱처럼 시간이 0으로 흘러도 같은 버스트를 다시 내지 않음)
    const float prevTime = _playTime;
    _playTime += deltaTime;
    for (const auto& burst : _settings.bursts)
    {
        const bool in_range = _cycleStarted ? (burst.time > prevTime && burst.time <= _playTime) : (burst.time <= _playTime);
        if (in_range) emit(burst.count);
    }
    _cycleStarted = true;

    // 연속 방출
    _emitAccumulator += _settings.rate_over_time * deltaTime;
    if (_emitAccumulator >= 1.0f)
    {
        const int n = static_cast<int>(_emitAccumulator);
        _emitAccumulator -= static_cast<float>(n);
        emit(n);
    }

    // 주기 끝
    if (_settings.duration > 0.0f && _playTime >= _settings.duration)
    {
        if (_settings.looping)
        {
            _playTime = 0.0f;
            _cycleStarted = false;
        }
        else _playing = false;
    }
}

bool ParticleSystemComponent::ensure_pool(ID3D12Device* device)
{
    if (_pool)
    {
        if (_settings.max_particles != _poolCapacity)
        {
            static bool logged = false;
            if (!logged) CLOG("[Particle] max_particles는 처음 재생 후 바꿀 수 없음 (" << _poolCapacity << " 유지)");
            logged = true;
            _settings.max_particles = _poolCapacity;
        }
        return true;
    }
    if (_settings.max_particles == 0) return false;

    _poolCapacity = _settings.max_particles;
    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(_poolCapacity) * kParticleStride, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    // 기본 힙은 0으로 채워져 생성되므로 모든 파티클이 수명 0(빈 자리)에서 시작
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&_pool))))
    {
        CERROR("[Particle] 파티클 버퍼 생성 실패 (" << _poolCapacity << "개)");
        _pool.Reset();
        return false;
    }
    _pool->SetName(L"ParticleSystemComponent_Pool");
    _poolState = D3D12_RESOURCE_STATE_COMMON;
    return true;
}

void ParticleSystemComponent::dispatch_compute(ID3D12GraphicsCommandList* command_list)
{
    _constantsGpu = 0;
    auto renderer = Renderer::instance();
    if (!ensure_pool(renderer->get_device())) return;

    ID3D12PipelineState* updatePso = renderer->get_or_create_compute_pso("particle_update", L"Particle_Update_CS.hlsl", "CS_Update", "particle_compute");
    ID3D12PipelineState* emitPso = renderer->get_or_create_compute_pso("particle_emit", L"Particle_Emit_CS.hlsl", "CS_Emit", "particle_compute");
    if (!updatePso || !emitPso) return;

    // 이번 프레임 상수 (방출·갱신·그리기 공용)
    auto allocator = GameFramework::instance()->linear_allocator();
    auto cb = allocator->allocate(sizeof(EmitterConstants));
    if (!cb.cpuPtr) return;

    EmitterConstants c = {};
    c.startColorA = _settings.start_color;
    c.startColorB = _settings.start_color_2;
    c.endColor = _settings.end_color;
    c.lifetime = _settings.start_lifetime;
    c.speed = _settings.start_speed;
    c.size = _settings.start_size;
    c.rotation = { XMConvertToRadians(_settings.start_rotation.x), XMConvertToRadians(_settings.start_rotation.y) };
    c.gravity = -kGravity * _settings.gravity_modifier;
    c.drag = _settings.drag;
    c.endSizeMultiplier = _settings.end_size_multiplier;
    c.lengthScale = _settings.length_scale;
    c.shape = static_cast<UINT>(_settings.shape);
    c.radius = _settings.radius;
    c.angle = XMConvertToRadians(_settings.angle);
    c.edgeLength = _settings.length;
    c.maxParticles = _poolCapacity;
    c.renderMode = static_cast<UINT>(_settings.render_mode);
    c.deltaTime = _pendingDeltaTime;
    c.requestCount = static_cast<UINT>(_pendingRequests.size());
    c.newParticleCount = _pendingParticles;
    c.clearAll = _clearRequested ? 1u : 0u;
    memcpy(cb.cpuPtr, &c, sizeof(c));
    _constantsGpu = cb.gpuAddr;

    // 방출 요청 업로드 (요청이 없어도 t0에 묶을 빈 자리 하나는 확보)
    const size_t requestBytes = std::max<size_t>(_pendingRequests.size(), 1) * sizeof(EmitRequest);
    auto req = allocator->allocate(requestBytes);
    if (!req.cpuPtr) return;
    memset(req.cpuPtr, 0, requestBytes);
    if (!_pendingRequests.empty()) memcpy(req.cpuPtr, _pendingRequests.data(), _pendingRequests.size() * sizeof(EmitRequest));
    const D3D12_GPU_VIRTUAL_ADDRESS requestsGpu = req.gpuAddr;

    // 풀을 UAV로
    if (_poolState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_pool.Get(), _poolState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        command_list->ResourceBarrier(1, &barrier);
        _poolState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    command_list->SetComputeRootSignature(renderer->get_root_signature("particle_compute"));
    command_list->SetComputeRootConstantBufferView(0, _constantsGpu);
    command_list->SetComputeRootShaderResourceView(1, requestsGpu);
    command_list->SetComputeRootUnorderedAccessView(2, _pool->GetGPUVirtualAddress());

    // 1. 갱신: 기존 파티클을 이번 프레임 시간만큼 진행 (새 파티클은 나이 0에서 시작하도록 먼저)
    command_list->SetPipelineState(updatePso);
    command_list->Dispatch(dispatch_groups(_poolCapacity), 1, 1);

    // 2. 방출
    if (_pendingParticles > 0)
    {
        auto uav = CD3DX12_RESOURCE_BARRIER::UAV(_pool.Get());
        command_list->ResourceBarrier(1, &uav);
        command_list->SetPipelineState(emitPso);
        command_list->Dispatch(dispatch_groups(_pendingParticles), 1, 1);
    }

    // 정점 셰이더에서 읽도록
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_pool.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    command_list->ResourceBarrier(1, &barrier);
    _poolState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    _pendingRequests.clear();
    _pendingParticles = 0;
    _pendingDeltaTime = 0.0f;
    _clearRequested = false;
}

void ParticleSystemComponent::draw(ID3D12GraphicsCommandList* command_list, UINT frame_index)
{
    if (!_pool || !_constantsGpu || _poolState != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) return;

    command_list->SetGraphicsRootConstantBufferView(2, _constantsGpu);
    command_list->SetGraphicsRootShaderResourceView(3, _pool->GetGPUVirtualAddress());
    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    command_list->DrawInstanced(4, _poolCapacity, 0, 0);
}

UINT ParticleSystemComponent::particle_count() const
{
    // CPU 추정으로 살아 있는 파티클이 없으면 그리지 않음 (추정은 최대 수명 기준이라 실제보다 크거나 같음)
    if (_aliveEstimate == 0 && _pendingParticles == 0) return 0;
    return _poolCapacity > 0 ? _poolCapacity : _settings.max_particles;
}
