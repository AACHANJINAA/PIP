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
    void on_sync_state(common::packet::OBJECT_STATE state);

	void set_hp(int hp) { _hp = hp; }
    int hp() const { return _hp; }
	void set_id(int64_t id) { _playerId = id; }
	int64_t id() const { return _playerId; }
private:
    int _hp;
	int64_t _playerId;
};

