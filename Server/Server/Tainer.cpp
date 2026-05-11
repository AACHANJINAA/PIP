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
		// Phase 1: Slam (내려찍기)
		_slamAtk.shape = new JPH::SphereShape(3.0f);
		_slamAtk.posOffset = { 0.0f, 0.0f, 3.5f };
		_slamAtk.damage = 20;
		_slamAtk.cooldown = 1.0f;
		_slamAtk.entityState = common::packet::EntityState::ACTION;
		_slamAtk.actionId = common::packet::ActionID::Tainer::Slam; // 13
		_slamAtk.animationDuration = 1.0f; 
		_slamAtk.attackTiming = 0.5f; 

		// Phase 1: Charge (일반 돌진)
		_chargeAtk.shape = new JPH::SphereShape(3.0f);
		_chargeAtk.posOffset = { 0.0f, 2.0f, 0.0f };
		_chargeAtk.damage = 25;
		_chargeAtk.cooldown = 3.0f;
		_chargeAtk.entityState = common::packet::EntityState::ACTION;
		_chargeAtk.actionId = common::packet::ActionID::Tainer::Charge; // 12
		_chargeAtk.animationDuration = 2.0f;
		_chargeAtk.isContinuous = true;
		_chargeAtk.hitInterval = 0.2f;

		// Phase 2: Claw (난타)
		_clawAtk.shape = new JPH::SphereShape(2.0f);
		_clawAtk.posOffset = { 0.0f, 1.0f, 1.5f };
		_clawAtk.damage = 15;
		_clawAtk.cooldown = 1.5f;
		_clawAtk.entityState = common::packet::EntityState::ACTION;
		_clawAtk.actionId = common::packet::ActionID::Tainer::Claw; // 14
		_clawAtk.animationDuration = 0.5f;

		// Phase 2: Grab (그랩 설정 통합)
		_grabChargeAtk.shape = new JPH::BoxShape(JPH::Vec3(1.5f, 1.0f, 2.5f));
		_grabChargeAtk.posOffset = { 0.0f, 1.0f, 1.2f };
		_grabChargeAtk.damage = 10;
		_grabChargeAtk.cooldown = 15.0f;
		_grabChargeAtk.entityState = common::packet::EntityState::ACTION;
		_grabChargeAtk.actionId = common::packet::ActionID::Tainer::GrabCharge; // 16
		_grabChargeAtk.isGrab = true;

		Tainer::SetupBT();
	}

	void Tainer::SetupBT()
	{
		auto ai = GetComponent<AIComponent>();
		if (!ai) return;
		ai->Initialize();

		auto bb = ai->GetBlackboard();
		bb->set("owner", static_cast<GameObject*>(this));
		bb->set("owner_npc", static_cast<NPC*>(this));
		bb->set("room_id", GetRoomId());
		bb->set("max_hp", static_cast<int>(_maxHp));
		bb->set("spawn_pos", GetPosition());

		BTBuilder builder;
		auto root = builder
			.sequence()
				.leaf<Condition_IsAlive>()
				.selector()
					// --- [전투 시퀀스] 플레이어가 방에 있을 때 ---
					.sequence()
						.leaf<Action_TargetingNearestPlayer>()
						.leaf<Condition_HasEnemy>()
						.selector()
							// [Phase 2] HP 50% 이하 패턴
							.sequence()
								.leaf<Condition_IsHPBelow>(0.5f)
								.selector()
									.sequence() // 페이즈 2 진입 포효 (최초 1회)
										.leaf<Condition_CheckFlagFalse>("is_p2_roar_done")
										.leaf<Action_Roar>(1.5f)
										.leaf<Action_SetFlagTrue>("is_p2_roar_done")
									.end()
									.selector()
										// 1. 통합 그랩 (근거리 즉시 / 중거리 돌진)
										.sequence()
											.leaf<Condition_IsEnemyInDistanceRange>(0.0f, 15.0f)
											.leaf<Action_GrabCharge>(18.0f, _grabChargeAtk)
										.end()
										// 2. 클로 난타
										.sequence()
											.leaf<Condition_IsEnemyInRange>(4.5f)
											.leaf<Action_AttackEnemy>(_clawAtk)
										.end()
										// 3. 페이즈 2 추격 (더 빠름)
										.leaf<Action_ChaseEnemy>(7.0f, 3.0f)
									.end()
								.end()
							.end()
							// [Phase 1] 기본 패턴
							.selector()
								// 1. 일반 돌진 (중거리)
								.sequence()
									.leaf<Condition_IsEnemyInDistanceRange>(7.0f, 15.0f)
									.leaf<Action_ChargeAttack>(16.0f, _chargeAtk)
								.end()
								// 2. 내려찍기 (근거리)
								.sequence()
									.leaf<Condition_IsEnemyInRange>(6.0f)
									.leaf<Action_AttackEnemy>(_slamAtk)
								.end()
								// 3. 페이즈 1 추격
								.leaf<Action_ChaseEnemy>(4.0f, 5.0f)
							.end()
						.end()
					.end()
					// --- [비전투 시퀀스] 플레이어가 없을 때 ---
					.sequence()
						.leaf<Action_FindRandomTarget>(15.0f)
						.leaf<Action_MoveToTarget>(3.0f)
					.end()
				.end()
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
