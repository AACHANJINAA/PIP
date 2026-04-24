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
	using required_components = std::tuple<RenderComponent, AnimationComponent, SocketComponenet, TargetingComponent>;

	MainPlayerScript() = default;
	~MainPlayerScript() override = default;

	void update(float deltaTime) override;
	void fixed_update(float deltaTime) override;
	void awake() override;

	void set_hp(int hp);
	int hp() const { return _hp; }
	void set_position(const f3& pos) const
	{
		if (auto transform = this->transform())
		{
			transform->set_local_position(pos);
		}
	}
	const f3& position() const { return this->transform()->local_position(); }

	void set_id(int64_t id) { _playerId = id; }
	void set_hp_bar_ui(std::shared_ptr<UIRenderComponent> ui) { 
		_hpBar_ui = ui;
		if (ui) {
			float width = ui->get_size_x();
			// [중요] 만약 width가 0이라면 아직 초기화 전이므로,
			// 기본값을 주거나 나중에 다시 가져오도록 로그를 찍어보세요.
			if (width > 0) _hpBar_maxWidth = width;

			//CLOG("[UI] HP Bar Linked. Max Width: " << _hpBar_maxWidth);
		}
	}
	int64_t id() const { return _playerId; }

	/*void apply_knockback(const common::Vec3& force) { _impactVelocity = force; }*/
	void sync_with_server(const common::packet::SC_PACKET_MOVE& movePacket);
private:
	// --- update 기능 분리용 private 함수 ---
	void update_hp_bar(float deltaTime);
	void handle_state(float deltaTime);
	void handle_input(float deltaTime);
	void update_physics_and_visuals(float deltaTime);
	void send_network_sync(float deltaTime);

	// ui 띄우기 용
	void die_ui_update(float deltaTime);

	// 무기 오브젝트 참조 (필요 시)
	std::shared_ptr<GameObject> _currentWeaponObject = nullptr;
	std::shared_ptr<WeaponScript> _currentWeapon;

	int32_t _hp{ 100 };
	int32_t _maxHp{ 100 };
	float _displayHp{ 100.0f };          // <- 추가 (lerp용 표시 HP)
	float _hpBar_maxWidth{ 100.0f };
	std::shared_ptr<UIRenderComponent> _hpBar_ui{ nullptr };
	int64_t _playerId;
	RenderComponent* _renderComponent	{ nullptr };
	GameObject* _camera					{ nullptr };
	std::shared_ptr<GameObject> _attackRangeObject;

	// [추가] 넉백 물리 제어 변수
	common::Vec3 _visualOffset = { 0, 0, 0 }; // 보정 오차 저장 변수
	common::Vec3 _logicalPosition = { 0, 0, 0 }; // 서버와 동기화되는 실제 예측 좌표
	float _verticalVelocity = 0.0f;              // 수직 속도 (낙하용)
	bool _isGrounded = true;                     // 접지 상태 (임시)


	float _speed{ 5.f };
	float _sendTimer{ 0.f };

	bool _isAttacking = false;


	// 스킬을 위한 변수들
	bool _isSkilling = false;   // [추가] 스킬 사용 중인지 여부
	float _nowSkillTime = 0.0f;    // [추가] 스킬 사용 시작 시점부터의 경과 시간
	
	float skillAnimationspeed = 0.65; // 스킬 애니메이션 속도
	float _skillBigSowrdSpawn = 0.95 * (1.f/skillAnimationspeed); // 대검 생성 시점
	float _skillDontFollowAnimationTime = 1.095f * (1.f / skillAnimationspeed); // 대검 안따라가는 시점
	std::shared_ptr<GameObject> _SkillObject = nullptr;
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
	float _timer = 0.0f;
};
