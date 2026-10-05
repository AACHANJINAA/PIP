#pragma once
#include "ParticleSystemComponent.h"

// 목표점으로 모이는 파티클 (대검 스킬, 컷씬 플레이어 파티클, 분수 연출)
// 파티클마다 수명·속도 없이, 위치를 진행도(set_compute_data)로부터 Particle_CS.hlsl이 직접 계산한다.
// 범용 파티클(ParticleSystemComponent)의 방출·갱신은 쓰지 않고 dispatch_compute·draw만 자체 구현한다.
class GatherParticleComponent : public ParticleSystemComponent
{
public:
    GatherParticleComponent();
    ~GatherParticleComponent() override = default;

    // C++에서 구운 정답지(목표점) 데이터를 GPU로 올림
    void init_particles(const std::vector<DirectX::XMFLOAT3>& targets, DirectX::XMFLOAT4 _set_color, float particle_size = 0.05f, float burst_radius = -1.0f);

    // 매 프레임 갱신에서 데이터만 저장 (실제 계산은 렌더러가 dispatch_compute 호출 시)
    void set_compute_data(const DirectX::XMFLOAT4X4& weapon_world, const DirectX::XMFLOAT3& player_pos, float skill_progress);

    // --- ParticleSystemComponent (렌더러가 호출) ---
    void dispatch_compute(ID3D12GraphicsCommandList* command_list) override;
    void draw(ID3D12GraphicsCommandList* command_list, UINT frame_index) override;
    UINT particle_count() const override { return _particleCount; }

    DirectX::XMFLOAT4 get_particle_color() const { return _particleColor; }
    float get_particle_size() const { return _particleSize; } // 파티클 크기

    // 파티클 없어지는 연출 관련 함수들
    void set_particle_dying(bool isDying)
    {
        _isDying = isDying;
        if (!isDying) {
            _deathTimer = 0.0f; // 다시 살아날 때 타이머 리셋
            _deathTimerEnd = false;
        }
    }
    bool is_dying() const { return _isDying; }
    float get_progress() const { return _skillProgress; }

    // 죽음 진행도 (0.0 ~ 1.0)
    float get_dying_progress() const { return std::clamp(_deathTimer / _deathDuration, 0.0f, 1.0f); }

    // 없어지는 연출이 끝났는지 여부
    bool is_death_timer_end() const { return _deathTimerEnd; }

    // 사라지는 연출 지속 시간을 설정
    void set_death_duration(float duration) { _deathDuration = duration; }

    void update(float deltaTime) override;

private:
    ComPtr<ID3D12Resource> _targetBuffer;  // 정답지 (SRV)
    ComPtr<ID3D12Resource> _currentBuffer; // 현재 위치 (UAV)
    bool _bufferInitialized = false;       // 첫 컴퓨트 전에는 생성 상태(COMMON)

    UINT _particleCount = 0;

    // 렌더러로 넘겨주기 위해 임시 저장해둘 데이터
    DirectX::XMFLOAT4X4 _weaponWorld;
    DirectX::XMFLOAT3 _playerPos;
    DirectX::XMFLOAT4 _particleColor{ 1,1,1,1 }; // 파티클 색상 (기본값 흰색)
    float _skillProgress = 0.0f;
    float _particleSize = 0.05f;
    float _burstRadius = -1.0f; // 초기 파티클 확산 최대 반경

    // 파티클 사라지는 연출을 위한 타이머
    bool _isDying = false;
    bool _deathTimerEnd = false;
    float _deathTimer = 0.0f;
    float _deathDuration = 3.f; // 사라지는 연출 총 시간 (3초)
};
