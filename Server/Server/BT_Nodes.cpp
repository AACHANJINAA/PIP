#include "pch.h"
#include "BT_Nodes.h"

#include "Server.h"
#include "Tainer.h"

namespace PIP::GAME
{
	using namespace common::packet;
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
			if (npc)
			{
				npc->SetState(common::packet::EntityState::IDLE);
				npc->SetActionId(0);
			}
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
			if (npc)
			{
				npc->SetState(common::packet::EntityState::IDLE);
				npc->SetActionId(0);
			}
			return NodeStatus::FAILURE;
		}

		// 실제 속도 적용 (currentSpeed 사용!)
		common::Vec3 vel = { (dx / distance) * currentSpeed, 0, (dz / distance) * currentSpeed };
		nc->SetVelocity(vel);

		if (currentSpeed < 0.1f) {
			npc->SetState(common::packet::EntityState::IDLE);
			npc->SetActionId(0);
		}
		else {
			npc->SetState(common::packet::EntityState::MOVE); // 배회는 보통 RUN 대신 WALK 사용
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
		auto owner = dynamic_cast<NPC*>(_blackboard->get<GameObject*>("owner"));
		int64_t targetId = _blackboard->get<int64_t>("target_enemy");

		auto state = owner->GetState();
		if (common::packet::EntityState::ACTION == state)
		{
			return true;
		}


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
		float distSq = dx * dx + dz * dz;
		float distance = std::sqrt(distSq);

		// 2. 근접 시 정지 (공격 사거리보다 약간 짧게 설정하여 확실히 접근)
		// 공격 사거리 내에 들어오면 상위 Sequence의 Condition_IsEnemyInRange가
		// 성공하면서 이 노드(Chase)는 중단되고 Attack 노드로 넘어갈 것입니다.
		if (distance <= _stopRange) {
			nc->SetVelocity({ 0, 0, 0 });
			npc->SetState(common::packet::EntityState::IDLE);
			npc->SetActionId(0);
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
			npc->SetState(common::packet::EntityState::IDLE);
		}
		else if (currentSpeed < _speed * 0.5f) { // 원래 속도의 50% 미만이면 걷기
			npc->SetState(common::packet::EntityState::MOVE);
		}
		else {
			npc->SetState(common::packet::EntityState::MOVE);
		} 

		// [추가] 현재 공격 애니메이션 재생 중이면 이동 속도를 주지 않음
		if (npc->GetState() == common::packet::EntityState::ACTION) {
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
		auto owner = dynamic_cast<NPC*>(_blackboard->get<GameObject*>("owner"));
		if (!owner) return NodeStatus::FAILURE;
		auto npc = dynamic_cast<NPC*>(owner);
		auto* room = SERVER::Server::Instance()->GetRoom(_blackboard->get<int>("room_id"));
		if (!room) return NodeStatus::FAILURE;

		// 1. 공격 시작 (최초 프레임)
		if (_attackDurationTimer <= 0.0f) {
			if (_timer > 0.0f) {
				_timer -= dt; // 쿨타임 중
				return NodeStatus::FAILURE;
			}

			_attackDurationTimer = _config.animationDuration;
			_hasAttacked = false;
			_hitTimer = 0.0f;

			owner->SetState(_config.entityState);
			owner->SetActionId(_config.actionId);
			_timer = _config.cooldown; // 쿨타임 세팅

			auto nc = owner->GetComponent<NPCControllerComponent>();
			if (nc) nc->SetVelocity({ 0, 0, 0 });

			return NodeStatus::RUNNING;
		}

		// 2. 공격 진행 중 (애니메이션 재생 중)
		_attackDurationTimer -= dt;
		owner->SetState(_config.entityState);
		owner->SetActionId(_config.actionId);
		float elapsed = _config.animationDuration - _attackDurationTimer;

		// --- 타격 판정 (지속형 vs 단발형) ---
		if (_config.isContinuous) {
			// [지속형] 돌진 등: 일정 주기마다 계속 판정
			_hitTimer -= dt;
			if (_hitTimer <= 0.0f) {
				int64_t npcId = npc->GetId();
				room->PushJob([room, npcId, config = _config]() {
					auto* attacker = room->GetActor(npcId);
					if (attacker) {
						room->ExecuteActorAction(attacker, config);
					}
					});
				_hitTimer = _config.hitInterval;
			}
		}
		else {
			// [단발형] 내려찍기 등: 정해진 타이밍에 '한 번만' 판정
			if (!_hasAttacked && elapsed >= _config.attackTiming) {
				int64_t npcId = npc->GetId();
				room->PushJob([room, npcId, config = _config]() {
					auto* attacker = room->GetActor(npcId);
					if (attacker) {
						room->ExecuteActorAction(attacker, config);
					}
					});
				_hasAttacked = true; // 중복 판정 방지!!
			}
		}

		// 3. 종료 판정
		if (_attackDurationTimer <= 0.0f) {
			_attackDurationTimer = 0.0f;
			return NodeStatus::SUCCESS;
		}

		return NodeStatus::RUNNING;

		//// 1. 타이머 업데이트
		//if (_timer > 0.0f) _timer -= dt;
		//if (_attackDurationTimer > 0.0f) _attackDurationTimer -= dt;

		//// 2. 공격 동작 중인 경우 (애니메이션 대기)
		//if (_attackDurationTimer > 0.0f) {
		//	return NodeStatus::RUNNING; // 아직 공격 애니메이션 중이므로 이동 노드로 못 넘어가게 함
		//}

		//// 3. 쿨타임 체크 (애니메이션은 끝났는데 쿨타임이 남았다면 실패 리턴하여 다른 행동 허용)
		//if (_timer > 0.0f) return NodeStatus::FAILURE;

		//int64_t target_id = _blackboard->get<int64_t>("target_enemy");
		//Actor* target = room->GetActor(target_id);
		//if (!target) return NodeStatus::FAILURE;

		//auto npc = dynamic_cast<NPC*>(owner);
		//auto targetActor = dynamic_cast<Actor*>(target);
		//if (!npc || !targetActor) return NodeStatus::FAILURE;

		//// 공격 중에는 이동을 멈춤
		//auto nc = owner->GetComponent<NPCControllerComponent>();
		//if (nc) nc->SetVelocity({ 0, 0, 0 });

		//// 타겟 바라보기
		//common::Vec3 targetPos = targetActor->GetPosition();
		//common::Vec3 currentPos = npc->GetPosition();
		//float dx = targetPos.x - currentPos.x;
		//float dz = targetPos.z - currentPos.z;
		//float distSq = dx * dx + dz * dz;

		//if (distSq > 0.01f) {
		//	float angle = std::atan2(dx, dz);
		//	DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0, angle, 0);
		//	common::Quat rot{};
		//	XMStoreFloat4(&rot, q);
		//	npc->SetRotation(rot);
		//}

		//if (_config.attackTiming > 0.0f)
		//{
		//	// 공격 타이밍이 설정되어 있다면, 타이밍에 맞춰 공격 판정 요청
		//	if (_attackDurationTimer <= (_config.animationDuration - _config.attackTiming))
		//	{
		//		int64_t npcId = npc->GetId();
		//		room->PushJob([room, npcId, config = _config]() {
		//			auto* attacker = room->GetActor(npcId);
		//			if (attacker) {
		//				room->ExecuteActorAction(attacker, config);
		//			}
		//		});
		//	}
		//	else {
		//		return NodeStatus::RUNNING; // 아직 공격 타이밍이 안 됐으므로 대기
		//	}
		//}
		//else
		//{
		//	// 공격 타이밍이 설정되어 있지 않다면, 애니메이션 시작과 동시에 공격 판정 요청
		//	int64_t npcId = npc->GetId();
		//	room->PushJob([room, npcId, config = _config]() {
		//		auto* attacker = room->GetActor(npcId);
		//		if (attacker) {
		//			room->ExecuteActorAction(attacker, config);
		//		}
		//	});
		//	
		//	 // 공격 타이밍이 없으므로 애니메이션 시작과 동시에 공격 판정 요청 후 바로 대기
		//	return NodeStatus::RUNNING;
		//}

		//// 5. 상태 설정 및 타이머 세팅
		//npc->SetState(_config.animationState);
		//_attackDurationTimer = _config.animationDuration; // 애니메이션 지속 시간 동안 행동 잠금
		//_timer = _config.cooldown; 

		//return NodeStatus::SUCCESS;
	}

	bool Condition_CheckFlagFalse::check()
	{
		if (!_blackboard->has(_flagName)) return true;
		return _blackboard->get<bool>(_flagName) == false;
	}

	NodeStatus Action_SetFlagTrue::tick(float dt, JPH::TempAllocator* allocator) {
		_blackboard->set(_flagName, true);
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
		auto owner = dynamic_cast<NPC*>(_blackboard->get<GameObject*>("owner"));
		if (!owner) return false;

		auto state = owner->GetState();
		auto action_id = owner->GetActionId();
		if (state == common::packet::EntityState::ACTION && (action_id == 11 || action_id == 12)) {
			return true;
		}

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

	NodeStatus Action_Turn::tick(float dt, JPH::TempAllocator* allocator)
	{
		auto owner = _blackboard->get<GameObject*>("owner");
		auto npc = dynamic_cast<NPC*>(owner);
		if (!npc) return NodeStatus::FAILURE;

		// 1. 최초 실행 시 세팅
		if (_timer <= 0.0f) {
			_timer = _duration;
		}

		// 2. 타겟 방향으로 천천히 회전 (Slerp 느낌으로 구현)
		int64_t targetId = _blackboard->get<int64_t>("target_enemy");
		auto room = SERVER::Server::Instance()->GetRoom(_blackboard->get<int>("room_id"));
		auto target = room->GetActor(targetId);
		if (!target)
		{
			return NodeStatus::FAILURE;
		}

		if (target) {
			common::Vec3 targetPos = target->GetPosition();
			common::Vec3 currentPos = npc->GetPosition();
			common::Vec3 dir = common::Normalize(targetPos - currentPos);
			dir.y = 0;

			// 현재 방향과 목표 방향 사이를 보간하여 회전 (회전 속도 조절 가능)
			auto trans = owner->GetComponent<TransformComponent>();
			// 간단한 구현을 위해 즉시 방향을 보지 않고 매 프레임 일정 비율만 회전
			// (정교한 Slerp 라이브러리가 있다면 적용, 아니면 점진적 각도 변경)
			trans->SmoothRotateTo(dir, dt * 5.0f);
		}

		_timer -= dt;

		if (_timer <= 0.0f) {
			_timer = 0.0f;
			return NodeStatus::SUCCESS;
		}

		return NodeStatus::RUNNING;
		// 먼저 소리를 지르고 소리지르는 애니메이션 끝나면 
		// 타겟 위치저장 -> 그 위치로 돌진 = 타겟을 따라가면서 돌진 아님
	}

	NodeStatus Action_SettingChargeTargetPos::tick(float dt, JPH::TempAllocator* allocator)
	{
		auto owner = _blackboard->get<GameObject*>("owner");
		int64_t targetId = _blackboard->get<int64_t>("target_enemy");
		auto room = SERVER::Server::Instance()->GetRoom(_blackboard->get<int>("room_id"));
		auto target = room->GetActor(targetId);

		if (!owner || !target) return NodeStatus::FAILURE;

		common::Vec3 bossPos = owner->GetComponent<TransformComponent>()->GetPosition();
		common::Vec3 targetPos = target->GetPosition();

		// 1. 방향 계산 (플레이어 쪽으로)
		common::Vec3 dir = common::Normalize(targetPos - bossPos);
		dir.y = 0; // 높이 차이는 무시 (지면 돌진)

		if (common::LengthSq(dir) < 0.001f) {
			// 너무 가까우면 보스가 보는 앞 방향으로 설정
			dir = owner->GetComponent<TransformComponent>()->GetForward();
		}

		// 2. 정확히 10m 떨어진 지점 계산
		common::Vec3 fixedChargePos = bossPos + (dir * 10.0f);

		// 3. 블랙보드에 저장
		_blackboard->set("charge_target_pos", fixedChargePos);

		return NodeStatus::SUCCESS;
	}

	NodeStatus Action_ChargeToPosition::tick(float dt, JPH::TempAllocator* allocator)
	{
		auto owner = dynamic_cast<NPC*>(_blackboard->get<GameObject*>("owner"));
		if (!owner || !_blackboard->has("charge_target_pos")) return NodeStatus::FAILURE;

		common::Vec3 targetPos = _blackboard->get<common::Vec3>("charge_target_pos");
		common::Vec3 currentPos = owner->GetPosition();

		float distSq = common::DistanceSq(targetPos, currentPos);
		common::Vec3 dir = common::Normalize(targetPos - currentPos);

		// [중요] 거리가 0.5m 이내면 도착한 것으로 간주
		if (distSq < 0.5f * 0.5f) {
			auto nc = owner->GetComponent<NPCControllerComponent>();
			if (nc) nc->SetVelocity({ 0, 0, 0 });
			return NodeStatus::SUCCESS;
		}

		// 돌진 속도 적용 (10m를 빠르게 주파하기 위해 15~20 추천)
		auto nc = owner->GetComponent<NPCControllerComponent>();
		if (nc) {
			nc->SetVelocity(dir * 15.0f);
			owner->SetState(_config.entityState);
			owner->SetActionId(_config.actionId); // 돌진 애니메이션 ID (예시)
		}

		// 매 프레임 타격 판정 (ExecuteActorAction)
		int room_id = _blackboard->get<int>("room_id");
		auto room = SERVER::Server::Instance()->GetRoom(room_id);
		if (room) {
			room->ExecuteActorAction(owner, _config);
		}

		return NodeStatus::RUNNING;

		// 플레이어가 도망가면 돌진이 빗나갈 수 있음.
		// 몬스터의 오브젝트 스테이트는 돌진으로 만들고
		// 애니메이션은 본골렘의 Swim으로 바인딩
		// 본 골렘의 포효는 1초 -> 1초동안 움직임은 멈춰야함
	}

	NodeStatus Action_TargetingNearestPlayer::tick(float dt, JPH::TempAllocator* allocator)
	{
		auto owner = dynamic_cast<Actor*>(_blackboard->get<GameObject*>("owner"));
		if (!owner) return NodeStatus::FAILURE;
		auto room = SERVER::Server::Instance()->GetRoom(_blackboard->get<int>("room_id"));
		if (!room)
		{
			return NodeStatus::FAILURE;
		}

		common::Vec3 ownerPos = owner->GetPosition();
		auto players = room->GetPlayersPos();
		float _nearestDistSq = std::numeric_limits<float>::max();
		for (auto [pid, pos] : players)
		{
			float distSq = common::DistanceSq(ownerPos, pos);
			// 가장 가까운 플레이어를 타겟으로 설정
			if (distSq < _nearestDistSq) {
				_nearestDistSq = distSq;
				_blackboard->set("target_enemy", pid);
			}
		}
		return NodeStatus::SUCCESS;
	}

	NodeStatus Action_Roar::tick(float dt, JPH::TempAllocator* allocator)
	{
		auto owner = _blackboard->get<GameObject*>("owner");
		auto npc = dynamic_cast<NPC*>(owner);
		if (!npc) return NodeStatus::FAILURE;

		// _timer를 노드 멤버 변수로 추가해야 함 (기본값 0)
		if (_timer <= 0.0f) {
			_timer = _duration; // 포효 지속 시간 (예: 2초)
			auto nc = owner->GetComponent<NPCControllerComponent>();
			if (nc) nc->SetVelocity({ 0, 0, 0 });
		}
		npc->SetState(common::packet::EntityState::ACTION);
		npc->SetActionId(ActionID::Tainer::Roar); // 포효 애니메이션 ID (예시)
		_timer -= dt;
		if (_timer <= 0.0f) {
			_timer = 0.0f;
			return NodeStatus::SUCCESS;
		}
		return NodeStatus::RUNNING;
	}

	NodeStatus Action_ChargeAttack::tick(float dt, JPH::TempAllocator* allocator)
	{
		// 1. [핵심] 쿨타임 감소 (매 틱 실행)
		if (_cooldownTimer > 0.0f) _cooldownTimer -= dt;

		auto owner = dynamic_cast<NPC*>(_blackboard->get<GameObject*>("owner"));
		if (!owner) return NodeStatus::FAILURE;
		auto room = SERVER::Server::Instance()->GetRoom(_blackboard->get<int>("room_id"));
		if (!room) return NodeStatus::FAILURE;

		// 2. 쿨타임 체크 (Ready 상태일 때만)
		if (_currentPhase == Phase::READY && _cooldownTimer > 0.0f) {
			return NodeStatus::FAILURE;
		}


		// --- Phase 0: 준비 (포효 시작) ---
		if (_currentPhase == Phase::READY) {
			_currentPhase = Phase::ROAR;
			_internalTimer = 1.2f; // 포효 애니메이션 시간 (약 1.2초)
			_cooldownTimer = _config.cooldown;
			owner->SetState(_config.entityState);
			owner->SetActionId(common::packet::ActionID::Tainer::Roar);

			auto nc = owner->GetComponent<NPCControllerComponent>();
			if (nc) nc->SetVelocity({ 0, 0, 0 }); // 포효 중 정지
			return NodeStatus::RUNNING;
		}

		// --- Phase 1: 포효 중 (대기) ---
		if (_currentPhase == Phase::ROAR) {
			_internalTimer -= dt;
			owner->SetState(_config.entityState); // 상태 유지
			owner->SetActionId(ActionID::Tainer::Roar);

			if (_internalTimer <= 0.0f) {
				_currentPhase = Phase::TURN;
				_internalTimer = 0.4f; // 회전 시간 (짧게)
			}
			return NodeStatus::RUNNING;
		}

		// --- Phase 2: 방향 조준 및 10m 지점 박제 ---
		if (_currentPhase == Phase::TURN) {
			_internalTimer -= dt;

			int64_t targetId = _blackboard->get<int64_t>("target_enemy");
			auto target = room->GetActor(targetId);

			if (target) {
				// 타겟을 향해 부드럽게 회전
				common::Vec3 dir = common::Normalize(target->GetPosition() - owner->GetPosition());
				dir.y = 0;
				owner->GetComponent<TransformComponent>()->SmoothRotateTo(dir, dt * 10.0f);

				// 회전이 끝나갈 무렵 딱 한 번만 10m 앞 지점을 계산해서 저장
				if (!_isTargetLocked && _internalTimer < 0.1f) {
					common::Vec3 dashTarget = owner->GetPosition() + (dir * 10.0f);
					_blackboard->set("charge_target_pos", dashTarget);
					_isTargetLocked = true;
				}
			}

			if (_internalTimer <= 0.0f) {
				_currentPhase = Phase::DASHING;
			}
			return NodeStatus::RUNNING;
		}

		// --- Phase 3: 실제 돌진 (10m 주파) ---
		if (_currentPhase == Phase::DASHING) {
			if (!_blackboard->has("charge_target_pos")) return NodeStatus::FAILURE;

			common::Vec3 targetPos = _blackboard->get<common::Vec3>("charge_target_pos");
			common::Vec3 currentPos = owner->GetPosition();

			// 현재 위치에서 목적지로 향하는 벡터
			using namespace common::VectorHelper;
			common::Vec3 toTarget = targetPos - currentPos;

			// DASHING 페이즈 첫 프레임에 돌진 방향 고정
			if (common::LengthSq(_dashDir) < 0.001f) {
				_dashDir = common::Normalize(toTarget);
			}

			// [핵심] 도착 판정 로직 개선
			// 1. 거리가 매우 가깝거나 (0.2m 이내)
			// 2. 목적지를 지나쳤을 때 (목표 방향 _dashDir과 현재 남은 방향 toTarget의 내적이 음수면 지나친 것)
			float dot = toTarget.x * _dashDir.x + toTarget.y * _dashDir.y + toTarget.z * _dashDir.z;
			float distSq = common::LengthSq(toTarget);

			if (distSq < 0.2f * 0.2f || dot < 0) {
				auto nc = owner->GetComponent<NPCControllerComponent>();
				if (nc) nc->SetVelocity({ 0, 0, 0 });

				// 상태 초기화 (다음 돌진을 위해)
				_currentPhase = Phase::READY;
				_isTargetLocked = false;
				_dashDir = { 0, 0, 0 };
				_blackboard->set("charge_target_pos", std::any());
				return NodeStatus::SUCCESS;
			}

			// 돌진 이동 적용
			// 매 프레임 계산하는 dir 대신, 고정된 _dashDir로 속도 적용 (회귀 방지)
			auto nc = owner->GetComponent<NPCControllerComponent>();
			if (nc) nc->SetVelocity(_dashDir * _speed);
			owner->SetState(_config.entityState);
			owner->SetActionId(ActionID::Tainer::Charge);

			// 매 프레임 타격 판정 실행
			int64_t npcId = owner->GetId();
			room->PushJob([room, npcId, config = _config]() {
				auto* attacker = room->GetActor(npcId);
				if (attacker) {
					room->ExecuteActorAction(attacker, config);
				}
			});

			return NodeStatus::RUNNING;
		}

		return NodeStatus::FAILURE;
	}
}
