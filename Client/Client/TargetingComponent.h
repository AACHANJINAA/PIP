#pragma once
#include "Behavior.h"

class GameObject;

class TargetingComponent : public Behavior
{
private:
    int64_t _currentTargetId = -1;
    bool _isLockedOn = false;

    float _maxDistance = 20.0f;    // 탐색 최대 거리
    float _targetingFov = 60.0f;   // 탐색 시야각

public:
	TargetingComponent() : Behavior("TargetingComponent") {} 
    void update(float deltaTime) override;

    int64_t current_target_id() const { return _currentTargetId; }
    bool is_locked_on() const { return _isLockedOn; }

    void toggle_lock_on();

    // 유효한 타겟인지 체크 (거리, 사망 여부 등)
    bool is_valid_target(int64_t id);

private:
    void find_best_target();
};

