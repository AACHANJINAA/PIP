#include "Callback.h"

#include "main.h"
#include "server.h"


namespace chess
{
	void CALLBACK g_send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag)
	{
		server::EXP_OVER* o = reinterpret_cast<server::EXP_OVER*>(p_over);
		delete o;
	}

	void CALLBACK g_recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag)
	{
		auto my_id = reinterpret_cast<long long>(p_over->hEvent);
		auto it = main::g_users.find(my_id);
		if (it != main::g_users.end())
		{
			it->second.recv_callback(err, num_bytes, p_over, flag);
		}
	}
}
