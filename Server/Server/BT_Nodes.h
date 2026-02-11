#pragma once
#include "BehaviorTree.h"
#include "GameObject.h"
#include "CharacterControllerComponent.h"
#include "TransformComponent.h"
#include "MapDataManager.h"
#include "NPC.h"


namespace PIP::GAME
{
	class Condition_HasTarget : public Condition
	{
	public:
		bool check() override
		{
			return _blackboard->has("target_pos");
		}
	};

    // [행동] 랜덤 타겟 찾기
    class Action_FindRandomTarget : public Action {
    public:
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override {
            // 임시 맵 범위 (실제로는 MapData에서 가져오는 게 좋음)
            auto mapData = MapDataManager::Instance()->GetTerrainData();

            auto max_x = mapData.GetInfo().max_x;
			auto min_x = mapData.GetInfo().min_x;
			auto max_z = mapData.GetInfo().max_z;
			auto min_z = mapData.GetInfo().min_z;

			float x_range = max_x - min_x;
			float z_range = max_z - min_z;

			float tx = rand() % static_cast<int>(x_range) + min_x;
			float tz = rand() % static_cast<int>(z_range) + min_z;

            // 지형 높이 보정 (Y좌표)
            common::Vec3 targetPos = MapDataManager::Instance()->AdjustPositionToGround({ tx, 50, tz });

            _blackboard->set("target_pos", targetPos);
            _blackboard->set("stuck_timer", 0.0f); // 타이머 리셋

            return NodeStatus::SUCCESS;
        }
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
}
