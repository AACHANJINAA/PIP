#pragma once
#include "ScriptComponent.h"

class NPCScript : public ScriptComponent {
public:
	using required_components = std::tuple<TransformComponent>;

	void awake() override;
	void update(float deltaTime) override;
	void late_update(float deltaTime) override;

	void set_id(int64_t npc_id) { _id = npc_id; }
	void set_hp(int hp) { _hp = hp; }
	void set_position(const XMFLOAT3& position);

	int64_t id() const { return _id; }
	int hp() const { return _hp; }
	const XMFLOAT3& position() const;

	void on_server_update(const XMFLOAT3& pos, const XMFLOAT3& vel, const XMFLOAT4& rot, uint32_t timestamp);
	void set_state(const common::packet::OBJECT_STATE& object_state) { _state = object_state; }

private:
	int		_hp = 0;
	int64_t _id = 0;

	// --- 동기화 변수 ---
	XMFLOAT3 _serverPos = { 0, 0, 0 };      // 서버 기준 위치
	XMFLOAT3 _serverVel = { 0, 0, 0 };      // 서버 기준 속도
	XMFLOAT4 _serverRot = { 0, 0, 0, 1 };   // 서버 기준 회전
	
	float _accumulatedTime = 0.0f;          // 패킷 수신 후 경과 시간
	bool _isFirstUpdate = true;             // 첫 패킷인지 여부
	common::packet::OBJECT_STATE _state = common::packet::OBJECT_STATE::IDLE;
};