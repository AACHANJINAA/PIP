#pragma once
#pragma once
#include "Behavior.h"
#include "TransformComponent.h"

class PhysicsColliderComponent;

// 검사 전용 콜라이더(공격 판정)가 다른 콜라이더(피격 판정)와 겹쳤을 때 전달되는 정보
struct TriggerHit
{
    PhysicsColliderComponent* self = nullptr;   // 이 오브젝트의 콜라이더 (예: 칼날)
    PhysicsColliderComponent* other = nullptr;  // 상대 콜라이더 (예: NPC 히트박스)
    XMFLOAT3 point = { 0, 0, 0 };               // 적중 지점 (월드)
    XMFLOAT3 direction = { 0, 0, 0 };           // 적중 지점에서 공격 콜라이더가 움직인 방향 (정규화)

    // 공격 콜라이더가 캡슐이면 이번 프레임에 쓸고 지나간 구간 (직전·현재 자세의 캡슐 중심선 양 끝, 월드). 메쉬 단위 판정용
    bool has_self_segment = false;
    XMFLOAT3 self_prev_a = { 0, 0, 0 }, self_prev_b = { 0, 0, 0 };
    XMFLOAT3 self_cur_a = { 0, 0, 0 }, self_cur_b = { 0, 0, 0 };
    float self_radius = 0.0f;
};

class ScriptComponent : public Behavior
{
public:
    ScriptComponent();
	ScriptComponent(const std::string& name) : Behavior(name) {}
    virtual ~ScriptComponent() = default;

    // --- 편의 기능 ---
    TransformComponent* transform() const; // GameObject의 TransformComponent를 쉽게 가져오는 함수

    //// --- 메시지/이벤트 시스템 ---
    //// 이 함수들은 특정 이벤트 발생 시 엔진 시스템(물리, 메시징 등)에 의해 호출됩니다.
    virtual void on_message(const std::string& message, void* payload = nullptr) {}
    virtual void on_collision_enter(std::shared_ptr<GameObject> other) {}
    virtual void on_collision_stay(std::shared_ptr<GameObject> other) {}
    virtual void on_collision_exit(std::shared_ptr<GameObject> other) {}
    // 공격 판정 콜라이더가 켜져 있는 동안 상대마다 한 번 호출 (Unity의 OnTriggerEnter와 비슷)
    virtual void on_trigger_enter(std::shared_ptr<GameObject> other, const TriggerHit& hit) {}
};