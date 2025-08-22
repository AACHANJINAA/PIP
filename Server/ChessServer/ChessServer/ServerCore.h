#pragma once

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
	
}