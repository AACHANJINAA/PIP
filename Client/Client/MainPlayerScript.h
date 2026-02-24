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
    void set_position(const f3& pos)
    {
		auto transform = this->transform();
        if (transform)
        {
            transform->set_local_position(pos);
		}
	}
	const f3& position() const { return this->transform()->local_position(); }

    void set_id(int64_t id) { _playerId = id; }
    void set_hp_bar_ui(UIRenderComponent* ui) { _hpBarUI = ui; }
	int64_t id() const { return _playerId; }

	void apply_knockback(const common::Vec3& force) { _impactVelocity = force; }
    void sync_with_server(const common::Vec3& pos, const common::Quat& rot);
private:
    void move_pos(common::packet::MOVE_TYPE cmd);

    int _hp;
    int _maxHp{ 100 };
    float _hpBarMaxWidth{ 500.0f };
    UIRenderComponent* _hpBarUI{ nullptr };
    int64_t _playerId;
	RenderComponent* _renderComponent{ nullptr };
	GameObject* _camera{ nullptr };
    std::shared_ptr<GameObject> _attackRangeObject;

    // [추가] 넉백 물리 제어 변수
    common::Vec3 _impactVelocity = { 0,0,0 };
    common::Vec3 _visualOffset = { 0, 0, 0 }; // 보정 오차 저장 변수

    float _speed{5.f};
    float _sendTimer{ 0.f };
};
