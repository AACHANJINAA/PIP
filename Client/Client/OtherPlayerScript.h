#pragma once
#include "RenderComponent.h"
#include "ScriptComponent.h"
#include "AnimationComponent.h"

class OtherPlayerScript : public ScriptComponent
{
public:
	using required_components = std::tuple<RenderComponent, AnimationComponent>;
    OtherPlayerScript() = default;
    virtual ~OtherPlayerScript() = default;

    virtual void update(float deltaTime) override;

    void awake() override;

    // 서버로부터 위치 동기화 패킷을 받았을 때 호출될 함수 (예시)
    void on_sync_position(const XMFLOAT3& newPosition);
	void on_sync_rotation(const XMFLOAT4& newRotation);
    void on_sync_state(common::packet::EntityState state);
    void on_sync_action_id(int32_t action_id);

	void set_hp(int hp) { _hp = hp; }
    int hp() const { return _hp; }
	void set_id(int64_t id) { _playerId = id; }
	int64_t id() const { return _playerId; }
private:
    int _hp;
	int64_t _playerId;
	common::packet::EntityState _state;
    int32_t _action_id = 0;
    // --- [추측 항법 및 보간용 변수] ---
    common::Vec3    _logicalPosition;   // 서버가 알려준 최신 논리적 위치
    common::Vec3    _visualOffset;      // 시각적 보간을 위한 오프셋 (이전 위치와의 차이)
    common::Vec3    _velocity;          // 추측 항법을 위한 속도 (옵션)

    float           _lerpFactor = 15.0f; // 보간 속도 (수치가 클수록 서버 위치에 빨리 도달)
};