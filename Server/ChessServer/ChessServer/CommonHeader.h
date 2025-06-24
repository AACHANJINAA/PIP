#pragma once
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <iostream>
#include <atomic>
#include <unordered_map>
#include <thread>
#include <array>
#include <ranges>
#include <vector>
#include <concurrent_unordered_map.h>
#include <mutex>

#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "MSWSock.LIB")

namespace chess
{
	HANDLE g_iocp;
	SOCKET g_s_socket;
	std::atomic<int> g_new_id = 0;

	namespace packet
	{
		struct PositionPacket;
	}

	namespace server
	{
		class SESSION;
	}

	extern std::unordered_map<long long, chess::packet::PositionPacket> g_positions;
	extern concurrency::concurrent_unordered_map< long long, std::atomic<std::shared_ptr<server::SESSION>>> g_users;

	void print_error(const char* msg, int err_no)
	{
		if (WSA_IO_PENDING == err_no)
		{
			return;
		}
		WCHAR* lpMsgBuf;
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, err_no,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf, 0, NULL);
		std::cout << msg;
		std::wcout << L" 에러 " << lpMsgBuf << std::endl;
#ifdef _DEBUG
		while (true); // 디버깅 용 그냥 죽으면 안되니까
#endif

		LocalFree(lpMsgBuf);
	}

	
}