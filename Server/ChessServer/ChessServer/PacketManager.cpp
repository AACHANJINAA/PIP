#include "pch.h"
#include "PacketManager.h"

namespace chess::packet
{
	void PacketManager::Initialize()
	{
		// Packet.h에 정의된 이름으로 수정
		RegisterHandler(PacketType::C2S_P_LOGIN, Handle_C2S_LOGIN);
		RegisterHandler(PacketType::C2S_P_MOVE, Handle_C2S_MOVE);
		RegisterHandler(PacketType::C2S_P_ATTACK, Handle_C2S_ATTACK);
		RegisterHandler(PacketType::C2S_P_ENTER_ROOM, Handle_C2S_ENTER_ROOM);
		RegisterHandler(PacketType::C2S_P_ROOM_LIST, Handle_C2S_ROOM_LIST);
		RegisterHandler(PacketType::C2S_P_CHAT_IN_ROOM, Handle_C2S_CHAT_IN_ROOM);
	}
}

