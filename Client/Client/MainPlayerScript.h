#pragma once
#include "ScriptComponent.h"
#include "RenderComponent.h"                        // [추가] 튜플에 사용하려면 전체 정의가 필요합니다.
#include "PhysicsCharacterControllerComponent.h"    // [추가] 튜플에 사용하려면 전체 정의가 필요합니다.
#include "AnimationComponent.h"                     // [추가]
#include "UIRenderComponent.h"  
constexpr float SENDINTERVAL{ 0.02f };
class MainPlayerScript : public ScriptComponent
{
public:
	using required_components = std::tuple<RenderComponent, PhysicsCharacterControllerComponent, AnimationComponent>;

	MainPlayerScript() = default;
	virtual ~MainPlayerScript() = default;

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

	void apply_knockback(const common::Vec3& force) { _impactVelocity = force; }
	void sync_with_server(const common::Vec3& pos, const common::Quat& rot);
private:
	// --- update 기능 분리용 private 함수 ---
	void update_hp_bar(float deltaTime);
	void handle_state(float deltaTime);
	void handle_input(float deltaTime);
	void update_physics_and_visuals(float deltaTime);
	void send_network_sync(float deltaTime);


	int _hp{ 100 };
	int _maxHp{ 100 };
	float _displayHp{ 100.0f };          // <- 추가 (lerp용 표시 HP)
	float _hpBar_maxWidth{ 500.0f };
	std::shared_ptr<UIRenderComponent> _hpBar_ui{ nullptr };
	int64_t _playerId;
	RenderComponent* _renderComponent{ nullptr };
	GameObject* _camera{ nullptr };
	std::shared_ptr<GameObject> _attackRangeObject;

	// [추가] 넉백 물리 제어 변수
	common::Vec3 _impactVelocity = { 0,0,0 };
	common::Vec3 _visualOffset = { 0, 0, 0 }; // 보정 오차 저장 변수

	float _speed{5.f};
	float _sendTimer{ 0.f };

	bool _isAttacking = false;
	bool _packetSent = false;
	common::Vec3 _currentMoveDir = { 0,0,0 };
};
