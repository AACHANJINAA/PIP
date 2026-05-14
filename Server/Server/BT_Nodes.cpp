#include "pch.h"
#include "BT_Nodes.h"

#include "PlayerControllerComponent.h"
#include "Server.h"
#include "Tainer.h"

namespace PIP::GAME
{
	using namespace common::packet;
	bool Condition_IsHitted::check()
	{
		auto npc = _blackboard->get<NPC*>("owner_npc");
		return npc && npc->GetState() == EntityState::HITTED;
	}

	bool Condition_IsAlive::check()
	{
		auto npc = _blackboard->get<NPC*>("owner_npc");
		return npc && npc->GetState() != EntityState::DEAD;
	}

	bool Condition_HasTarget::check()
	{
		// 1. 배회 목적지(target_pos)가 있는지 확인
		if (!_blackboard->has("target_pos")) return false;

		return true;
	}

	NodeStatus Action_FindRandomTarget::tick(float dt, JPH::TempAllocator* allocator)
	{
		GameObject* owner = _blackboard->get<GameObject*>("owner");
		if (!owner)
		{
			return NodeStatus::FAILURE;
		}
		auto tc = owner->GetComponent<TransformComponent>();
		if (!tc)
		{
			return NodeStatus::FAILURE;
		}
		common::Vec3 currentPos = tc->GetPosition();

		// 2. 현재 위치 기준 [-_range, _range] 범위의 랜덤 오프셋 계산
		// 기존의 정수형 rand() % 대신, 실수 범위(-_range ~ +_range)의 랜덤값을 생성합니다.
		float offsetX = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * _range;
		float offsetZ = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * _range;

		float tx = currentPos.x + offsetX;
		float tz = currentPos.z + offsetZ;

		// 지형 높이 보정 (Y좌표)
		common::Vec3 targetPos = MapDataManager::Instance()->AdjustPositionToGround({ tx, 50, tz });

		_blackboard->set("target_pos", targetPos);
		_blackboard->set("stuck_timer", 0.0f); // 타이머 리셋

		return NodeStatus::SUCCESS;
	}
	

