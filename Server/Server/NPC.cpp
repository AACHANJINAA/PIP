#include "pch.h"
#include "NPC.h"

namespace PIP
{
    NPC::NPC(int npc_id, int npc_type, int room_id, common::Vec3 position)
        : _npc_id(npc_id), _npc_type(npc_type), _room_id{ room_id }, _position(position)
    {
        // NPC 생성 시 로그 (필요 시 사용)
        // MYLOG("[NPC] Created NPC " << _npcId << " of type " << _npcType);
    }

    NPC::~NPC()
    {
        // NPC 소멸 시 로그 (필요 시 사용)
        // MYLOG("[NPC] Destroyed NPC " << _npcId);
    }
}

