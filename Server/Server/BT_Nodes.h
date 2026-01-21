#pragma once
#include "BehaviorTree.h"
#include "GameObject.h"
#include "CharacterControllerComponent.h"
#include "TransformComponent.h"
#include "MapDataManager.h"


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

        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override {
            GameObject* owner = _blackboard->get<GameObject*>("owner");
            if (!owner) return NodeStatus::FAILURE;

            auto cc = owner->GetComponent<CharacterControllerComponent>();
            auto tc = owner->GetComponent<TransformComponent>();
            if (!cc || !tc) return NodeStatus::FAILURE;

            common::Vec3 target = _blackboard->get<common::Vec3>("target_pos");
            common::Vec3 current = tc->GetPosition();

            // 거리 계산
            float dx = target.x - current.x;
            float dz = target.z - current.z;
            float distSq = dx * dx + dz * dz;

            // 1. 도착 체크
            if (distSq < 1.0f) {
                cc->SetVelocity({ 0,0,0 });
                _blackboard->set("target_pos", std::any()); // 목표 삭제 (비우기)
                return NodeStatus::SUCCESS;
            }

            // 2. 끼임 감지 (Stuck Check)
            float stuckTimer = _blackboard->get<float>("stuck_timer");

            // 이전 프레임 위치 가져오기 (없으면 현재 위치로 초기화)
            common::Vec3 lastPos = current;
            if (_blackboard->has("last_pos")) {
                lastPos = _blackboard->get<common::Vec3>("last_pos");
            }

            float movedDist = static_cast<float>(std::sqrt(std::pow(current.x - lastPos.x, 2) + std::pow(current.z - lastPos.z, 2)));

            // 예상 이동 거리의 20%도 못 갔으면 끼인 것으로 간주
            if (movedDist < (_speed * dt * 0.2f)) {
                stuckTimer += dt;
            }
            else {
                stuckTimer = 0.0f;
            }

            // 상태 업데이트
            _blackboard->set("stuck_timer", stuckTimer);
            _blackboard->set("last_pos", current);

            // 2초 이상 끼임 -> 실패 반환 -> 상위 Selector가 새 타겟 찾음
            if (stuckTimer > 2.0f) {
                _blackboard->set("target_pos", std::any()); // 목표 삭제
                return NodeStatus::FAILURE;
            }

            // 3. 이동 (Velocity 설정)
            float dist = std::sqrt(distSq);
            common::Vec3 vel;
            vel.x = (dx / dist) * _speed;
            vel.y = 0; // Y축은 물리엔진 중력에 맡김
            vel.z = (dz / dist) * _speed;

            cc->SetVelocity(vel);

            // 4. 회전 (바라보기)
            if (dist > 0.1f) {
                float angle = std::atan2(vel.x, vel.z);
                DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0, angle, 0);
                common::Vec4 rot;
                XMStoreFloat4((XMFLOAT4*)&rot, q);
                tc->SetRotation(rot);
            }

            return NodeStatus::RUNNING;
        }
    };
}
