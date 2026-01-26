#pragma once
#include "Packet.h"
#include "server.h"

namespace PIP::packet
{
	using namespace common::packet;
	PacketStream MakeSpawnPlayerPacket(std::shared_ptr<SERVER::SESSION> session);


	void Handle_C2S_LOGIN(std::shared_ptr<SERVER::SESSION> session, packet::PacketStream& stream);
	void Handle_C2S_MOVE(std::shared_ptr<SERVER::SESSION> session, packet::PacketStream& stream);
	void Handle_C2S_ATTACK(std::shared_ptr<SERVER::SESSION> session, packet::PacketStream& stream);
	void Handle_C2S_ENTER_ROOM(std::shared_ptr<SERVER::SESSION> session, packet::PacketStream& stream);
	void Handle_C2S_ROOM_LIST(std::shared_ptr<SERVER::SESSION> session, packet::PacketStream& stream);
	void Handle_C2S_CHAT_IN_ROOM(std::shared_ptr<SERVER::SESSION> session, packet::PacketStream& stream);
	void Handle_C2S_ACTION(std::shared_ptr<PIP::SERVER::SESSION> session, PIP::packet::PacketStream& stream);
}
