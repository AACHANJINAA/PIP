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

    // �����κ��� ��ġ ����ȭ ��Ŷ�� �޾��� �� ȣ��� �Լ� (����)
    void on_sync_position(const XMFLOAT3& newPosition);
	void on_sync_rotation(const XMFLOAT4& newRotation);
    void on_sync_state(common::packet::EntityState state);
    void on_sync_action_id(int32_t action_id);
    void on_sync_grab(int64_t grabbed_by_id, int8_t grab_slot); // [추가]
    void on_sync_velocity(const common::Vec3& velocity) { _velocity = velocity; } // [추가]
    void on_sync_hp(int hp) { _hp = hp; } // [추가]
    void reset_state(); // [추가] 리스폰 시 상태 초기화

    void set_hp(int hp) { _hp = hp; }    int hp() const { return _hp; }
    void set_id(int64_t id) { _playerId = id; }
    int64_t id() const { return _playerId; }
    private:
    int _hp;
    int64_t _playerId;
    common::packet::EntityState _state;
    int32_t _action_id = 0;
    int64_t _grabbedById = -1; // [추가]
    int8_t  _grabSlot = -1;    // [추가]
    // --- [ ׹   ] ---
    common::Vec3    _logicalPosition;   // ������ �˷��� �ֽ� ������ ��ġ
    common::Vec3    _visualOffset;      // �ð��� ������ ���� ������ (���� ��ġ���� ����)
    common::Vec3    _velocity;          // ���� �׹��� ���� �ӵ� (�ɼ�)

    float           _lerpFactor = 15.0f; // ���� �ӵ� (��ġ�� Ŭ���� ���� ��ġ�� ���� ����)
};