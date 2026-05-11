#pragma once
#include "Packet.h"
#include "server.h"

namespace PIP::packet
{
	using namespace common::packet;
	PacketStream MakeSpawnPlayerPacket(const std::shared_ptr<SERVER::SESSION>& session);


	void Handle_C2S_LOGIN(const std::shared_ptr<SERVER::SESSION>& session, packet::PacketStream& stream);
	void Handle_C2S_MOVE(const std::shared_ptr<SERVER::SESSION>& session, packet::PacketStream& stream);
	void Handle_C2S_ATTACK(const std::shared_ptr<SERVER::SESSION>& session, packet::PacketStream& stream);
	void Handle_C2S_ENTER_ROOM(const std::shared_ptr<SERVER::SESSION>& session, packet::PacketStream& stream);
	void Handle_C2S_ROOM_LIST(const std::shared_ptr<SERVER::SESSION>& session, packet::PacketStream& stream);
	void Handle_C2S_CHAT_IN_ROOM(const std::shared_ptr<SERVER::SESSION>& session, packet::PacketStream& stream);
	void Handle_C2S_ACTION(const std::shared_ptr<SERVER::SESSION>& session, PIP::packet::PacketStream& stream);
	void Handle_C2S_PLAYER_READY(const std::shared_ptr<SERVER::SESSION>& session, PIP::packet::PacketStream& stream);
	void Handle_C2S_DEBUG_COMMAND(const std::shared_ptr<SERVER::SESSION>& session, PIP::packet::PacketStream& stream);
}
