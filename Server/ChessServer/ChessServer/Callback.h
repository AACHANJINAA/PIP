#pragma once
#include "CommonHeader.h"
namespace chess
{
	void CALLBACK g_recv_callback(DWORD, DWORD, LPWSAOVERLAPPED, DWORD);
	void CALLBACK g_send_callback(DWORD, DWORD, LPWSAOVERLAPPED, DWORD);
}
