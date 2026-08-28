#pragma once
#include "ScriptComponent.h"
#include "RenderComponent.h"                        // [추가] 튜플에 사용하려면 전체 정의가 필요합니다.
#include "AnimationComponent.h"
#include "SocketComponenet.h"
#include "TargetingComponent.h"
#include "UIRenderComponent.h"  
#include "WeaponScript.h"
constexpr float SENDINTERVAL{ 0.02f };
class MainPlayerScript : public ScriptComponent
{
public:
	using required_components = std::tuple<RenderComponent, AnimationComponent, SocketComponent, TargetingComponent>;

	MainPlayerScript() = default;
	~MainPlayerScript() override = default;

	void update(float deltaTime) override;
	void fixed_update(float deltaTime) override;
	void awake() override;

	void set_hp(int hp);
	int hp() const { return _hp; }
	
	void set_max_hp(int max_hp) { _maxHp = max_hp; }
	int max_hp() const { return _maxHp; }

	void set_attack_damage(int dmg) { _attackDamage = dmg; }
	int attack_damage() const { return _attackDamage; }

	void set_mp(int mp);
	bool is_skilling() const { return _isSkilling; }
	
	int mp() const { return _mp; } // [추가]
	void set_position(const f3& pos) const
	{
		if (auto transform = this->transform())
		{
			transform->set_local_position(pos);
		}
	}
	const f3& position() const { return this->transform()->local_position(); }

	void set_yaw(float yaw) { _currentyaw = yaw; }

	void set_id(int64_t id) { _playerId = id; }
	void set_mp_bar_ui(std::shared_ptr<UIRenderComponent> ui) {
		_mpBar_ui = ui;
		if (ui) {
			float width = ui->get_size_x();
			if (width > 0) _mpBar_maxWidth = width;
		}
	}
	
	// 퀘스트 배너 노출
	void show_reward_banner();

	void set_hp_bar_ui(std::shared_ptr<UIRenderComponent> ui) { 
		_hpBar_ui = ui;
		if (ui) {
			float width = ui->get_size_x();
			if (width > 0) _hpBar_maxWidth = width;
		}
	}

	int64_t id() const { return _playerId; }

	/*void apply_knockback(const common::Vec3& force) { _impactVelocity = force; }*/
	void sync_with_server(const common::packet::SC_PACKET_MOVE& movePacket);
	void reset_state(); // [추가] 리스폰 시 상태 초기화
	void update_quest_ui(float deltaTime); // 퀘스트 UI 업데이트

private:
	// --- update 기능 분리용 private 함수 ---
	void update_hp_bar(float deltaTime);
	void update_mp_bar(float deltaTime);
	void handle_state(float deltaTime);
	void handle_input(float deltaTime);
	void update_physics_and_visuals(float deltaTime);
	void send_network_sync(float deltaTime);

	// ui 띄우기 용
	void die_ui_update(float deltaTime);

	// 스킬 비주얼 함수
	void update_skill_visuals(float deltaTime);

	// 실제 공격 판정과 패킷 전송을 처리하는 함수 (애니메이션 프레임에 맞춰 호출)
	void process_attack_and_packet(); // 애니메이션 시간은 내부에서 직접 구함

	// 무기 오브젝트 참조 (필요 시)
	std::shared_ptr<GameObject> _currentWeaponObject = nullptr;
	std::shared_ptr<WeaponScript> _currentWeapon;

	int32_t _hp{ 100 };
	int32_t _maxHp{ 100 };
	int32_t _attackDamage{ 10 }; // 기본 데미지 임시 세팅
	float _displayHp{ 100.0f };          // <- 추가 (lerp용 표시 HP)
	float _hpBar_maxWidth{ 100.0f };
	std::shared_ptr<UIRenderComponent> _hpBar_ui{ nullptr };
	
	float _displayMp{ 100.0f };          // lerp용 (부드러운 이동)
	float _mpBar_maxWidth{ 100.0f };      // 바의 최대 길이를 저장할 변수
	std::shared_ptr<UIRenderComponent> _mpBar_ui{ nullptr }; // 실제 UI 컴포넌트

	// 퀘스트 배너 표시 타이머
	float _rewardBannerTimer{ 0.0f };

	int32_t _mp{ 100 };                  // [추가]
	int32_t _maxMp{ 100 };               // [추가]
	int64_t _playerId;
	RenderComponent* _renderComponent	{ nullptr };
	GameObject* _camera					{ nullptr };
	std::shared_ptr<GameObject> _attackRangeObject;

