#pragma once
#include "INetSync.h"
#include "ScriptComponent.h"
#include "AnimationComponent.h"
#include "GameFramework.h"
#include "MonsterHPComponent.h"

class NPCScript : public ScriptComponent, public INetSync {
public:
	NPCScript();
	~NPCScript() override;
	using required_components = std::tuple<TransformComponent, MonsterHPComponent, AnimationComponent, RenderComponent>;

	virtual void init_visual();

	void awake() override;
	void on_destroy() override;
	void update(float deltaTime) override;
	void late_update(float deltaTime) override;

	void set_id(int64_t npc_id);
	void set_npc_type(common::packet::NPCType type) { _npcType = type; } // 추가
	void set_hp(int hp);
	int  get_hp() const { return _hp; }
	void set_position(const XMFLOAT3& position);

	virtual void handle_animation_branching();

	int64_t id() const { return _id; }
	int hp() const { return _hp; }
	const XMFLOAT3& position() const;

	virtual void on_server_update(const common::packet::SC_PACKET_NPC_MOVE& npc_move_packet);
	void initialize_from_server(const common::packet::SC_PACKET_NPC_SPAWN& spawnPkt);

	// --- INetSync 인터페이스 구현 ---
	void on_receive_snapshot(const NetSnapshot& snapshot) override;
	void apply_snapshot() override;
protected:
	int32_t		_hp = 100;
	int64_t		_id = -1;

	common::packet::NPCType		_npcType = common::packet::NPCType::Basic;
	common::packet::EntityState _state = common::packet::EntityState::IDLE;
	int32_t _actionId = -1;
	int64_t _grabbedById = -1; // [추가]
	int8_t  _grabSlot = -1;    // [추가]
	// --- 동기화 변수 ---
	XMFLOAT3 _serverPos = { 0, 0, 0 };      // 서버 기준 위치
	XMFLOAT3 _serverVel = { 0, 0, 0 };      // 서버 기준 속도
	XMFLOAT4 _serverRot = { 0, 0, 0, 1 };   // 서버 기준 회전
	
	float _accumulatedTime = 0.0f;          // 패킷 수신 후 경과 시간
	bool _isFirstUpdate = true;             // 첫 패킷인지 여부

	NetSnapshot _pendingSnapshot;
	bool _isNewDataArrived = false;
};