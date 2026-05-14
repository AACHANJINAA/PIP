#include "pch.h"
#include "MagicGuard.h"
#include "AIComponent.h"
#include "BT_Nodes.h"

namespace PIP::GAME
{
	MagicGuard::MagicGuard(int64_t npc_id, int room_id, common::Vec3 position, int32_t hp)
		: NPC(npc_id, common::packet::NPCType::MagicGuard, room_id, position, hp)
	{
	}

	void MagicGuard::SetupBT()
	{
		auto ai = GetComponent<AIComponent>();
		if (!ai) return;

		BTBuilder builder;
		builder.selector()
			.sequence() // 타겟 추격
				.leaf<Condition_HasEnemy>()
				.leaf<Action_FindPath>("MainStage_NavMesh")
				.leaf<Action_FollowPath>(15.0f) // 추격 속도 (빠르게)
			.end()
			.sequence() // 주변 순찰
				.leaf<Action_FindRandomTarget>(25.0f) // 20m 범위 내 랜덤 위치
				.leaf<Action_FindPath>("MainStage_NavMesh")
				.leaf<Action_FollowPath>(5.0f) // 순찰 속도 (천천히)
			.end()
		.end();

		ai->SetBehaviorTree(builder.build());

		auto bb = ai->GetBlackboard();
		bb->set("owner_npc", static_cast<NPC*>(this));
		bb->set("owner", static_cast<GameObject*>(this));
		bb->set("room_id", _room_id);
		
		// 스테이지별 네브메쉬 이름 설정
		// TODO: Room 초기화 시점에 설정되도록 연동 필요
		bb->set("navmesh_name", std::string("MainStage_NavMesh")); 
	}
}
