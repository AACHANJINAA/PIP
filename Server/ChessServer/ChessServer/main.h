#pragma once
#include "CommonHeader.h"
#include "Packet.h"
#include "server.h"

namespace chess::main
{
	extern std::unordered_map<long long, chess::packet::PositionPacket> g_positions;
	extern std::unordered_map<long long, chess::server::SESSION> g_users;
	
	void error_display(const char* msg, int err_no);
	
}