#include "pch.h"
#include "BT_Nodes.h"

#include "Server.h"

namespace PIP::GAME
{
	NodeStatus Action_MoveToTarget::tick(float dt, JPH::TempAllocator* allocator) {
        GameObject* owner = _blackboard->get<GameObject*>("owner");
        if (!owner) return NodeStatus::FAILURE;
        auto npc = dynamic_cast<NPC*>(owner);

        auto nc = owner->GetComponent<NPCControllerComponent>();
        auto tc = owner->GetComponent<TransformComponent>();
        if (!nc || !tc) return NodeStatus::FAILURE;

        common::Vec3 target = _blackboard->get<common::Vec3>("target_pos");
        common::Vec3 current = tc->GetPosition();

        // 거리 계산
        float dx = target.x - current.x;
        float dz = target.z - current.z;
        float distance = std::sqrt(dx * dx + dz * dz);

        // 1. 정지 판정 (거리가 충분히 가까우면 즉시 정지)
        if (distance < 0.2f) {
            nc->SetVelocity({ 0, 0, 0 });
            _blackboard->set("target_pos", std::any());
            if (npc) npc->SetState(common::packet::OBJECT_STATE::IDLE);
            return NodeStatus::SUCCESS;
        }

        // 2. [핵심] 자연스러운 감속 (Arrival Steering)
		// 목적지 1.5m 전부터 속도를 서서히 줄임
        float slowRadius = 1.5f;
        float currentSpeed = _speed;
        if (distance < slowRadius) {
            currentSpeed = _speed * (distance / slowRadius);
        }

        // 3. [핵심] 오버슈팅 방지 (이번 프레임에 갈 수 있는 최대 거리 제한)
		// 다음 프레임에 타겟을 넘어가지 않도록 속도를 클램핑
        float maxSafeSpeed = distance / dt;
        currentSpeed = std::min(currentSpeed, maxSafeSpeed);

        // 2. 끼임 감지 (Stuck Check)
        float stuckTimer = _blackboard->get<float>("stuck_timer");

        // 이전 프레임 위치 가져오기 (없으면 현재 위치로 초기화)
        common::Vec3 lastPos = current;
        if (_blackboard->has("last_pos")) {
            lastPos = _blackboard->get<common::Vec3>("last_pos");
        }

        float movedDist = static_cast<float>(std::sqrt(std::pow(current.x - lastPos.x, 2) + std::pow(current.z - lastPos.z, 2)));

        // 끼임 감지 기준 수정 (현재 목표 속도의 20%로 비교)
        if (movedDist < (currentSpeed * dt * 0.2f)) {
            // 감속 중이 아닐 때만 끼임 타이머 증가 (목적지 근처 덜덜거림 방지)
            if (distance > slowRadius) stuckTimer += dt;
        }
        else {
            stuckTimer = 0.0f;
        }

        // 상태 업데이트
        _blackboard->set("stuck_timer", stuckTimer);
        _blackboard->set("last_pos", current);

        // 2초 이상 끼임 -> 실패 반환 -> 상위 Selector가 새 타겟 찾음
        if (stuckTimer > 1.0f) {
            _blackboard->set("target_pos", std::any()); // 목표 삭제
            // [추가] 멈췄으니 IDLE
            if (npc) npc->SetState(common::packet::OBJECT_STATE::IDLE);
            return NodeStatus::FAILURE;
        }

        // 실제 속도 적용 (currentSpeed 사용!)
        common::Vec3 vel = { (dx / distance) * currentSpeed, 0, (dz / distance) * currentSpeed };
        nc->SetVelocity(vel);

        if (currentSpeed < 0.1f) {
            npc->SetState(common::packet::OBJECT_STATE::IDLE);
        }
        else {
            npc->SetState(common::packet::OBJECT_STATE::WALK); // 배회는 보통 RUN 대신 WALK 사용
        }

        // 4. 회전 (바라보기)
        if (distance > 0.1f) {
            float angle = std::atan2(vel.x, vel.z);
            DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0, angle, 0);
            common::Vec4 rot;
            XMStoreFloat4((XMFLOAT4*)&rot, q);
            tc->SetRotation(rot);
        }

