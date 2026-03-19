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
		_slamAtk.posOffset = { 0.0f, 0.0f, 3.5f };
		_slamAtk.damage = 20;
		_slamAtk.cooldown = 1.0f;
		_slamAtk.entityState = common::packet::EntityState::ACTION;
		_slamAtk.actionId = common::packet::ActionID::Tainer::Slam;
		_slamAtk.animationDuration = 1.0f; // 내려찍기 애니메이션은 1초 지속
		_slamAtk.attackTiming = 0.5f; // 애니메이션 시작 후 0.5초에 판정 발생
		_slamAtk.hitInterval = 0.0f; // 단발 공격이므로 판정 주기는 의미 없음
		_slamAtk.isContinuous = false; // 내려찍기는 단발 공격

		// Phase 1: Charge
		_chargeAtk.shape = new JPH::SphereShape(3.0f);
		_chargeAtk.posOffset = { 0.0f, 2.0f, 0.0f };
		_chargeAtk.damage = 25;
		_chargeAtk.cooldown = 3.0f;
		_chargeAtk.entityState = common::packet::EntityState::ACTION;
		_chargeAtk.actionId = common::packet::ActionID::Tainer::Charge;
		_chargeAtk.animationDuration = 2.0f; // 돌진 애니메이션은 2초 지속
		_chargeAtk.hitInterval = 0.2f; // 돌진 중 0.2초마다 판정 발생
		_chargeAtk.isContinuous = true; // 돌진은 지속 공격

		// Phase 2: Claw (난타)
		_clawAtk.shape = new JPH::SphereShape(2.0f);
		_clawAtk.posOffset = { 0.0f, 1.0f, 1.5f };
		_clawAtk.damage = 15;
		_clawAtk.cooldown = 1.5f;
		_clawAtk.entityState = common::packet::EntityState::ACTION;
		_clawAtk.actionId = common::packet::ActionID::Tainer::Claw;
		_clawAtk.animationDuration = 0.5f; // 클로 난타는 빠르게 여러 번 공격

		// Phase 2: Grab
		_grabAtk.shape = new JPH::BoxShape(JPH::Vec3(1.0f, 1.0f, 2.5f));
		_grabAtk.posOffset = { 0.0f, 1.0f, 1.2f };
		_grabAtk.damage = 50;
		_grabAtk.cooldown = 10.0f;
		_grabAtk.entityState = common::packet::EntityState::ACTION;
		_grabAtk.actionId = common::packet::ActionID::Tainer::Grab;
		_grabAtk.animationDuration = 1.f; 

		Tainer::SetupBT();
	}

	void Tainer::SetupBT()
	{
		auto ai = GetComponent<AIComponent>();
		if (!ai) return;
		ai->Initialize();

		auto bb = ai->GetBlackboard();
		// Blackboard 기본 데이터 세팅
		bb->set("owner", static_cast<GameObject*>(this));
		bb->set("room_id", GetRoomId());
		bb->set("max_hp", _maxHp);

		BTBuilder builder;
		auto root = builder
			.sequence()
				.leaf_name<Action_TargetingNearestPlayer>("Targeting")
				.selector() // 공격이 먼저!
					.sequence() // [공격 1] 내려찍기
						.leaf_name<Condition_IsEnemyInRange>("In_Slam_Range", 6.5f)
						.leaf_name<Action_AttackEnemy>("Slam_Attack", _slamAtk)
					.end()
					.sequence() // [공격 2] 돌진
						.leaf_name<Condition_IsEnemyInDistanceRange>("Charge_Range", 10.0f, 15.0f)
						.leaf_name<Action_ChargeAttack>("Action_Charge", 16.0f, _chargeAtk)
					.end()
				.leaf_name<Action_ChaseEnemy>("Chase", 4.0f, 6.5f) // 공격 못 하면 추격
				.end()
			.end()
		.build();
		//	.selector()
		//		// --- [우선순위 1] 페이즈 2 패턴 (HP 50% 이하) ---
		//		.sequence()
		//			.leaf_name<Condition_IsPhase>("Condition_IsPhase", TainerPhase::PHASE_2) // 페이즈 체크
		//			.selector()
		//				.sequence() // 페이즈 2 진입 시 딱 한 번만 포효
		//					.leaf_name<Condition_CheckFlagFalse>("Condition_CheckFlagFalse","is_p2_roar_done")
		//					.leaf_name<Action_Roar>("Action_Roar")
		//					.leaf_name<Action_SetFlagTrue>("Action_SetFlagTrue","is_p2_roar_done")
		//				.end()
		//				.selector()
		//					// 1-1. 잡기 공격 (가장 강력함, 사거리 짧음)
		//					.sequence()
		//						.leaf_name<Condition_IsEnemyInRange>("Condition_IsEnemyInRange",2.0f)
		//						.leaf_name<Action_AttackEnemy>("Grab Attack",_grabAtk)
		//					.end()
		//					// 1-2. 클로 난타 (빠른 공격, 사거리 중간)
		//					.sequence()
		//						.leaf_name<Condition_IsEnemyInRange>("Condition_IsEnemyInRange", 4.0f)
		//						.leaf_name<Action_RotateToEnemy>("Action_RotateToEnemy") // 공격 전 타겟 방향 회전
		//						.leaf_name<Action_AttackEnemy>("Claw Attack",_clawAtk)
		//					.end()
		//					// 1-3. 타겟 추격 (페이즈 2는 더 빠름)
		//					.leaf_name<Action_ChaseEnemy>("2Phase Chasing", 7.0f)
		//				.end()
		//			.end()
		//		.end()
		//		// --- [우선순위 2] 페이즈 1 패턴 (기본) ---
		//		.sequence()
		//			.leaf_name<Condition_IsPhase>("Condition_IsPhase", TainerPhase::PHASE_1)
		//			.selector()
		//				// 2-1. 돌진 (특정 거리 2m~12m 사이일 때만 사용)
		//				.sequence()
		//					.leaf_name<Condition_CheckFlagFalse>("Charge_CD", "is_charge_cd")
		//					.leaf_name<Condition_IsEnemyInDistanceRange>("Charge_Range", 2.0f, 12.0f)
		//					// 이 노드 하나가 포효(1회) -> 회전 -> 10m 돌진을 순차적으로 수행합니다.
		//					.leaf_name<Action_ChargeAttack>("Action_Charge", 18.0f, _chargeAtk)
		//					.leaf_name<Action_SetFlagTrue>("Set_Charge_CD", "is_charge_cd")
		//				.end()
		//				// 2-2. 내려찍기 (가까이 있으면 사용)
		//				.sequence()
		//					.leaf_name<Condition_IsEnemyInRange>("Condition_IsEnemyInRange", 4.0f)
		//					.leaf_name<Action_AttackEnemy>("Slam Attack", _slamAtk)
		//				.end()
		//				// 2-3. 타겟 추격 (페이즈 1 속도)
		//				.leaf_name<Action_ChaseEnemy>("1Phase Chasing", 4.0f)
		//			.end()
		//		.end()
		//		// --- [우선순위 3] 공통: 타겟이 없을 경우 ---
		//		.leaf_name<Action_TargetingNearestPlayer>("Action_TargetingNearestPlayer")
		//	.end()
		//.build();

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
