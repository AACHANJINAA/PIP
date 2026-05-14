#pragma once
#include "BehaviorTree.h"
#include "GameObject.h"
#include "CharacterControllerComponent.h"
#include "CombatDef.h"
#include "TransformComponent.h"
#include "MapDataManager.h"



namespace PIP::GAME
{
    class Condition_IsHitted : public Condition
    {
    public:
		bool check() override;
    };
    class Condition_IsAlive : public Condition
    {
    public:
		bool check() override;
    };

	class Condition_HasTarget : public Condition
	{
	public:
        bool check() override;
	};

    // [행동] 랜덤 타겟 찾기
    class Action_FindRandomTarget : public Action {
		float _range;
    public:
        Action_FindRandomTarget(float range = 500.0f) : _range{range}{}
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
        Condition_IsEnemyInRange(float range) : _range(range) { }
        bool check() override;
    };

    // [행동] 적 추격
    class Action_ChaseEnemy : public Action {
        float _speed;
        float _stopRange;
    public:
        Action_ChaseEnemy(float speed, float stopRange) : _speed(speed), _stopRange{ stopRange } {}
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };

    // [행동] 적 공격
    class Action_AttackEnemy : public Action {
        float _hitTimer = 0.0f;
        float _timer = 0.0f;
        float _attackDurationTimer = 0.0f;
        bool  _hasAttacked = false; // 중복 판정 방지 플래그
        AttackConfig _config;
    public:
        Action_AttackEnemy(const AttackConfig& config) : _config(config) {}
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };

    class Condition_CheckFlagFalse : public Condition {
        std::string _flagName;
    public:
        Condition_CheckFlagFalse(std::string name) : _flagName(std::move(name)) { set_name("Condition_CheckFlagFalse"); }
        bool check() override;
    };

    class Action_SetFlagTrue : public Action {
        std::string _flagName;
    public:
        Action_SetFlagTrue(std::string name) : _flagName(std::move(name)) { set_name("Action_SetFlagTrue"); }
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };

    class Condition_IsPhase : public Condition
    {
    public:
        Condition_IsPhase(TainerPhase targetPhase) : _targetPhase(targetPhase) { set_name("Condition_IsPhase"); }

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
        Condition_IsEnemyInDistanceRange(float min, float max) : _min(min), _max(max) { set_name("Condition_IsEnemyInDistanceRange"); }

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
		Action_RotateToEnemy() { set_name("Action_RotateToEnemy"); }
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

    class Action_Turn : public Action
	{
        float _timer = 0.0f;
        float _duration = 1.0f;
    public:
        Action_Turn(float duration = 1.0f) : _duration(duration) { set_name("Action_Turn"); }
		NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };
    class Action_SettingChargeTargetPos : public Action 
	{
    public:
		Action_SettingChargeTargetPos() { set_name("Action_SettingChargeTargetPos"); }
		NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };
    class Action_ChargeToPosition : public Action 
	{
        float _speed;
        AttackConfig _config;
    public:
        Action_ChargeToPosition(float speed, const AttackConfig& config) : _speed(speed), _config(config) { set_name("Action_ChargeToPosition"); }
		NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };

    class Action_TargetingNearestPlayer : public Action
    {
    public:
		Action_TargetingNearestPlayer() { set_name("Action_TargetingNearestPlayer"); }
		NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };

    class Action_Roar : public Action
    {   
        float _timer{ 0.0f };
		float _duration{ 1.0f };
	public:
        Action_Roar(float duration = 1.0f) : _duration(duration) { set_name("Action_Roar"); }
		NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };

    // [행동] NavMesh 길찾기 수행
    class Action_FindPath : public Action {
        std::string _navName;
    public:
        Action_FindPath(std::string_view navName) : _navName(navName) { set_name("Action_FindPath"); }
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };

    // [행동] 경로 따라가기
    class Action_FollowPath : public Action {
        float _speed;
    public:
        Action_FollowPath(float speed) : _speed(speed) { set_name("Action_FollowPath"); }
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    };

    class Action_ChargeAttack : public Action 
	{
    public:
        // 돌진의 세부 단계 정의
        enum class Phase { READY, ROAR, TURN, DASHING };

        Action_ChargeAttack(float speed, const AttackConfig& config)
            : _speed(speed), _config(config)
        {}

        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;
    private:
        float _speed;
        AttackConfig _config;

        Phase _currentPhase = Phase::READY; // 현재 단계
        float _internalTimer = 0.0f;        // 단계별 대기 시간용
        bool  _isTargetLocked = false;      // 10m 지점 박제 여부
		float _cooldownTimer = 0.0f;        // 재사용 대기시간 타이머
        common::Vec3 _dashDir = { 0, 0, 0 }; // [추가] 고정된 돌진 방향 저장용
    };

    // [신규] 돌진 후 잡고 다니는 패턴 
    class Action_GrabCharge : public Action
    {
    public:
        enum class Phase { READY, ROAR, TURN, DASHING, CARRYING, SLAM };

        Action_GrabCharge(float speed, const AttackConfig& config)
            : _speed(speed), _config(config)
        {}

        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override;

    private:
        float _speed;
        AttackConfig _config;

        Phase _currentPhase = Phase::READY;
        float _internalTimer = 0.0f;
        bool  _isTargetLocked = false;
        float _cooldownTimer = 0.0f;
        common::Vec3 _dashDir = { 0, 0, 0 };
        std::vector<int64_t> _grabbedPlayerIds; // [수정] 최대 2명
        float _damageTimer = 0.0f; // [추가] DoT 타이머
    };
}
