#include "pch.h"
#include "MagicGuard.h"
#include "AIComponent.h"
#include "BT_Nodes.h"

namespace PIP::GAME
{
	MagicGuard::MagicGuard(int64_t npc_id, int room_id, common::Vec3 position, int32_t hp)
		: NPC(npc_id, common::packet::NPCType::MagicGuard, room_id, position, hp)
	{
		MagicGuard::SetupBT();
	}

	void MagicGuard::SetupBT()
	{
		if (!_aiComponent) return;
		_aiComponent->Initialize();
		auto bb = _aiComponent->GetBlackboard();

		bb->set("owner",                static_cast<GameObject*>(this));
		bb->set("owner_npc",            static_cast<NPC*>(this));
		bb->set("room_id",              _room_id);
		bb->set("path_search_cooldown", 0.0f);
		bb->set("last_search_pos",      common::Vec3{ 0, 0, 0 });

		// [중요] 부활 시 소실 방지를 위한 순찰 데이터 복구
		if (!_patrolPoints.empty()) {
			bb->set("patrol_points", _patrolPoints);
			bb->set("patrol_index",  0);
		}

		// ----------------------------------------------------------------
		// MagicGuard 전용 근접 공격 설정
		// ----------------------------------------------------------------
		AttackConfig normalAtk;
		normalAtk.shape             = new JPH::SphereShape(1.2f);   // 반경 1.2m 구체
		normalAtk.posOffset         = { 0.0f, 1.0f, 1.2f };        // 전방 1.2m 지점
		normalAtk.damage            = 15.0f;
		normalAtk.cooldown          = 1.5f;
		normalAtk.animationDuration = 0.8f;
		normalAtk.attackTiming      = 0.4f;                          // 애니메이션 중반에 판정
		normalAtk.entityState       = common::packet::EntityState::ACTION;
		normalAtk.actionId          = 1;                             // 클라이언트 기본 공격 ID

		// ----------------------------------------------------------------
		// BT 구조
		//
		// Sequence (루트)
		// ├── Condition_IsAlive
		// └── Selector
		//     ├── [P1] 전투/추격 Sequence   ← target_enemy 있을 때
		//     │   ├── Condition_HasEnemy
		//     │   ├── Condition_IsTargetInLeashRange(20)
		//     │   └── Selector
		//     │       ├── Sequence (2m 이내 → 공격)
		//     │       │   ├── Condition_IsEnemyInRange(2.0f)
		//     │       │   └── Action_AttackEnemy(normalAtk)
		//     │       └── Sequence (A* 추격)
		//     │           ├── Action_UpdateEnemyPosToTarget
		//     │           ├── Action_FindPath("MainStage_NavMesh")
		//     │           └── Action_FollowPath(4.5f)
		//     └── [P2] 순찰 Sequence        ← target_enemy 없을 때
		//         ├── Condition_DetectPlayer(8.0f) [Inverter]
		//         │   감지 성공 → target_enemy 세팅 → FAILURE 반환
		//         │   → 이 Sequence 실패 → 다음 틱 P1으로 자동 전환
		//         ├── Action_SetNextPatrolPos
		//         ├── Action_FindPath("MainStage_NavMesh")
		//         └── Action_FollowPath(3.0f)
		// ----------------------------------------------------------------
		BTBuilder builder;
		auto root = builder
			.sequence()                                     // [루트] 생존 시에만 동작
				.leaf<Condition_IsAlive>()
				.selector()                                 // 우선순위 결정기
					// ── [P1] 전투/추격 시퀀스 ──────────────────────────
					.sequence()
						.leaf<Condition_HasEnemy>()                         // target_enemy 존재?
						.leaf<Condition_IsTargetInLeashRange>(20.0f)        // 20m 내 추격 유지
						.selector()                                         // 공격 vs 추격
							// 1-a. 2m 이내 → 공격
							.sequence()
								.leaf<Condition_IsEnemyInRange>(2.0f)
								.leaf<Action_AttackEnemy>(normalAtk)
							.end()
							// 1-b. 공격 사거리 밖 → A* 추격
							.sequence()
								.leaf<Action_UpdateEnemyPosToTarget>()
								.leaf<Action_FindPath>("MainStage_NavMesh")
								.leaf<Action_FollowPath>(4.5f)
							.end()
						.end()
					.end()

					// ── [P2] 순찰 시퀀스 ────────────────────────────────
					.sequence()
						// Inverter: 감지 성공(true) → FAILURE → Sequence 종료
						//           → 다음 틱 P1(전투)이 먼저 평가됨
						.leaf<Condition_DetectPlayer>(DecoratorType::Inverter, 8.0f)
						.leaf<Action_SetNextPatrolPos>()
						.leaf<Action_FindPath>("MainStage_NavMesh")
						.leaf<Action_FollowPath>(3.0f)
					.end()

				.end()
			.end()
			.build();

		root->set_blackboard(bb);
		_aiComponent->SetBehaviorTree(root);
	}
}
