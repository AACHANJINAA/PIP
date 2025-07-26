#include "pch.h"
#include "PacketManager.h"

namespace chess::packet
{
    void PacketManager::Initialize()
    {
        // Packet.h에 정의된 이름으로 수정
        RegisterHandler(static_cast<uint16_t>(PacketType::C2S_P_LOGIN), Handle_C2S_LOGIN);
        RegisterHandler(static_cast<uint16_t>(PacketType::C2S_P_MOVE), Handle_C2S_MOVE);
		RegisterHandler(static_cast<uint16_t>(PacketType::C2S_P_ATTACK), Handle_C2S_ATTACK);
    }
}

