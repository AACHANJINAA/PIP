#pragma once
#include "RenderComponent.h"
#include "ScriptComponent.h"
#include "AnimationComponent.h"
#include "SocketComponenet.h"

class OtherPlayerScript : public ScriptComponent
{
public:
	using required_components = std::tuple<RenderComponent, AnimationComponent, SocketComponenet>;
    OtherPlayerScript() = default;
    virtual ~OtherPlayerScript() = default;

    virtual void update(float deltaTime) override;

    void awake() override;

    // 서버로부터 위치 동기화 패킷을 받았을 때 호출될 함수 (예시)
    void on_sync_position(const XMFLOAT3& newPosition);
	void on_sync_rotation(const XMFLOAT4& newRotation);
    void on_sync_state(common::packet::EntityState state);
    void on_sync_action_id(int32_t action_id);
    void on_sync_grab(int64_t grabbed_by_id, int8_t grab_slot); // [추가]
    void on_sync_velocity(const common::Vec3& velocity) { _velocity = velocity; } // [추가]
    void on_sync_hp(int hp) { _hp = hp; } // [추가]
    void on_sync_mp(int mp) { _mp = mp; } // [추가]
    void reset_state(); // [추가] 리스폰 시 상태 초기화

	void set_party_slot_index(int index) { _partySlotIndex = index; }
	int get_party_slot_index() const { return _partySlotIndex; }

    void set_hp(int hp) { _hp = hp; }    int hp() const { return _hp; }
    void set_max_hp(int maxHp) { _maxHp = maxHp; } // [추가]
    void set_id(int64_t id) { _playerId = id; }
    int64_t id() const { return _playerId; }
    bool  is_skilling() const { return _isSkilling; }
private:
    int _hp{ 100 };
    int _maxHp{ 100 }; // [추가]
    int _mp{ 100 }; // [추가]
    int64_t _playerId = -1; // [수정] Session ID(0) 와의 충돌 방지를 위해 -1 로 초기화
    common::packet::EntityState _state;
    common::packet::EntityState _prevState = common::packet::EntityState::IDLE; // [추가] 이전 상태 추적용
    int32_t _action_id = 0;
    int64_t _grabbedById = -1; // [추가]
    int8_t  _grabSlot = -1;    // [추가]
    // --- [추측 항법 및 보간용 변수] ---
    common::Vec3    _logicalPosition;   // 서버가 알려준 최신 논리적 위치
    common::Vec3    _visualOffset;      // 시각적 보간을 위한 오프셋 (이전 위치와의 차이)
    common::Vec3    _velocity;          // 추측 항법을 위한 속도 (옵션)

    float           _lerpFactor = 15.0f; // 보간 속도 (수치가 클수록 서버 위치에 빨리 도달)


	// 공격 관련 변수들
	float _attackAnimationSpeed = 1.8f; // 공격 애니메이션 속도

    // 스킬을 위한 변수들
    bool _isSkilling = false;
    bool _isSkillAnimationStarted = false;      // [추가] 스킬 최초 시작 체크용
    bool _isSkillEndAnimationStart = false;     // [추가] 후딜레이(skill_end) 진입 체크용

    float _skillAnimationspeed = 0.65f;
    float _skillSwingAnimationSpeed = 0.8f; // 검이 완성된 후 스킬 휘두르는 애니메이션 속도
    float _skillParticleSpawnTime = 0.25f;      // 0.25초(6프레임) 정지 시점
    float _particleGatherDuration = 2.0f;       // 파티클 모이는 시간
    bool _isSwordGathered = false;              // 다 모였는지 플래그
    float _skillGatherTimer = 0.0f;             // 파티클 타이머
    float _skillEndingAnimationSpeed = 1.0f;    // 마무리 애니메이션 속도

	int _partySlotIndex = -1; // UI 매니저의 파티 슬롯 인덱스


    std::shared_ptr<GameObject> _SkillObject = nullptr;
    std::shared_ptr<GameObject> _particleEffectObject = nullptr;
    void init_skill_variables();

    // 무기 오브젝트 참조 (필요 시)
    std::shared_ptr<GameObject> _currentWeaponObject = nullptr;
    //std::shared_ptr<WeaponScript> _currentWeapon;

};