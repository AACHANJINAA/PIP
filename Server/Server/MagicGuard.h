#pragma once
#include "NPC.h"

namespace PIP::GAME
{
	class MagicGuard : public NPC
	{
	public:
		MagicGuard(int64_t npc_id, int room_id, common::Vec3 position, int32_t hp);
		~MagicGuard() override = default;

		void SetupBT() override;
	};
}
