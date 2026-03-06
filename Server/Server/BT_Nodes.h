#pragma once
#include "BehaviorTree.h"
#include "GameObject.h"
#include "CharacterControllerComponent.h"
#include "CombatDef.h"
#include "TransformComponent.h"
#include "MapDataManager.h"



namespace PIP::GAME
{
	class Condition_HasTarget : public Condition
	{
	public:
        bool check() override;
	};

    // [행동] 랜덤 타겟 찾기
    class Action_FindRandomTarget : public Action {
    public:
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };

    // [행동] 목표로 이동 (끼임 감지 포함)
    class Action_MoveToTarget : public Action {
        float _speed;
    public:
        Action_MoveToTarget(float speed) : _speed(speed) {}

        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };

    // [조건] 타겟 적이 있는가?
    class Condition_HasEnemy : public Condition {
    public:
        bool check() override;
    };

    // [조건] 타겟이 공격 사거리 내에 있는가?
    class Condition_IsEnemyInRange : public Condition {
        float _range;
    public:
        Condition_IsEnemyInRange(float range) : _range(range) {}
        bool check() override;
    };

    // [행동] 적 추격
    class Action_ChaseEnemy : public Action {
        float _speed;
    public:
        Action_ChaseEnemy(float speed) : _speed(speed) {}
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };

    // [행동] 적 공격
    class Action_AttackEnemy : public Action {
        float _timer = 0.0f;
        float _attackDurationTimer = 0.0f;
        NPCAttackConfig _config;
    public:
        Action_AttackEnemy(NPCAttackConfig config) : _config(config) {}
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };
    class Condition_IsPhase : public Condition
    {
    public:
        Condition_IsPhase(TainerPhase targetPhase) : _targetPhase(targetPhase) {}

        bool check() override;

    private:
        TainerPhase _targetPhase;
    };

    class Condition_IsHPBelow : public Condition
    {
    public:
        Condition_IsHPBelow(float ratio) : _ratio(ratio) {}

        bool check() override;

    private:
        float _ratio;
    };

    class Condition_IsEnemyInDistanceRange : public Condition
    {
    public:
        Condition_IsEnemyInDistanceRange(float min, float max) : _min(min), _max(max) {}

        bool check() override;

    private:
        float _min, _max;
    };

    class Action_PlayBossAnimation : public Action
    {
    public:
        Action_PlayBossAnimation(const std::string& animKey) : _animKey(animKey) {}

        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;

    private:
        std::string _animKey;
    };

    class Action_RotateToEnemy : public Action
    {
    public:
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };

    class Action_SetPhase : public Action
    {
    public:
        Action_SetPhase(TainerPhase nextPhase) : _nextPhase(nextPhase) {}

        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;

    private:
        TainerPhase _nextPhase;
    };
}
