#include "pch.h"
#include "BT_Nodes.h"

#include "Server.h"
#include "Tainer.h"

namespace PIP::GAME
{
	bool Condition_HasTarget::check()
	{
		// 1. 배회 목적지(target_pos)가 있는지 확인
		if (!_blackboard->has("target_pos")) return false;

		return true;
	}

	NodeStatus Action_FindRandomTarget::tick(float dt, JPH::TempAllocator* allocator)
	{
		// 나중에 NPC의 현재 위치를 기준으로 일정 반경 내에서 랜덤 타겟을 찾도록 개선 필요할 수도 있음
		// 보스의 경우는 그래야함


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
		if (!_blackboard->has("target_enemy")) return false;
		auto owner = dynamic_cast<Actor*>(_blackboard->get<GameObject*>("owner"));
		int64_t targetId = _blackboard->get<int64_t>("target_enemy");

		int room_id = _blackboard->get<int>("room_id");
		auto room = SERVER::Server::Instance()->GetRoom(room_id);
		auto target = room->GetActor(targetId);

		// 타겟이 이미 나갔거나 죽었다면 실패
		if (!target || target->GetHP() <= 0) return false;

		common::Vec3 p1 = owner->GetPosition();
		common::Vec3 p2 = target->GetPosition();
		
		float dx = p1.x - p2.x;
		float dz = p1.z - p2.z;
		float distSq = dx * dx + dz * dz;

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
		if (!room) return NodeStatus::FAILURE;

		// 1. 타이머 업데이트
		if (_timer > 0.0f) _timer -= dt;
		if (_attackDurationTimer > 0.0f) _attackDurationTimer -= dt;

		// 2. 공격 동작 중인 경우 (애니메이션 대기)
		if (_attackDurationTimer > 0.0f) {
			return NodeStatus::RUNNING; // 아직 공격 애니메이션 중이므로 이동 노드로 못 넘어가게 함
		}

		// 3. 쿨타임 체크 (애니메이션은 끝났는데 쿨타임이 남았다면 실패 리턴하여 다른 행동 허용)
		if (_timer > 0.0f) return NodeStatus::FAILURE;
		
		GameObject* owner = _blackboard->get<GameObject*>("owner");
		int64_t target_id = _blackboard->get<int64_t>("target_enemy");
		Actor* target = room->GetActor(target_id);
		if (!owner || !target) return NodeStatus::FAILURE;

		auto npc = dynamic_cast<NPC*>(owner);
		auto targetActor = dynamic_cast<Actor*>(target);
		if (!npc || !targetActor) return NodeStatus::FAILURE;

		// 공격 중에는 이동을 멈춤
		auto nc = owner->GetComponent<NPCControllerComponent>();
		if (nc) nc->SetVelocity({ 0, 0, 0 });

		// 타겟 바라보기
		common::Vec3 targetPos = targetActor->GetPosition();
		common::Vec3 currentPos = npc->GetPosition();
		float dx = targetPos.x - currentPos.x;
		float dz = targetPos.z - currentPos.z;
		float distSq = dx * dx + dz * dz;

		if (distSq > 0.01f) {
			float angle = std::atan2(dx, dz);
			DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0, angle, 0);
			common::Quat rot{};
			XMStoreFloat4(&rot, q);
			npc->SetRotation(rot);
		}

		// 4. 실제 공격 판정 요청
		int64_t npcId = npc->GetId();
		room->PushJob([room, npcId, config = _config]() {
			auto* attacker = room->GetActor(npcId);
			if (attacker) {
				room->ExecuteActorAction(attacker, config);
			}
		});

		// 5. 상태 설정 및 타이머 세팅
		npc->SetState(common::packet::OBJECT_STATE::ATTACK);
		_attackDurationTimer = 1.0f; // 애니메이션 지속 시간 동안 행동 잠금
		_timer = _config.cooldown; 