	NodeStatus Action_MoveToTarget::tick(float dt, JPH::TempAllocator* allocator) {
		auto npc = _blackboard->get<NPC*>("owner_npc");
		if (!npc) return NodeStatus::FAILURE;

		auto nc = npc->GetNPCController();
		auto tc = npc->GetTransform();
		if (!nc || !tc) return NodeStatus::FAILURE;

		common::Vec3 target = _blackboard->get<common::Vec3>("target_pos");
		common::Vec3 current = tc->GetPosition();

		// 거리 계산
		float dx = target.x - current.x;
		float dz = target.z - current.z;
		float distance = std::sqrt(dx * dx + dz * dz);

		// 1. 정지 판정
		if (distance < 0.2f) {
			nc->SetVelocity({ 0, 0, 0 });
			_blackboard->set("target_pos", std::any());
			npc->SetState(common::packet::EntityState::IDLE);
			npc->SetActionId(0);
			return NodeStatus::SUCCESS;
		}

		// 2. 자연스러운 감속
		float slowRadius = 1.5f;
		float currentSpeed = _speed;
		if (distance < slowRadius) {
			currentSpeed = _speed * (distance / slowRadius);
		}

		// 3. 오버슈팅 방지
		float maxSafeSpeed = distance / dt;
		currentSpeed = std::min(currentSpeed, maxSafeSpeed);

		// 4. 끼임 감지
		float stuckTimer = _blackboard->get<float>("stuck_timer");
		common::Vec3 lastPos = current;
		if (_blackboard->has("last_pos")) {
			lastPos = _blackboard->get<common::Vec3>("last_pos");
		}

		float movedDistSq = (current.x - lastPos.x) * (current.x - lastPos.x) + (current.z - lastPos.z) * (current.z - lastPos.z);
		float expectedDist = currentSpeed * dt * 0.2f;

		if (movedDistSq < (expectedDist * expectedDist)) {
			if (distance > slowRadius) stuckTimer += dt;
		}
		else {
			stuckTimer = 0.0f;
		}

		_blackboard->set("stuck_timer", stuckTimer);
		_blackboard->set("last_pos", current);

		if (stuckTimer > 1.0f) {
			_blackboard->set("target_pos", std::any());
			nc->SetVelocity({ 0, 0, 0 });
			npc->SetState(common::packet::EntityState::IDLE);
			npc->SetActionId(0);
			return NodeStatus::FAILURE;
		}

		// 5. 이동 및 상태 적용
		common::Vec3 vel = { (dx / distance) * currentSpeed, 0, (dz / distance) * currentSpeed };
		nc->SetVelocity(vel);

		if (currentSpeed < 0.1f) {
			npc->SetState(common::packet::EntityState::IDLE);
			npc->SetActionId(0);
		}
		else {
			npc->SetState(common::packet::EntityState::MOVE);
		}

		// 6. 회전
		if (distance > 0.1f) {
			float angle = std::atan2(vel.x, vel.z);
			DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0, angle, 0);
			common::Quat rot;
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
		if (!room) return NodeStatus::FAILURE;

		auto npc = _blackboard->get<NPC*>("owner_npc");
		int64_t enemy_id = _blackboard->get<int64_t>("target_enemy");

		Actor* enemy = room->GetActor(enemy_id);
		if (!npc || !enemy) return NodeStatus::FAILURE;

		auto nc = npc->GetNPCController();
		auto tc = npc->GetTransform();
		if (!nc || !tc) return NodeStatus::FAILURE;

		// 1. 타겟과의 거리 및 방향 계산
		common::Vec3 targetPos = enemy->GetPosition();
		common::Vec3 currentPos = tc->GetPosition();

		float dx = targetPos.x - currentPos.x;
		float dz = targetPos.z - currentPos.z;
		float distance = std::sqrt(dx * dx + dz * dz);

		// 2. 근접 시 정지
		if (distance <= _stopRange) {
			nc->SetVelocity({ 0, 0, 0 });
			npc->SetState(common::packet::EntityState::IDLE);
			npc->SetActionId(0);
			return NodeStatus::SUCCESS;
		}

		// 3. 속도 계산
		float currentSpeed = _speed;
		float slowRadius = 1.0f;
		if (distance < slowRadius) {
			currentSpeed = _speed * (distance / slowRadius);
		}
		float maxSafeSpeed = distance / dt;
		currentSpeed = std::min(currentSpeed, maxSafeSpeed);

		// 4. 이동 속도 적용
		common::Vec3 dir = { dx / distance, 0, dz / distance };
		nc->SetVelocity(dir * currentSpeed);

		// 5. 상태 결정
		npc->SetState(common::packet::EntityState::MOVE);

		if (npc->GetState() == common::packet::EntityState::ACTION) {
			nc->SetVelocity({ 0, 0, 0 });
			return NodeStatus::RUNNING;
		}

		// 6. 부드러운 회전 처리
		if (distance > 0.1f) {
			float angle = std::atan2(dx, dz);
			DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0, angle, 0);
			common::Quat rot{};
			XMStoreFloat4(&rot, q);
			tc->SetRotation(rot);
		}

