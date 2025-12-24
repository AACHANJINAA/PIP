#pragma once
#include "ScriptComponent.h"
struct InterpolatedNPCState {
	common::Vec3 position{};
	common::Vec3 velocity{};
	common::Quat rotation{};
	uint32_t	 timestamp{};
	uint32_t	 client_receive_time{};
};
class NPCScript : public ScriptComponent {
public:
	using required_components = std::tuple<TransformComponent>;

	void awake() override;
	void update(float deltaTime) override;
	void late_update(float deltaTime) override;

	void set_id(int64_t npc_id) { _id = npc_id; }
	void set_hp(int hp) { _hp = hp; }
	void set_position(const f3& position);

	int64_t id() const { return _id; }
	int hp() const { return _hp; }
	const f3& position() const;

	void on_server_update(const f3& pos, const f3& vel, const common::Quat& rot, uint32_t timestamp);
private:
	int		_hp{};
	int64_t _id{};

	InterpolatedNPCState _currentTargetState{};
	InterpolatedNPCState _prevTargetState{};

	float _interpolateTimer = 0.0f;
	float _interpolateDuration = 0.2f;
	bool  _hasReceivedFirstPacket = false;
};
