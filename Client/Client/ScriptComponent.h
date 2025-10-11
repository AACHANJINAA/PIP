#pragma once
#pragma once
#include "Behaviour.h"
#include "TransformComponent.h"

class ScriptComponent : public Behaviour
{
public:
    ScriptComponent();
    virtual ~ScriptComponent() = default;

    // --- 편의 기능 ---
    TransformComponent* transform() const; // GameObject의 TransformComponent를 쉽게 가져오는 함수

    // --- 메시지/이벤트 시스템 ---
    // 이 함수들은 특정 이벤트 발생 시 엔진 시스템(물리, 메시징 등)에 의해 호출됩니다.
    virtual void on_message(const std::string& message, void* payload = nullptr) {}
    virtual void on_trigger_enter(GameObject* other) {}
    virtual void on_trigger_stay(GameObject* other) {}
    virtual void on_trigger_exit(GameObject* other) {}
};