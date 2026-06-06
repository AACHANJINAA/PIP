#pragma once
#include "NPC.h"

namespace PIP::GAME
{
	class QuestNPC : public NPC
	{
	public:
		QuestNPC(int64_t npc_id, NPCType npc_type, int room_id, common::Vec3 position, int32_t hp);
		~QuestNPC() override;
		void SetupBT() override;

		bool ValidateHit(JPH::PhysicsSystem* physics,
			const JPH::Shape* attackShape,
			const JPH::RMat44& attackTransform,
			uint32_t timestamp,
			GameObject* attacker,
			int32_t damage) override;
	};
}
