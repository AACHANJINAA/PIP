#pragma once
#include "Behavior.h"

class ParticleSystemComponent : public Behavior
{
public:
    ParticleSystemComponent();
    virtual ~ParticleSystemComponent();

    // C++에서 구운 정답지 데이터를 GPU로 올리는 함수
    void init_particles(const std::vector<DirectX::XMFLOAT3>& targets, DirectX::XMFLOAT4 _set_color, float particle_size = 0.05f, float burst_radius = -1.0f);

    // [추가] 매 프레임 업데이트에서 데이터만 저장해두는 함수
    void set_compute_data(const DirectX::XMFLOAT4X4& weapon_world, const DirectX::XMFLOAT3& player_pos, float skill_progress);

    // [추가] 렌더러에서 직접 호출할 컴퓨트 셰이더 실행 함수
    void dispatch_compute(ID3D12GraphicsCommandList* command_list);

    // 렌더링 시 사용할 현재 버퍼의 GPU 주소
    D3D12_GPU_VIRTUAL_ADDRESS get_current_buffer_address() const { return _currentBuffer ? _currentBuffer->GetGPUVirtualAddress() : 0; }
    UINT get_particle_count() const { return _particleCount; }

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

	// 3초 기준의 죽음 진행도 (0.0 ~ 1.0) 반환
	float get_dying_progress() const { return std::clamp(_deathTimer / _deathDuration, 0.0f, 1.0f); }

	// 없어지는 연출이 끝났는지 여부를 확인하는 함수
	bool is_death_timer_end() const { return _deathTimerEnd; }

	// 사라지는 연출 지속 시간을 설정
	void set_death_duration(float duration) { _deathDuration = duration; }

	// Behavior의 update 오버라이드
	void update(float deltaTime) override;

private:
    void create_compute_pso();

private:
    ComPtr<ID3D12Resource> _targetBuffer;  // 정답지 (SRV)
    ComPtr<ID3D12Resource> _currentBuffer; // 현재 위치 (UAV)

    ComPtr<ID3D12PipelineState> _computePSO;
    UINT _particleCount = 0;

    // [추가] 렌더러로 넘겨주기 위해 임시 저장해둘 데이터
    DirectX::XMFLOAT4X4 _weaponWorld;
    DirectX::XMFLOAT3 _playerPos;
	DirectX::XMFLOAT4 _particleColor{ 1,1,1,1 }; // 파티클 색상 (기본값 흰색)
    float _skillProgress = 0.0f;
	float _particleSize = 0.05f; // 파티클 크기 (임시로 0.1f로 설정)
    float _burstRadius = -1.0f; // 초기 파티클 확산 최대 반경


    // 파티클 사라지는 연출을 위한 타이머
	bool _isDying = false;
	bool _deathTimerEnd = false;
	float _deathTimer = 0.0f;
	float _deathDuration = 3.f; // 사라지는 연출 총 시간 (3초)

};