        return NodeStatus::RUNNING;
    }

	bool Condition_HasEnemy::check() {
        // 1. 키가 있는지 먼저 확인 (필수!)
        if (!_blackboard->has("target_enemy")) return false;

        auto* room = SERVER::Server::Instance()->GetRoom(_blackboard->get<int>("room_id"));
        if (!room) return false;

        int64_t enemy_id = _blackboard->get<int64_t>("target_enemy");
        return room->GetActor(enemy_id) != nullptr;

    }

	bool Condition_IsEnemyInRange::check() {
        auto* room = SERVER::Server::Instance()->GetRoom(_blackboard->get<int>("room_id"));
        if (!room)
        {
            return false;
        }
        int64_t enemy_id = _blackboard->get<int64_t>("target_enemy");
        GameObject* owner = _blackboard->get<GameObject*>("owner");
		GameObject* enemy = room->GetActor(enemy_id);
        if (!owner) return false;
        if (!enemy)
        {
            _blackboard->set("target_enemy", static_cast<int64_t>(0));
            return false;
        }

        auto npc = dynamic_cast<NPC*>(owner);
        float distSq = common::DistanceSq(npc->GetPosition(), dynamic_cast<Actor*>(enemy)->GetPosition());
        return distSq <= (_range * _range);
    }

	NodeStatus Action_ChaseEnemy::tick(float dt, JPH::TempAllocator* allocator) {
        auto* room = SERVER::Server::Instance()->GetRoom(_blackboard->get<int>("room_id"));
        if (!room)
        {
            return NodeStatus::FAILURE;
        }
        GameObject* owner = _blackboard->get<GameObject*>("owner");
        int64_t enemy_id = _blackboard->get<int64_t>("target_enemy");

        Actor* enemy = room->GetActor(enemy_id);
        if (!owner || !enemy) return NodeStatus::FAILURE;

        auto npc = dynamic_cast<NPC*>(owner);
        auto targetActor = dynamic_cast<Actor*>(enemy);
        if (!npc || !targetActor) return NodeStatus::FAILURE;

        // GetActor(ID)를 통해 현재 방에 실재하는지 검증
        if (!room || room->GetActor(enemy->GetId()) == nullptr) {
            _blackboard->set("target_enemy", std::any()); // 존재하지 않으므로 타겟 지움
            return NodeStatus::FAILURE;
        }

        auto nc = owner->GetComponent<NPCControllerComponent>();
        auto tc = owner->GetComponent<TransformComponent>();
        if (!nc || !tc) return NodeStatus::FAILURE;

        // 1. 타겟과의 거리 및 방향 계산
        common::Vec3 targetPos = targetActor->GetPosition();
        common::Vec3 currentPos = tc->GetPosition();

        float dx = targetPos.x - currentPos.x;
        float dz = targetPos.z - currentPos.z;
        float distance = std::sqrt(dx * dx + dz * dz);

        // 2. 근접 시 정지 (공격 사거리보다 약간 짧게 설정하여 확실히 접근)
        // 공격 사거리 내에 들어오면 상위 Sequence의 Condition_IsEnemyInRange가
        // 성공하면서 이 노드(Chase)는 중단되고 Attack 노드로 넘어갈 것입니다.
        if (distance < 0.5f) {
            nc->SetVelocity({ 0, 0, 0 });
            npc->SetState(common::packet::OBJECT_STATE::IDLE);
            return NodeStatus::SUCCESS;
        }

        // 3. 속도 계산 (추격은 공격적이므로 slowRadius를 작게 잡음)
        float currentSpeed = _speed;
        float slowRadius = 1.0f; // 1m 전부터 감속 시작
        if (distance < slowRadius) {
            currentSpeed = _speed * (distance / slowRadius);
        }

        // 오버슈팅 방지 (다음 프레임에 타겟을 뚫고 지나가지 않게 함)
        float maxSafeSpeed = distance / dt;
        currentSpeed = std::min(currentSpeed, maxSafeSpeed);

        // 4. 이동 속도 적용
        common::Vec3 dir = { dx / distance, 0, dz / distance };
        nc->SetVelocity(dir * currentSpeed);

        // 3. [핵심] 계산된 속도에 따라 상태(애니메이션) 결정
        if (currentSpeed < 0.1f) {
            npc->SetState(common::packet::OBJECT_STATE::IDLE);
        }
        else if (currentSpeed < _speed * 0.5f) { // 원래 속도의 50% 미만이면 걷기
            npc->SetState(common::packet::OBJECT_STATE::WALK);
        }
        else {
            npc->SetState(common::packet::OBJECT_STATE::WALK);
            // npc->SetState(common::packet::OBJECT_STATE::RUN); //TODO: 추격은 보통 RUN 상태이니깐 클라이언트에서 런 애니메이션 필요
        } 

        // [추가] 현재 공격 애니메이션 재생 중이면 이동 속도를 주지 않음
        if (npc->GetState() == common::packet::OBJECT_STATE::ATTACK) {
            nc->SetVelocity({ 0, 0, 0 });
            return NodeStatus::RUNNING;
        }

        // 5. 부드러운 회전 처리
        if (distance > 0.1f) {
            float angle = std::atan2(dx, dz);
            DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0, angle, 0);
            common::Quat rot{};
            XMStoreFloat4(&rot, q);
            tc->SetRotation(rot);
        }

        // 추격 중에는 계속 RUNNING 반환
        return NodeStatus::RUNNING;
    }

	NodeStatus Action_AttackEnemy::tick(float dt, JPH::TempAllocator* allocator) {
        auto* room = SERVER::Server::Instance()->GetRoom(_blackboard->get<int>("room_id"));
        if (!room)
        {
            return NodeStatus::FAILURE;
        }
        // 1. 쿨타임 체크
        if (_timer > 0) _timer -= dt;
        if (_attackDurationTimer > 0) _attackDurationTimer -= dt;

        // 2. 아직 공격 애니메이션 재생 중이면 무조건 RUNNING
        if (_attackDurationTimer > 0.0f) {
            return NodeStatus::RUNNING;
        }

        // 3. 애니메이션은 끝났는데 쿨타임이 남았다면 실패 리턴 (다른 행동 허용)
        if (_timer > 0.0f) return NodeStatus::FAILURE;

        // 4. 공격 동작 중인 경우 (애니메이션 대기)
        if (_attackDurationTimer > 0) {
            _attackDurationTimer -= dt;
            return NodeStatus::RUNNING; // 아직 공격 중이므로 이동 노드로 못 넘어가게 함
        }
        
        GameObject* owner = _blackboard->get<GameObject*>("owner");
        int64_t target_id = _blackboard->get<int64_t>("target_enemy");
		Actor* target = room->GetActor(target_id);
        if (!owner || !target) return NodeStatus::FAILURE;

        auto npc = dynamic_cast<NPC*>(owner);
        auto targetActor = dynamic_cast<Actor*>(target);
        if (!npc || !targetActor) return NodeStatus::FAILURE;

        // 1. 공격 중에는 이동을 멈춤 (문워크 방지)
        auto nc = owner->GetComponent<NPCControllerComponent>();
        if (nc) nc->SetVelocity({ 0, 0, 0 });

        // 2. 타겟 바라보기 (공격 방향 정렬)
        common::Vec3 targetPos = targetActor->GetPosition();
        common::Vec3 currentPos = npc->GetPosition();

        // Y축 차이를 무시한 순수 수평 방향 계산
        float dx = targetPos.x - currentPos.x;
        float dz = targetPos.z - currentPos.z;
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist > 0.1f) {
            float angle = std::atan2(dx, dz);
            DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0, angle, 0);
            common::Quat rot{};
            XMStoreFloat4(&rot, q);
            npc->SetRotation(rot);
        }

        // 4. 실제 공격 판정 요청 (Room Job)
        if (room) {
            
            int64_t npcId = npc->GetId(); // ID 미리 따놓기
            room->PushJob([room, npcId, config = _config]() {
                // 실행 시점에 안전하게 다시 찾음
                auto* attacker = room->GetActor(npcId);
                if (attacker) {
                    room->ExecuteActorAction(attacker, config);// config 데이터를 캡처하여 안전하게 전달
                }
            });
        }

        // 5. 상태 설정 (애니메이션 재생용)
        npc->SetState(common::packet::OBJECT_STATE::ATTACK);

        // 6. 타이머 리셋 및 성공 반환
        // [중요] 공격 애니메이션이 재생될 시간(예: 0.8초) 동안 락을 겁니다.
        _attackDurationTimer = 1.5f;
        _timer = _config.cooldown; // 전체 쿨타임 세팅
        return NodeStatus::SUCCESS;
    }
}
