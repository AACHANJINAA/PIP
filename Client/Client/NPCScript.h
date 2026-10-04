#pragma once
#include "INetSync.h"
#include "SnapshotBuffer.h"
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
	// 내 칼날 예측 적중 순간 (HitFeel_Plan_KR.md 3단계): 종류별 피격음을 바로 재생하고 시각을 기록
	// 이후 서버 HP 감소(set_hp)와 서버 피격 응답에서는 같은 소리를 다시 내지 않음
	void on_predicted_hit();
	bool predicted_hit_recently() const;
	int  get_hp() const { return _hp; }
	void set_position(const XMFLOAT3& position);

	virtual void handle_animation_branching();

	int64_t id() const { return _id; }
	int hp() const { return _hp; }
	const XMFLOAT3& position() const;

	virtual void on_server_update(const common::packet::SC_PACKET_NPC_MOVE& npc_move_packet);
	void initialize_from_server(const common::packet::SC_PACKET_NPC_SPAWN& spawnPkt);

	// 모션 이벤트 (넉백 등, NetMotionSync_Design_KR.md 5.3): 렌더 시각이 구간 안이면 수평 위치를 곡선으로 계산
	void on_motion_start(const common::packet::SC_PACKET_MOTION_START& packet);
	void on_motion_end(const common::packet::SC_PACKET_MOTION_END& packet);

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
	
	bool _isFirstUpdate = true;             // 첫 패킷인지 여부

	// 서버 시각 기준 보간 (위치·회전)과, 서버 시각에 맞춰 적용할 상태 대기열
	SnapshotBuffer _positionBuffer;
	struct PendingState
	{
		double time;
		NetSnapshot snapshot;
	};
	std::deque<PendingState> _pendingStates;

	// 모션 구간. 구간 안의 위치 스냅샷은 수평 위치에 쓰지 않음(높이·회전은 스냅샷 보간)
	struct ActiveMotion
	{
		uint32_t id;
		common::motion::MotionCurve curve;
		double start_time;		// 서버 시각 (ms)
		double end_time;		// 예정 종료 시각, MOTION_END나 다음 모션이 오면 앞당겨짐
		XMFLOAT3 start_pos;
		XMFLOAT3 end_pos;
		float hold;
		float duration;
	};
	std::deque<ActiveMotion> _motions;
	// render_time이 모션 구간 안이면 그 모션의 수평 위치를 pos에 덮어씀
	bool apply_motion(double render_time, XMFLOAT3& pos);

	void apply_state(const NetSnapshot& snapshot);	// 상태·액션·잡기·HP와 그에 따른 사운드
	void play_damage_sound();						// 종류별 피격음 (보스, 매직 컨스트럭트)
	double _lastPredictedHitMs = -1.0e9;			// 마지막 예측 적중 시각 (NetNowMsPrecise)
	static constexpr double kPredictedHitSoundWindowMs = 1000.0; // 이 안에 온 서버 피격은 예측으로 이미 소리를 낸 것으로 봄
	double render_time() const;						// 추정 서버 시각 - 보간 지연
	XMFLOAT4 to_visual_rotation(const common::Quat& server_rot) const;
};