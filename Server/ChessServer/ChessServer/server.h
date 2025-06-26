#pragma once
#include "pch.h"
#include "CommonHeader.h"
#include "Packet.h"

namespace chess::server
{
	enum IO_OP : std::uint8_t
	{
		IO_RECV = 0,
		IO_SEND = 1,
		IO_ACCEPT = 2,
		IO_ERROR = 255
	};

	class EXP_OVER
	{
	public:
		EXP_OVER(IO_OP io_op) : _io_op(io_op)
		{
			ZeroMemory(&_over, sizeof(_over));

			_wsabuf[0].buf = reinterpret_cast<CHAR*>(_buffer.data());
			_wsabuf[0].len = static_cast<ULONG>(_buffer.size());
		}

		WSAOVERLAPPED			_over;
		IO_OP					_io_op;
		SOCKET					_accept_socket;
		std::array<UCHAR,1024>	_buffer;
		std::array<WSABUF, 1>	_wsabuf;
	};

	extern EXP_OVER g_accept_over;


	enum class SESSION_STATE : char
	{
		ST_FREE = 0,
		ST_INGAME = 1,
		ST_CLOSE = 2,
	};

	class SESSION : public std::enable_shared_from_this<SESSION>
	{
	public:
		SOCKET						_c_socket;
		long long					_id;

		EXP_OVER					_recv_over { IO_RECV };
		//unsigned char				_remained;

		std::vector<char>			_recv_buffer; // 세션별 수신 버퍼: 클라이언트로부터 받은 데이터를 임시 저장

		std::atomic<SESSION_STATE>	_state;
		short						_x, _y;
		std::string					_name;
		
	public:
		SESSION();
		SESSION(long long session_id, SOCKET s);
		~SESSION();

		void do_recv();
		void do_send(const char* data, size_t size);
		void OnRecv(size_t len);

		void send_player_info_packet();

		/*void send_player_pos();
		void process_packet(unsigned char* p);*/
	};

	void do_accept(SOCKET s_socket, EXP_OVER& accept_over);
	void worker();
	
}
