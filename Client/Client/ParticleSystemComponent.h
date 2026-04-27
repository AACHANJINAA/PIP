#pragma once
#include "Behavior.h"

class ParticleSystemComponent : public Behavior
{
public:
    ParticleSystemComponent();
    virtual ~ParticleSystemComponent();

    // C++에서 구운 정답지 데이터를 GPU로 올리는 함수
    void init_particles(const std::vector<DirectX::XMFLOAT3>& targets, DirectX::XMFLOAT4 _set_color );

    // [추가] 매 프레임 업데이트에서 데이터만 저장해두는 함수
    void set_compute_data(const DirectX::XMFLOAT4X4& weapon_world, const DirectX::XMFLOAT3& player_pos, float skill_progress);

    // [추가] 렌더러에서 직접 호출할 컴퓨트 셰이더 실행 함수
    void dispatch_compute(ID3D12GraphicsCommandList* command_list);

    // 렌더링 시 사용할 현재 버퍼의 GPU 주소
    D3D12_GPU_VIRTUAL_ADDRESS get_current_buffer_address() const { return _currentBuffer ? _currentBuffer->GetGPUVirtualAddress() : 0; }
    UINT get_particle_count() const { return _particleCount; }

    DirectX::XMFLOAT4 get_particle_color() const { return _particleColor; }

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
};