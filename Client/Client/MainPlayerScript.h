#pragma once
#include "ScriptComponent.h"

constexpr float SENDINTERVAL{ 0.02f };
class RenderComponent;
class MainPlayerScript : public ScriptComponent
{
public:
	using required_components = std::tuple<RenderComponent>;

    MainPlayerScript() = default;
    virtual ~MainPlayerScript() = default;

    virtual void update(float deltaTime) override;
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
	int64_t id() const { return _playerId; }

	void apply_knockback(const common::Vec3& force) { _impactVelocity = force; }

private:
    void move_pos(common::packet::MOVE_TYPE cmd);

    int _hp;
    int64_t _playerId;
	RenderComponent* _renderComponent{ nullptr };
	GameObject* _camera{ nullptr };
    std::shared_ptr<GameObject> _attackRangeObject;

    // [추가] 넉백 물리 제어 변수
    common::Vec3 _impactVelocity = { 0,0,0 };

    float _speed{5.f};
    float _sendTimer{ 0.f };
};
