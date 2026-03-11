#include "pch.h"
#include "Tainer.h"

#include "AIComponent.h"
#include "BT_Nodes.h"

namespace PIP::GAME
{
	Tainer::Tainer(int64_t npc_id, int room_id, common::Vec3 position)
		: NPC(npc_id, NPCType::Tainer, room_id, position, 500)
	{
		_maxHp = 500;
		SetName("Boss_Tainer");
		SetFaction(Faction::FACTION_MONSTER);

		// --- 공격 설정 초기화 ---
		// Phase 1: Slam
		_slamAtk.shape = new JPH::SphereShape(3.0f);
		_slamAtk.posOffset = { 0.0f, 0.0f, 2.0f };
		_slamAtk.damage = 30;
		_slamAtk.cooldown = 3.0f;
		_slamAtk.animationKey = "Slam";

		// Phase 1: Charge
		_chargeAtk.shape = new JPH::BoxShape(JPH::Vec3(1.5f, 1.5f, 2.5f));
		_chargeAtk.posOffset = { 0.0f, 1.0f, 2.0f };
		_chargeAtk.damage = 25;
		_chargeAtk.cooldown = 6.0f;
		_chargeAtk.animationKey = "Charge";

		// Phase 2: Claw (난타)
		_clawAtk.shape = new JPH::SphereShape(2.0f);
		_clawAtk.posOffset = { 0.0f, 1.0f, 1.5f };
		_clawAtk.damage = 15;
		_clawAtk.cooldown = 1.5f;
		_clawAtk.animationKey = "Claw";

		// Phase 2: Grab
		_grabAtk.shape = new JPH::BoxShape(JPH::Vec3(1.0f, 1.0f, 1.5f));
		_grabAtk.posOffset = { 0.0f, 1.0f, 1.2f };
		_grabAtk.damage = 50;
		_grabAtk.cooldown = 10.0f;
		_grabAtk.animationKey = "Grab";

		Tainer::SetupBT();
	}

	void Tainer::SetupBT()
	{
		auto ai = GetComponent<AIComponent>();
		if (!ai) return;

		auto bb = ai->GetBlackboard();
		// Blackboard 기본 데이터 세팅
		bb->set("owner", static_cast<GameObject*>(this));
		bb->set("room_id", GetRoomId());
		bb->set("max_hp", _maxHp);

		BTBuilder builder;
		auto root = builder
			.selector()
				// --- [우선순위 1] 페이즈 2 패턴 (HP 50% 이하) ---
				.sequence()
					.leaf<Condition_IsPhase>(TainerPhase::PHASE_2) // 페이즈 체크
					.selector()
						// 1-1. 잡기 공격 (가장 강력함, 사거리 짧음)
						.sequence()
							.leaf<Condition_IsEnemyInRange>(2.0f)
							.leaf<Action_AttackEnemy>(_grabAtk)
						.end()
						// 1-2. 클로 난타 (빠른 공격, 사거리 중간)
						.sequence()
							.leaf<Condition_IsEnemyInRange>(4.0f)
							.leaf<Action_RotateToEnemy>() // 공격 전 타겟 방향 회전
							.leaf<Action_AttackEnemy>(_clawAtk)
						.end()
						// 1-3. 타겟 추격 (페이즈 2는 더 빠름)
						.leaf<Action_ChaseEnemy>(7.0f)
					.end()
				.end()
				// --- [우선순위 2] 페이즈 1 패턴 (기본) ---
				.sequence()
					.leaf<Condition_IsPhase>(TainerPhase::PHASE_1)
					.selector()
						// 2-1. 돌진 (특정 거리 4m~10m 사이일 때만 사용)
						.sequence()
							.leaf<Condition_IsEnemyInDistanceRange>(4.0f, 10.0f)
							.leaf<Action_RotateToEnemy>()
							.leaf<Action_AttackEnemy>(_chargeAtk)
						.end()
						// 2-2. 내려찍기 (가까이 있으면 사용)
						.sequence()
							.leaf<Condition_IsEnemyInRange>(4.0f)
							.leaf<Action_AttackEnemy>(_slamAtk)
						.end()
						// 2-3. 타겟 추격 (페이즈 1 속도)
						.leaf<Action_ChaseEnemy>(4.0f)
					.end()
				.end()
				// --- [우선순위 3] 공통: 타겟이 없을 경우 ---
				.leaf<Action_FindRandomTarget>()
			.end()
		.build();

		root->set_blackboard(bb);
		ai->SetBehaviorTree(root);
	}

	void Tainer::CheckPhaseTransition()
	{
		if (_currentPhase == TainerPhase::PHASE_1 && GetHP() < _maxHp * 0.5f)
		{
			_currentPhase = TainerPhase::PHASE_2;
			// TODO: 페이즈 전환 패킷 브로드캐스트 (연출용)
			MYLOG("Tainer Phase 2 Started!");
			SetupBT(); // BT 재구성
		}
	}

	bool Tainer::ValidateHit(JPH::PhysicsSystem* physics, const JPH::Shape* attackShape,
		const JPH::RMat44& attackTransform, uint32_t timestamp, GameObject* attacker, int32_t damage)
	{
		bool hit = NPC::ValidateHit(physics, attackShape, attackTransform, timestamp, attacker, damage);
		if (hit) CheckPhaseTransition();
		return hit;
	}

	void Tainer::SetPhase(const TainerPhase& new_phase)
	{
		_currentPhase = new_phase;
	}

	void Tainer::Update(float deltaTime, JPH::TempAllocator* allocator)
	{
		NPC::Update(deltaTime, allocator);
	}
}
