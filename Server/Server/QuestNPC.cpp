#include "pch.h"
#include "QuestNPC.h"
#include "AIComponent.h"

namespace PIP::GAME
{
	QuestNPC::QuestNPC(int64_t npc_id, NPCType npc_type, int room_id, common::Vec3 position, int32_t hp)
		: NPC(npc_id, npc_type, room_id, position, hp)
	{
		// 퀘스트 NPC는 데미지를 받지 않으므로 ValidateHit에서 false를 반환하여 무적 판정합니다.
		// HitboxComponent는 삭제 기능이 없어 그대로 두되 사용되지 않습니다.
		QuestNPC::SetupBT();
	}
	QuestNPC::~QuestNPC()
	{
	}

	void QuestNPC::SetupBT()
	{
		if (!_aiComponent) return;

		_aiComponent->Initialize();
		//가만히 있는 NPC
		return;
	}

	bool QuestNPC::ValidateHit(JPH::PhysicsSystem* physics,
		const JPH::Shape* attackShape,
		const JPH::RMat44& attackTransform,
		uint32_t timestamp,
		GameObject* attacker,
		int32_t damage)
	{
		// 퀘스트 NPC는 무적이므로 타격을 무시함
		return false;
	}
}
