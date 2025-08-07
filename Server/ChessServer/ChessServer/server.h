#pragma once
#include "pch.h"
#include "CommonHeader.h"
#include "PacketStream.h"
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

	struct LogicPacket //[추가] 로직 스레드에 전달될 패킷 구조체
	{
		std::shared_ptr<SESSION> session; // 이 세션이
		packet::PacketStream packet_stream; // 그냥 스트림을 가져오도록 변경
	};

	// [추가] 로직 스레드와 그에 해당하는 작업 큐를 묶는 구조체
	struct LogicWorker
	{
		std::thread thread;
		ConcurrentQueue<LogicPacket> queue;

		LogicWorker(std::thread t) : thread(std::move(t)) {}
		LogicWorker(LogicWorker&& other) noexcept
			: thread(std::move(other.thread)), queue(std::move(other.queue))
		{}
		LogicWorker& operator=(LogicWorker&& other) noexcept
		{
			if (this != &other)
			{
				thread = std::move(other.thread);
				queue = std::move(other.queue);
			}
			return *this;
		}

		// 복사 생성/할당 = delete
		LogicWorker(const LogicWorker&) = delete;
		LogicWorker& operator=(const LogicWorker&) = delete;
	};

	class Room;
	class SESSION; // [추가] 전방 클래스 선언
	class Server : public Singleton<Server>
	{
		friend class Singleton<Server>;
	private:
		Server();
		~Server();
	public:

		void Start(int io_threads, int logic_threads);
		void Stop();

		// 로직 큐를 얻어오기 위한 public 메소드
		auto get_logic_queue(int worker_idx) -> ConcurrentQueue<LogicPacket>*;
		auto GetRoom(int room_id) -> Room*; // [추가] 특정 룸을 얻어오기 위한 메소드

		// [추가] 세션 관리를 위한 함수들
		void AddSession(long long session_id, std::shared_ptr<SESSION> session);
		std::shared_ptr<SESSION> GetSession(long long session_id);
		void RemoveSession(long long session_id);
	private:
		// 서버 내부 동작 함수들 (클래스 외부에서 호출될 필요 없음)
		void do_accept();
		void IO_worker();
		void Logic_worker(int thread_idx);
		void register_new_session(SOCKET client_socket);

	private:
		SOCKET					 _listen_socket;
		EXP_OVER				 _accept_over;

		std::vector<std::thread> _io_threads;
		std::vector<LogicWorker> _logic_workers;

		std::atomic<bool>		 _is_running;
		std::atomic<int>		 _logic_thread_balancer; // 새 세션을 분배하기 위한 카운터

		// [추가] 서버가 관리하는 룸 목록
		std::vector<std::unique_ptr<Room>> _rooms;

		concurrency::concurrent_unordered_map<long long, std::shared_ptr<SESSION>> _sessions;
	};

	class SESSION : public std::enable_shared_from_this<SESSION>
	{
	public:
		SOCKET						_c_socket;
		long long					_id;
		int                         _logic_thread_idx; // 담당 로직 스레드의 인덱스
		int							_room_id = -1;

		EXP_OVER					_recv_over { IO_RECV };
		//unsigned char				_remained;

		std::vector<char>			_recv_buffer; // 수신 버퍼: 클라이언트로부터 받은 데이터를 임시 저장

		std::atomic<SESSION_STATE>	_state;

		// 플레이어 정보
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

}