#pragma once
#include "CommonHeader.h"
#include "Packet.h"
#include "server.h"

namespace chess::packet
{
	void Handle_C2S_LOGIN(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream);
	void Handle_C2S_MOVE(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream);
}