	// [추가] 넉백 물리 제어 변수
	common::Vec3 _visualOffset = { 0, 0, 0 }; // 보정 오차 저장 변수
	common::Vec3 _logicalPosition = { 0, 0, 0 }; // 서버와 동기화되는 실제 예측 좌표
	common::Quat _logicalRotation = { 0, 0, 0, 1 }; // 서버와 동기화되는 실제 예측 회전 (쿼터니언)
	float _verticalVelocity = 0.0f;              // 수직 속도 (낙하용)
	bool _isGrounded = true;                     // 접지 상태 (임시)


	float _speed{ 5.f };
	float _sendTimer{ 0.f };

	// 공격 관련 변수들
	bool _isAttacking = false;
	float _attackAnimationSpeed = 1.8f; // 공격 애니메이션 속도


	// 스킬을 위한 변수들
	bool _isSkilling = false;   // 스킬 사용 중인지 여부
	bool _isSkillAnimationStarted = false; // 스킬 애니메이션이 시작되었는지 여부
	bool _isSkillEndAnimationStart = false;   // 스킬 종료 애니메이션 시작 했는지 여부
	float _nowSkillTime = 0.0f;    // 스킬 사용 시작 시점부터의 경과 시간
	
	float _skillAnimationspeed = 0.65f; // 스킬 애니메이션 속도
	float _skillParticleSpawnTime = 0.25f;       // 파티클 생성 시작 시간 및 애니메이션 멈추는 시간(6프레임)
	float _particleGatherDuration = 2.f;         // 파티클이 흩어져 있다가 100% 모이는 데 걸리는 시간
	bool _isSwordGathered = false; // 대검이 모였는지 여부
	float _skillGatherTimer = 0.0f; // 대검이 모이는 시간 측정용 타이머

	bool _isSkillEnd = false;   // 아예 스킬이 종료했는지 여부 //스킬 종료 했는지 여부 마지막 스킬 마무리 애니메이션이 끝났는지 여부
	float _skillEndingAnimationSpeed = 1.f; // 스킬이 끝나는 애니메이션의 속도

	float _skillSwingAnimationSpeed = 0.8f; // 검이 완성된 후 스킬 휘두르는 애니메이션 속도


	std::shared_ptr<GameObject> _SkillObject = nullptr;
	std::shared_ptr<GameObject> _particleEffectObject = nullptr;
	void init_skill_variables();




	bool _isChargingSkill = false;   // [추가]
	float _skillChargeTimer = 0.0f;  // [추가]
	bool _packetSent = false;
	common::Vec3 _currentMoveDir = { 0,0,0 };
	// [추가] 서버와 동일한 보간용 현재 속도 변수
	common::Vec3 _currentVelocity = { 0, 0, 0 };

	// 방향 회전을 위한 변수 추가
	float _currentyaw = 0.0f; // 캐릭터의 현재 Yaw (회전) 값

	common::packet::EntityState _state = common::packet::EntityState::IDLE;
	int32_t _actionId = 0;

	// [추가] 대쉬 상태 변수
	bool _isDashing = false;
	int32_t _dashActionId = 0;
	float _dashCooldownTimer = 0.0f; // [추가] 대쉬 쿨타임

	int64_t _grabbedById = -1; // [추가]
	int8_t  _grabSlot = -1;    // [추가]
	float _timer = 0.0f;

	// --- 퀘스트 "도와줘!!" 연출용 변수 ---
	bool _wasQuestActive = false;
	bool _isHelpMeShowing = false;
	float _helpMeAlpha = 0.0f;
	float _helpMeFadeSpeed = 1.0f; // 페이드 아웃 속도 (1.0f일 때 1초 동안 사라짐)
	std::shared_ptr<UIRenderComponent> _helpMeUI{ nullptr };

	// --- 퀘스트 스토리 UI 관련 변수 ---
	float _qAutoToggleTimer = -1.0f;
	common::packet::QuestState _prevQuest1State = common::packet::QuestState::NONE;
	bool _isQuestStoryShowing = false;
	bool _isQuestStoryFadingOut = false;
	float _questStoryAlpha = 0.0f;
	float _questStoryFadeSpeed = 2.0f; // 페이드 아웃 속도 (2.0f일 때 0.5초 동안 사라짐)

	// --- 조작법 UI 관련 변수 ---
	bool _isControlsUIShowing = false;
	bool _isControlsUIFadingOut = false;
	float _controlsUIAlpha = 0.0f;
	float _controlsUIFadeSpeed = 4.0f; // 페이드 아웃 속도
};
