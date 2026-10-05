#pragma once
#include <deque>
#include <random>
#include "Behavior.h"
#include "ParticleRenderComponent.h"
#include "ParticleSystemSettings.h"

// 범용 파티클 (유니티 ParticleSystem에 해당, ParticleSystem_Plan_KR.md)
//
// 사용법:
//   auto ps = obj->add_component<ParticleSystemComponent>();   // ParticleRenderComponent는 자동으로 붙음
//   auto& s = ps->settings();
//   s.start_lifetime = { 0.5f, 1.0f }; s.rate_over_time = 40.0f; s.shape = ParticleShape::Cone;
//   ps->play();                                                  // play_on_awake면 생략 가능
//
// 내부: CPU는 방출 요청(위치, 방향, 개수, 시드)만 만들고, 초기값 생성·갱신·그리기는 GPU 컴퓨트·빌보드가 한다.
// 방출 위치·방향은 오브젝트의 월드 위치·정면(+Z). 시간은 오브젝트 시간(GameObject 시간 배율)을 따른다.
//
// 렌더러는 dispatch_compute·draw·particle_count만 부른다. 다른 동작이 필요한 파티클(GatherParticleComponent)은 이 셋을 오버라이드한다.
class ParticleSystemComponent : public Behavior
{
public:
    using required_components = std::tuple<ParticleRenderComponent>;

    ParticleSystemComponent();
    explicit ParticleSystemComponent(const std::string& name);
    ~ParticleSystemComponent() override;

    void awake() override;
    void update(float deltaTime) override;

    // --- 설정 (유니티 인스펙터의 Main·Emission·Shape·Renderer 모듈) ---
    ParticleSystemSettings& settings() { return _settings; }
    const ParticleSystemSettings& settings() const { return _settings; }
    void set_settings(const ParticleSystemSettings& settings) { _settings = settings; }

    // --- 재생 제어 ---
    void play();                // 처음부터 재생 (연속 방출·버스트)
    void stop();                // 방출만 멈춤 (살아 있는 파티클은 수명대로)
    void clear();               // 살아 있는 파티클까지 모두 지움
    bool is_playing() const { return _playing; }

    // 즉시 방출: 오브젝트 위치·정면에서 / 지정한 월드 위치·방향에서 (재생 여부와 무관)
    void emit(int count);
    void emit(int count, const XMFLOAT3& world_pos, const XMFLOAT3& world_dir);

    // --- 렌더러가 호출 ---
    // 그리기 전에 호출: 방출·갱신 컴퓨트 (리소스 상태 전환 포함, 그래픽 상태는 렌더러가 다시 묶음)
    virtual void dispatch_compute(ID3D12GraphicsCommandList* command_list);
    // ParticleRenderComponent가 호출: PSO·루트 시그니처·카메라·b0가 묶인 상태에서 그리기
    virtual void draw(ID3D12GraphicsCommandList* command_list, UINT frame_index);
    // 그릴 파티클 수 (0이면 그리지 않음). 범용은 살아 있을 수 있으면 풀 크기
    virtual UINT particle_count() const;

private:
    struct EmitRequest      // Particle_Common.hlsli의 EmitRequest와 같은 배치 (48바이트)
    {
        XMFLOAT3 pos;
        UINT poolStart;
        XMFLOAT3 dir;
        UINT count;
        UINT seed;
        UINT firstThread;
        UINT pad[2];
    };
    struct Batch            // 링 버퍼에 쓴 묶음 (가장 오래된 것부터 만료되면 자리를 돌려받음)
    {
        UINT count;
        double expireTime;  // 이 묶음의 최대 수명이 끝나는 시각 (_time 기준)
    };

    void apply_render_pso();
    bool ensure_pool(ID3D12Device* device);
    void release_expired();

    ParticleSystemSettings _settings;

    // 재생 상태
    bool _playing = false;
    float _playTime = 0.0f;         // 이번 주기에서 지난 시간
    bool _cycleStarted = false;     // 이번 주기의 첫 update를 지났는지 (0초 버스트 한 번만)
    float _emitAccumulator = 0.0f;  // 연속 방출 소수점 누적
    double _time = 0.0;             // 컴포넌트 시간 (만료 계산용)
    float _pendingDeltaTime = 0.0f; // 마지막 컴퓨트 이후 쌓인 시뮬레이션 시간

    // 링 버퍼 (CPU가 쓰기 위치·살아 있을 수 있는 수를 추정, GPU에서 읽어 오지 않음)
    std::deque<Batch> _batches;
    UINT _head = 0;
    UINT _aliveEstimate = 0;
    bool _overflowLogged = false;

    std::vector<EmitRequest> _pendingRequests;
    UINT _pendingParticles = 0;
    std::mt19937 _rng;

    // GPU 리소스
    ComPtr<ID3D12Resource> _pool;
    UINT _poolCapacity = 0;
    D3D12_RESOURCE_STATES _poolState = D3D12_RESOURCE_STATE_COMMON;
    bool _clearRequested = false;
    D3D12_GPU_VIRTUAL_ADDRESS _constantsGpu = 0;    // 이번 프레임 방출기 상수 (draw에서 b2로)
};
