#pragma once

namespace PIP
{
	namespace server
	{
		extern HANDLE g_iocp;
		extern std::atomic<int> g_new_id;

		class SESSION;
	}
}
