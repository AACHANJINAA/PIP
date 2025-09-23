#pragma once
#pragma once
#include "Component.h"

class Behaviour : public Component
{
public:
    Behaviour();
    virtual ~Behaviour() = default;

    // --- Getter/Setter (코드 컨벤션 적용) ---
    bool is_enabled() const { return _isEnabled; }
    void set_enabled(bool isEnabled);

    // --- 생명주기 함수 ---
    // 이 함수들은 Script 클래스에서 구체적으로 오버라이드될 것입니다.
    virtual void awake() {}
    virtual void on_enable() {}
    virtual void start() {}
    virtual void update(float deltaTime) {}
    virtual void fixed_update(float deltaTime) {}
    virtual void late_update(float deltaTime) {}
    virtual void on_disable() {}
    virtual void on_destroy() {}

protected:
    bool _isEnabled;
};