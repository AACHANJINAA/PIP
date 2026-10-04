#pragma once
#include "RenderComponent.h"
#include "ScriptComponent.h"
#include "AnimationComponent.h"
#include "SocketComponenet.h"
#include "SnapshotBuffer.h"

class OtherPlayerScript : public ScriptComponent
{
public:
	using required_components = std::tuple<RenderComponent, AnimationComponent, SocketComponent>;
    OtherPlayerScript() = default;
    virtual ~OtherPlayerScript() = default;

    virtual void update(float deltaTime) override;

    void awake() override;

    // 서버 이동 패킷: 위치·회전은 보간 버퍼에, 상태·액션·잡기·HP·MP는 그 서버 시각에 적용하도록 대기열에 쌓음
    void on_server_move(const common::packet::SC_PACKET_MOVE& packet);
    // 생성·부활처럼 서버가 위치를 정해 주는 경우: 쌓인 표본을 버리고 이 위치·회전(서버 기준)에서 다시 시작
    void reset_transform(const XMFLOAT3& position, const XMFLOAT4& server_rotation);
    void on_sync_mp(int mp) { _mp = mp; } // [추가]
    void reset_state(); // [추가] 리스폰 시 상태 초기화

	void set_party_slot_index(int index) { _partySlotIndex = index; }
	int get_party_slot_index() const { return _partySlotIndex; }

	void set_hp(int hp);
    int hp() const { return _hp; }
    void set_max_hp(int maxHp) { _maxHp = maxHp; } // [추가]
    void set_id(int64_t id) { _playerId = id; }
    int64_t id() const { return _playerId; }
    bool  is_skilling() const { return _isSkilling; }
private:
    int _hp{ 100 };
    int _maxHp{ 100 }; // [추가]
    int _mp{ 100 }; // [추가]
    int64_t _playerId = -1; // [수정] Session ID(0) 와의 충돌 방지를 위해 -1 로 초기화
    common::packet::EntityState _state = common::packet::EntityState::IDLE;
    int32_t _action_id = 0;
    int64_t _grabbedById = -1; // [추가]
    int8_t  _grabSlot = -1;    // [추가]

    // --- 서버 시각 기준 보간 (NetMotionSync_Design_KR.md 5.2) ---
    SnapshotBuffer _positionBuffer;     // 위치·회전 (회전은 모델 정면 보정을 넣은 값)
    struct PendingState
    {
        double time;                    // 서버 시각 (ms)
        common::packet::EntityState state;
        int32_t action_id;
        int64_t grabbed_by_id;
        int8_t  grab_slot;
        int32_t hp;
        int32_t mp;
    };
    std::deque<PendingState> _pendingStates;

    void apply_state(const PendingState& pending);  // 상태·액션(공격음)·잡기·HP·MP
    double render_time() const;                     // 도착 기준 서버 시각 - 플레이어 보간 지연
    static XMFLOAT4 to_visual_rotation(const XMFLOAT4& server_rotation); // 서버와 모델 정면 차이(Y 180도) 보정


	// 공격 관련 변수들
	float _attackAnimationSpeed = 1.8f; // 공격 애니메이션 속도

    // 스킬을 위한 변수들
    bool _isSkilling = false;
    bool _isSkillAnimationStarted = false;      // [추가] 스킬 최초 시작 체크용
    bool _isSkillEndAnimationStart = false;     // [추가] 후딜레이(skill_end) 진입 체크용
    bool _isDashAnimationStarted = false;       // [추가]

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