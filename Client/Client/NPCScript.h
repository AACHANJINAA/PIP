#pragma once
#include "ScriptComponent.h"

class NPCScript : public ScriptComponent {
public:
	using required_components = std::tuple<TransformComponent>;

	virtual void awake() override;
	virtual void update(float deltaTime) override;
	virtual void late_update(float deltaTime) override;


	void set_position(const f3& position);
	const f3& position() const;
	void set_hp(int hp) { _hp = hp; }
	int hp() const { return _hp; }
	void set_id(int64_t npc_id) { _id = npc_id; }
	int64_t id() const { return _id; }

private:
	int		_hp{};
	int64_t _id{};
};
