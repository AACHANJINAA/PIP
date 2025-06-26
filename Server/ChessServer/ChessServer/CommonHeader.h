#pragma once
#include "pch.h"

namespace chess
{
	extern HANDLE g_iocp;
	extern SOCKET g_s_socket;
	extern std::atomic<int> g_new_id;

	namespace packet
	{
		struct PositionPacket;
	}

	namespace server
	{
		class SESSION;
	}

	extern std::unordered_map<long long, chess::packet::PositionPacket> g_positions;
	extern concurrency::concurrent_unordered_map< long long, std::shared_ptr<server::SESSION>> g_users;

	
	
}