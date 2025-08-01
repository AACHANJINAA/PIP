#pragma once
#include "pch.h"
#include "CommonHeader.h"
#include "Packet.h"
#include "Room.h"

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
		ST_LOBBY = 1,
		ST_INGAME = 2,
		ST_CLOSE = 3,
	};

	class Server;
	class SESSION : public std::enable_shared_from_this<SESSION>
	{
	public:
		SOCKET						_c_socket;
		long long					_id;
		int                         _logic_thread_idx; // 담당 로직 스레드의 인덱스
		int							_room_id = -1;

		EXP_OVER					_recv_over { IO_RECV };
		//unsigned char				_remained;

		std::vector<char>			_recv_buffer; // 세션별 수신 버퍼: 클라이언트로부터 받은 데이터를 임시 저장

		std::atomic<SESSION_STATE>	_state;
		short						_x, _y;
		std::string					_name;
		std::atomic<short>			_hp;
		short						_max_hp;
		
	public:
		SESSION();
		SESSION(long long session_id, SOCKET s, int logic_index);
		~SESSION();

		void do_recv();
		void do_send(const char* data, size_t size);
		void OnRecv(size_t len, Server* server_ptr);

		void send_player_info_packet();
	};

	struct LogicPacket //[추가] 로직 스레드에 전달될 패킷 구조체
	{
		std::shared_ptr<SESSION> session;
		std::vector<char> packet_data;
	};

	class Room; // [추가] 방 클래스 전방 선언
	class Server : public Singleton<Server>
	{
		friend class Singleton<Server>;
	private:
		Server();
		~Server();
	public:

		void Start(int io_threads, int logic_threads);
		void Stop();

		// 로직 큐에 접근하기 위한 public 메서드
		auto get_logic_queue(int queue_idx) -> ConcurrentQueue<LogicPacket>*;
		auto GetRoom(int room_id) -> Room*; // [추가] 특정 방에 접근하기 위한 메서드
	private:
		// 기존 전역 함수들을 클래스 멤버로 가져옴
		void do_accept();
		void IO_worker();
		void Logic_worker(int thread_idx);
		void register_new_session(SOCKET client_socket);

	private:
		SOCKET _listen_socket;
		EXP_OVER _accept_over;

		std::vector<std::thread> _io_threads;
		std::vector<std::thread> _logic_threads;

		// 각 로직 스레드에 할당될 ConcurrentQueue
		std::vector<std::unique_ptr<ConcurrentQueue<LogicPacket>>> _logic_queues;

		std::atomic<bool> _is_running;
		std::atomic<int> _logic_thread_balancer; // 새 세션을 분배하기 위한 카운터

		// [추가] 서버가 관리할 방 목록
		std::vector<std::unique_ptr<Room>> _rooms;
	};
	
	
}
