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

		bb->set("owner", static_cast<GameObject*>(this));
		bb->set("owner_npc", static_cast<NPC*>(this)); // 명시적 캐스팅
		bb->set("room_id", _room_id);
		bb->set("path_search_cooldown", 0.0f);
		bb->set("last_search_pos", common::Vec3{ 0,0,0 });

		// [중요] 부활 시 소실 방지를 위한 순찰 데이터 복구
		if (!_patrolPoints.empty()) {
			bb->set("patrol_points", _patrolPoints);
			bb->set("patrol_index", 0);
		}

		BTBuilder builder;
		auto root = builder
			.sequence()
			.leaf<Condition_IsAlive>()
			// [테스트용] 추격/전투 로직을 빼고 순찰만 수행
			.sequence()
				.leaf<Action_SetNextPatrolPos>()
				.leaf<Action_FindPath>("MainStage_NavMesh")
				.leaf<Action_FollowPath>(3.0f)
			.end()
		.end()
		.build();

		root->set_blackboard(bb);
		_aiComponent->SetBehaviorTree(root);
	}
}
