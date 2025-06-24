#pragma once
#include "Callback.h"
#include "CommonHeader.h"
#include "Packet.h"

namespace chess::server
{
	class EXP_OVER
	{
	public:
		EXP_OVER(long long id, const std::vector<char>& data);

		WSAOVERLAPPED			_send_over;
		long long				_id;
		std::array<char, 1024>	_send_buffer;
		std::array<WSABUF, 1>	_send_wsabuf;
	};

	class SESSION
	{
	private:
		SOCKET						_c_socket;
		long long					_id;
		WSAOVERLAPPED				_recv_over;
		std::array<char, 1024>		_recv_buffer;
		std::array<WSABUF, 1> 		_recv_wsabuf;

		void do_recv();
		void process_command(packet::CommandType cmd);
	public:
		SESSION();
		SESSION(long long session_id, SOCKET s);
		~SESSION();

		void recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag);
		void spread_users();
		
		void do_send(long long id, const std::vector<char>& data);
		void send_id();
	};
}
