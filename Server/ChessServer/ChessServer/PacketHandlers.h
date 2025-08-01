#pragma once
#include "CommonHeader.h"
#include "Packet.h"
#include "server.h"

namespace chess::packet
{
	PacketStream MakeEnterPacket(std::shared_ptr<chess::server::SESSION> session);


	void Handle_C2S_LOGIN(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream);
	void Handle_C2S_MOVE(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream);
	void Handle_C2S_ATTACK(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream);
	void Handle_C2S_ENTER_ROOM(std::shared_ptr<server::SESSION> session, packet::PacketStream& stream);
	void Handle_C2S_ROOM_LIST(std::shared_ptr<server::SESSION> session, packet::PacketStream& stream);
	void Handle_C2S_CHAT_IN_ROOM(std::shared_ptr<server::SESSION> session, packet::PacketStream& stream);
}