		return NodeStatus::SUCCESS;
	}

	bool Condition_IsPhase::check() {
		auto ownerObj = _blackboard->get<GameObject*>("owner");
		auto tainer = dynamic_cast<Tainer*>(ownerObj);

		return (tainer && tainer->GetCurrentPhase() == _targetPhase);
	}

	bool Condition_IsHPBelow::check() {
		auto ownerObj = _blackboard->get<GameObject*>("owner");
		auto maxHP = _blackboard->get<float>("max_hp");
		auto npc = dynamic_cast<NPC*>(ownerObj);
		if (!npc) return false;

		// 보스전의 경우 최대 체력을 블랙보드에 넣어두거나 Tainer 클래스에서 가져옴
		// 여기서는 예시로 5000을 사용하거나 npc->GetMaxHP() (구현되어 있다면)를 사용
		float hpRatio = static_cast<float>(npc->GetHP()) / maxHP;
		return hpRatio <= _ratio;
	}

	bool Condition_IsEnemyInDistanceRange::check() {
		auto owner = dynamic_cast<Actor*>(_blackboard->get<GameObject*>("owner"));
		if (!owner) return false;

		// 블랙보드에서 타겟 ID를 가져옴
		int64_t targetId = _blackboard->get<int64_t>("target_enemy");

		int room_id = _blackboard->get<int>("room_id");
		// 서버의 RoomManager 혹은 Singleton을 통해 타겟 객체 참조
		auto room = SERVER::Server::Instance()->GetRoom(room_id);
		if (!room) return false;

		auto target = room->GetActor(targetId);
		if (!target) return false;

		float dist = common::Distance(owner->GetPosition(), target->GetPosition());
		return (dist >= _min && dist <= _max);
	}

	NodeStatus Action_PlayBossAnimation::tick(float dt, JPH::TempAllocator* allocator) {
		auto owner = dynamic_cast<Actor*>(_blackboard->get<GameObject*>("owner"));
		if (!owner) return NodeStatus::FAILURE;

		// [중요] 클라이언트에 애니메이션 트리거 전송
		// 프로젝트의 패킷 정의에 따라 SC_PACKET_ACTION_NOTIFY 등을 사용
		/*
		packet::SC_PACKET_ACTION_NOTIFY pkt;
		pkt._actor_id = owner->GetId();
		pkt._anim_key = _animKey;
		auto room = Server::Instance()->GetRoom(owner->GetRoomId());
		room->Broadcast(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		*/

		MYLOG("Boss Action: " << _animKey);
		return NodeStatus::SUCCESS;
	}

	NodeStatus Action_RotateToEnemy::tick(float dt, JPH::TempAllocator* allocator) {
		auto owner = dynamic_cast<Actor*>(_blackboard->get<GameObject*>("owner"));
		int64_t targetId = _blackboard->get<int64_t>("target_enemy");

		// 타겟 ID가 없으면 실패 반환 -> 다음 Selector 자식으로 넘어감
		if (targetId <= 0) return NodeStatus::FAILURE;
		int room_id = _blackboard->get<int>("room_id");
		auto room = SERVER::Server::Instance()->GetRoom(room_id);
		auto target = room->GetPlayer(targetId);
		if (!target) return NodeStatus::FAILURE;

		common::Vec3 dir = common::Normalize(target->GetPosition() - owner->GetPosition());
		dir.y = 0;

		if (common::Length(dir) > 0.001f) {
			float angle = std::atan2(dir.x, dir.z);
			DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0, angle, 0);
			common::Quat rot;
			XMStoreFloat4((XMFLOAT4*)&rot, q);
			owner->GetComponent<TransformComponent>()->SetRotation(rot);
		}

		return NodeStatus::SUCCESS;
	}

	NodeStatus Action_SetPhase::tick(float dt, JPH::TempAllocator* allocator) {
		auto ownerObj = _blackboard->get<GameObject*>("owner");
		auto tainer = dynamic_cast<Tainer*>(ownerObj);
		if (tainer) {
			// Tainer 클래스 내부에 SetPhase(TainerPhase)가 구현되어 있어야 함
			tainer->SetPhase(_nextPhase);
			return NodeStatus::SUCCESS;
		}
		return NodeStatus::FAILURE;
	}
}