		return NodeStatus::RUNNING;
	}

	NodeStatus Action_AttackEnemy::tick(float dt, JPH::TempAllocator* allocator) {
		auto npc = _blackboard->get<NPC*>("owner_npc");
		if (!npc) return NodeStatus::FAILURE;

		auto* room = SERVER::Server::Instance()->GetRoom(_blackboard->get<int>("room_id"));
		if (!room) return NodeStatus::FAILURE;

		// 1. 공격 시작 (최초 프레임)
		if (_attackDurationTimer <= 0.0f) {
			if (_timer > 0.0f) {
				_timer -= dt;
				return NodeStatus::FAILURE;
			}

			_attackDurationTimer = _config.animationDuration;
			_hasAttacked = false;
			_hitTimer = 0.0f;

			npc->SetState(_config.entityState);
			npc->SetActionId(_config.actionId);
			_timer = _config.cooldown;

			auto nc = npc->GetNPCController();
			if (nc) nc->SetVelocity({ 0, 0, 0 });

			return NodeStatus::RUNNING;
		}

		// 2. 공격 진행 중
		_attackDurationTimer -= dt;
		npc->SetState(_config.entityState);
		npc->SetActionId(_config.actionId);
		float elapsed = _config.animationDuration - _attackDurationTimer;

		// --- 타격 판정 ---
		if (_config.isContinuous) {
			_hitTimer -= dt;
			if (_hitTimer <= 0.0f) {
				int64_t npcId = npc->GetId();
				room->PushJob([room, npcId, config = _config]() {
					auto* attacker = room->GetActor(npcId);
					if (attacker) room->ExecuteActorAction(attacker, config);
				});
				_hitTimer = _config.hitInterval;
			}
		}
		else {
			if (!_hasAttacked && elapsed >= _config.attackTiming) {
				int64_t npcId = npc->GetId();
				room->PushJob([room, npcId, config = _config]() {
					auto* attacker = room->GetActor(npcId);
					if (attacker) room->ExecuteActorAction(attacker, config);
				});
				_hasAttacked = true;
			}
		}

		// 3. 종료 판정
		if (_attackDurationTimer <= 0.0f) {
			_attackDurationTimer = 0.0f;
			return NodeStatus::SUCCESS;
		}

		return NodeStatus::RUNNING;
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
		auto maxHP = _blackboard->get<int>("max_hp");
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

	NodeStatus Action_GrabCharge::tick(float dt, JPH::TempAllocator* allocator)
	{
		if (_cooldownTimer > 0.0f) _cooldownTimer -= dt;

		auto owner = dynamic_cast<NPC*>(_blackboard->get<GameObject*>("owner"));
		if (!owner) return NodeStatus::FAILURE;
		auto room = SERVER::Server::Instance()->GetRoom(_blackboard->get<int>("room_id"));
		if (!room) return NodeStatus::FAILURE;

		if (_currentPhase == Phase::READY && _cooldownTimer > 0.0f) {
			return NodeStatus::FAILURE;
		}

		// --- Phase 0: 준비 (포효 시작) ---
		if (_currentPhase == Phase::READY) {
			_currentPhase = Phase::ROAR;
			_internalTimer = 1.2f;
			_cooldownTimer = _config.cooldown;
			_grabbedPlayerIds.clear(); // 초기화
			owner->SetState(_config.entityState);
			owner->SetActionId(common::packet::ActionID::Tainer::Roar);

			auto nc = owner->GetComponent<NPCControllerComponent>();
			if (nc) nc->SetVelocity({ 0, 0, 0 });
			return NodeStatus::RUNNING;
		}

		// --- Phase 1: 포효 중 ---
		if (_currentPhase == Phase::ROAR) {
			_internalTimer -= dt;
			owner->SetState(_config.entityState);
			owner->SetActionId(ActionID::Tainer::Roar);

			if (_internalTimer <= 0.0f) {
				_currentPhase = Phase::TURN;
				_internalTimer = 0.4f;
			}
			return NodeStatus::RUNNING;
		}

		// --- Phase 2: 조준 ---
		if (_currentPhase == Phase::TURN) {
			_internalTimer -= dt;
			int64_t targetId = _blackboard->get<int64_t>("target_enemy");
			auto target = room->GetActor(targetId);

			if (target) {
				common::Vec3 targetPos = target->GetPosition();
				common::Vec3 ownerPos = owner->GetPosition();
				common::Vec3 dir = common::Normalize(targetPos - ownerPos);
				dir.y = 0;
				owner->GetComponent<TransformComponent>()->SmoothRotateTo(dir, dt * 10.0f);

				// [추가] 근거리 즉시 잡기: 이미 사거리 내에 있다면 돌진 대기시간 단축 및 즉시 전환
				float distSq = common::DistanceSq(targetPos, ownerPos);
				if (distSq < 2.5f * 2.5f) {
					_currentPhase = Phase::DASHING;
					_internalTimer = 2.0f; // 돌진(잡기 시도) 타이머 시작
					_dashDir = dir;
					return NodeStatus::RUNNING;
				}

				if (!_isTargetLocked && _internalTimer < 0.1f) {
					common::Vec3 dashTarget = owner->GetPosition() + (dir * 15.0f); // 잡기는 좀 더 멀리 돌진
					_blackboard->set("charge_target_pos", dashTarget);
					_isTargetLocked = true;
				}
			}

			if (_internalTimer <= 0.0f) {
				_currentPhase = Phase::DASHING;
				_dashDir = { 0, 0, 0 };
				_internalTimer = 2.0f; // [추가] 돌진 최대 지속 시간 (2초)
			}
			return NodeStatus::RUNNING;
		}

		// --- Phase 3: 돌진 및 잡기 시도 ---
		if (_currentPhase == Phase::DASHING) {
			_internalTimer -= dt; // [추가] 타임아웃 타이머 감소

			if (!_blackboard->has("charge_target_pos")) return NodeStatus::FAILURE;

			common::Vec3 targetPos = _blackboard->get<common::Vec3>("charge_target_pos");
			common::Vec3 currentPos = owner->GetPosition();
			common::Vec3 toTarget = targetPos - currentPos;

			if (common::LengthSq(_dashDir) < 0.001f) {
				_dashDir = common::Normalize(toTarget);
			}

			float dot = toTarget.x * _dashDir.x + toTarget.y * _dashDir.y + toTarget.z * _dashDir.z;
			float distSq = common::LengthSq(toTarget);

			// [수정] 도착 판정 혹은 2초 타임아웃 시 실패 처리
			if (distSq < 0.2f * 0.2f || dot < 0 || _internalTimer <= 0.0f) {
				auto nc = owner->GetComponent<NPCControllerComponent>();
				if (nc) nc->SetVelocity({ 0, 0, 0 });

				_currentPhase = Phase::READY;
				_isTargetLocked = false;
				_dashDir = { 0, 0, 0 };
				
				if (_internalTimer <= 0.0f) {
					MYLOG("[Grab] Dash timed out (2s) - Grab failed.");
				}
				return NodeStatus::FAILURE; // 잡기 실패
			}

			auto nc = owner->GetComponent<NPCControllerComponent>();
			if (nc) nc->SetVelocity(_dashDir * _speed);
			owner->SetState(_config.entityState);
			owner->SetActionId(_config.actionId); // [수정] Charge -> GrabCharge (설정값 사용)

			// 타격 판정 (잡기 포함)
			int64_t npcId = owner->GetId();
			room->PushJob([room, npcId, config = _config]() {
				auto* attacker = room->GetActor(npcId);
				if (attacker) room->ExecuteActorAction(attacker, config);
			});

			// 잡힌 플레이어가 있는지 체크
			auto players = room->GetPlayersPos();
			for (auto [pid, pos] : players) {
				auto p = room->GetPlayer(pid);
				if (p && p->GetGrabbedById() == owner->GetId() && p->GetHP() > 0) {
					// 아직 리스트에 없는 플레이어라면 추가 시도
					if (std::find(_grabbedPlayerIds.begin(), _grabbedPlayerIds.end(), pid) == _grabbedPlayerIds.end()) {
						if (_grabbedPlayerIds.size() < 2) {
							int8_t slot = (int8_t)_grabbedPlayerIds.size();
							p->SetGrabSlot(slot);
							p->SetState(common::packet::EntityState::GRABBED);
							_grabbedPlayerIds.push_back(pid);
							
							_currentPhase = Phase::CARRYING;
							_internalTimer = 3.0f; // 3초간 난타
							_damageTimer = 0.0f;   // 즉시 첫 데미지 주게 초기화
							MYLOG("[Grab] Player " << pid << " grabbed into slot " << (int)slot);
						}
						else {
							// 최대 2명을 넘어가면 그랩 해제 (그냥 맞기만 함)
							p->SetGrabbedById(-1);
							p->SetGrabSlot(-1);
						}
					}
				}
			}

			return NodeStatus::RUNNING;
		}

		// --- Phase 4: 플레이어 들고 난타 (3초) ---
		if (_currentPhase == Phase::CARRYING) {
			_internalTimer -= dt;
			_damageTimer -= dt; // [추가] DoT 타이머 감소
			owner->SetState(_config.entityState);
			owner->SetActionId(ActionID::Tainer::GrabCarry);

			auto nc = owner->GetComponent<NPCControllerComponent>();
			if (nc) nc->SetVelocity({ 0, 0, 0 }); // 난타 중에는 정지

			bool shouldApplyDamage = false;
			if (_damageTimer <= 0.0f) {
				shouldApplyDamage = true;
				_damageTimer = 1.0f; // 1초 간격으로 데미지 적용
			}

			common::Vec3 ownerPos = owner->GetPosition();
			common::Vec3 forward = owner->GetComponent<TransformComponent>()->GetForward();
			common::Vec3 right = owner->GetComponent<TransformComponent>()->GetRight();

			for (auto it = _grabbedPlayerIds.begin(); it != _grabbedPlayerIds.end(); ) {
				auto p = room->GetPlayer(*it);
				if (p && p->GetHP() > 0 && p->GetGrabbedById() == owner->GetId()) {
					// 1. 데미지 적용 (1초당 20)
					if (shouldApplyDamage) {
						p->SetHP(p->GetHP() - 10);
						if (p->GetHP() <= 0) {
							room->OnPlayerDead(room->GetSession(p->GetId()));
							it = _grabbedPlayerIds.erase(it);
							continue;
						}
					}

					// 2. 위치 고정 (클라이언트에서 본 부착을 하겠지만, 서버에서도 물리 계산 방지 및 위치 동기화를 위해 업데이트)
					// slot 0: 왼손 근처, slot 1: 오른손 근처 (임시 좌표)
					float sideOffset = (p->GetGrabSlot() == 0) ? -1.0f : 1.0f;
					common::Vec3 grabPos = ownerPos + (forward * 1.5f) + (right * sideOffset);
					grabPos.y += 1.5f;
					p->SetPosition(grabPos);
					p->SetState(common::packet::EntityState::GRABBED);
					
					++it;
				}
				else {
					// 플레이어가 죽었거나 나갔으면 리스트에서 제거
					if (p) {
						p->SetGrabbedById(-1);
						p->SetGrabSlot(-1);
					}
					it = _grabbedPlayerIds.erase(it);
				}
			}

			if (_internalTimer <= 0.0f || _grabbedPlayerIds.empty()) {
				_currentPhase = Phase::SLAM;
				_internalTimer = 1.0f; // 슬램/릴리즈 애니메이션 시간
			}
			return NodeStatus::RUNNING;
		}

		// --- Phase 5: 슬램 (팅겨내기) ---
		if (_currentPhase == Phase::SLAM) {
			_internalTimer -= dt;
			owner->SetState(_config.entityState);
			owner->SetActionId(ActionID::Tainer::GrabSlam);

			auto nc = owner->GetComponent<NPCControllerComponent>();
			if (nc) nc->SetVelocity({ 0, 0, 0 });

			if (_internalTimer <= 0.0f) {
				common::Vec3 forward = owner->GetComponent<TransformComponent>()->GetForward();
				common::Vec3 ownerPos = owner->GetPosition();

				for (int64_t pid : _grabbedPlayerIds) {
					auto p = room->GetPlayer(pid);
					if (p) {
						// 1. 버스트 데미지 적용 (슬램 타격) 하고 싶으면 키기
						// p->SetHP(p->GetHP() - 40);

						// 2. 상태 해제
						p->SetGrabbedById(-1);
						p->SetGrabSlot(-1);
						
						// HP가 남아있으면 IDLE, 아니면 DEAD
						if (p->GetHP() > 0) {
							p->SetState(common::packet::EntityState::IDLE);
						}
						else {
							p->SetState(common::packet::EntityState::DEAD);
							room->OnPlayerDead(room->GetSession(p->GetId()));
						}

						// [보정] 보스 몸통 정면 1.5m 위치로 강제 이동 후 튕겨내기
						common::Vec3 releasePos = ownerPos + (forward * 1.5f);
						releasePos.y += 0.5f; // 지면보다 약간 위
						p->SetPosition(releasePos);

						// 2. 팅겨내기 (물리적 속도 부여)
						auto pcc = p->GetComponent<PlayerControllerComponent>();
						if (pcc) {
							common::Vec3 launchDir = forward;
							launchDir.y = 0.3f; // 약간 위로 향하는 궤적
							pcc->AddImpact(common::Normalize(launchDir) * 18.0f); // [수정] SetMoveVelocity -> AddImpact
						}
						
						MYLOG("[Grab] Player " << pid << " released and bounced from boss body");
					}
				}

				_currentPhase = Phase::READY;
				_grabbedPlayerIds.clear();
				_isTargetLocked = false;
				_dashDir = { 0, 0, 0 };
				return NodeStatus::SUCCESS;
			}
			return NodeStatus::RUNNING;
		}

		return NodeStatus::FAILURE;
	}

	NodeStatus Action_FindPath::tick(float dt, JPH::TempAllocator* allocator)
	{
		// 1. 길찾기 빈도 제한 (스태거링)
		float nextSearchTimer = _blackboard->get<float>("path_search_cooldown");
		if (nextSearchTimer > 0.0f) {
			nextSearchTimer -= dt;
			_blackboard->set("path_search_cooldown", nextSearchTimer);
			return NodeStatus::SUCCESS; // 아직 쿨타임이면 기존 경로 유지
		}

		auto npc = _blackboard->get<NPC*>("owner_npc");
		common::Vec3 start = npc->GetPosition();
		common::Vec3 end = _blackboard->get<common::Vec3>("target_pos");

		// 2. 타겟이 의미 있게 움직였을 때만 실제 A* 수행 (예: 2m 이상)
		common::Vec3 lastTarget = _blackboard->get<common::Vec3>("last_search_pos");
		if (common::DistanceSq(lastTarget, end) < 4.0f) {
			nextSearchTimer = 0.5f; // 짧은 휴식 후 재체크
			_blackboard->set("path_search_cooldown", nextSearchTimer);
			return NodeStatus::SUCCESS;
		}

		// 3. 실제 탐색 호출
		std::vector<common::Vec3> newPath;
		if (MapDataManager::Instance()->FindPath(_navName, start, end, newPath)) {
			_blackboard->set("path", std::move(newPath));
			lastTarget = end;
			nextSearchTimer = 1.0f + (rand() % 500) * 0.001f; // 1.0~1.5초 무작위 쿨타임 (부하 분산)
			_blackboard->set("path_search_cooldown", nextSearchTimer);
			return NodeStatus::SUCCESS;
		}
		return NodeStatus::FAILURE;
	}

	NodeStatus Action_FollowPath::tick(float dt, JPH::TempAllocator* allocator)
	{
		if (!_blackboard->has("path")) return NodeStatus::FAILURE;
		auto path = _blackboard->get<std::vector<common::Vec3>>("path");
		if (path.empty()) return NodeStatus::FAILURE;

		auto npc = _blackboard->get<NPC*>("owner_npc");
		if (!npc) return NodeStatus::FAILURE;

		auto nc = npc->GetNPCController();
		auto tc = npc->GetTransform();
		if (!nc || !tc) return NodeStatus::FAILURE;

		common::Vec3 currentPos = tc->GetPosition();
		common::Vec3 nextTarget = path[0];

		// 도착 판정 (수평 거리 기준 1.0m 이내면 도달로 간주)
		common::Vec3 diff = nextTarget - currentPos;
		diff.y = 0;

		if (common::Distance(currentPos, nextTarget) < 1.0f) {
			path.erase(path.begin());
			_blackboard->set("path", path);
			if (path.empty()) {
				nc->SetVelocity({ 0, 0, 0 });
				return NodeStatus::SUCCESS;
			}
			nextTarget = path[0];
			diff = nextTarget - currentPos;
			diff.y = 0;
		}

		// 이동 처리
		if (common::LengthSq(diff) > 0.001f) {
			common::Vec3 moveDir = common::Normalize(diff);
			nc->SetVelocity(moveDir * _speed);

			// 회전 처리
			float angle = std::atan2(moveDir.x, moveDir.z);
			DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0, angle, 0);
			common::Quat rot;
			XMStoreFloat4((XMFLOAT4*)&rot, q);
			tc->SetRotation(rot);

			npc->SetState(common::packet::EntityState::MOVE);
		}

		return NodeStatus::RUNNING;
	}
}
