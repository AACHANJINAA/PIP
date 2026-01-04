#pragma once
#include "Room.h"
#include "Player.h"

namespace PIP::server
{
	class Server;


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
		EXP_OVER(IO_OP io_op) : _io_op(io_op), _accept_socket(-1)
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

	class SESSION : public std::enable_shared_from_this<SESSION>
	{
	public:
		SOCKET								_c_socket;
		long long							_id;
		int									_logic_thread_idx; // 담당 로직 스레드의 인덱스
		int									_room_id = -1;

		EXP_OVER							_recv_over{ IO_RECV };

		std::vector<char>					_recv_buffer; // 수신 버퍼: 클라이언트로부터 받은 데이터를 임시 저장

		std::atomic<SESSION_STATE>			_state;
		Player								_player;
	public:
		SESSION();
		SESSION(long long session_id, SOCKET s, int logic_index);
		~SESSION();

		void do_recv();
		void do_send(const char* data, size_t size);
		void OnRecv(size_t len, Server* server_ptr);

	};




	struct TimerJob
	{
		std::chrono::steady_clock::time_point	_execute_time;
		std::function<void()>					_task;
		int										_owner_thread_idx;
		bool operator>(const TimerJob& other) const
		{
			return _execute_time > other._execute_time;
		}
	};
	struct LogicJob
	{
		std::function<void()> _task;
	};

	struct LogicWorker
	{
		std::thread thread;
		concurrency::concurrent_queue<LogicJob> queue;
		//concurrency::concurrent_priority_queue<TimerJob, std::greater<TimerJob>> _timer_queue;
		std::priority_queue<TimerJob, std::vector<TimerJob>, std::greater<TimerJob>> _timer_queue;

		LogicWorker() = default;
		LogicWorker(std::thread t) : thread(std::move(t)) {}
		LogicWorker(LogicWorker&& other) noexcept
			: thread(std::move(other.thread)), queue(std::move(other.queue))
		{} // emplace_back에서 이동 생성자 호출하기 위함

		LogicWorker(const LogicWorker&) = delete;
		LogicWorker& operator=(const LogicWorker&) = delete;
	};

	class Room;
	class Server : public Singleton<Server>
	{
		friend class Singleton<Server>;
	private:
		Server();
		~Server();
	public:

		void Start(int io_thread_count, int logic_thread_count);
		void Stop();

		// Room에서 타이머 잡을 추가할 수 있도록 헬퍼 함수 추가
		void AddTimerJob(int worker_idx, std::chrono::milliseconds delay, std::function<void()> task);

		// 로직 큐를 얻어오기 위한 public 메소드
		concurrency::concurrent_queue<LogicJob>* get_logic_queue(int worker_idx);
		Room* GetRoom(int room_id); // [추가] 특정 룸을 얻어오기 위한 메소드
		int GetLogicWorkerCount() const { return static_cast<int>(_logic_workers.size()); }

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
		HANDLE						_iocp;
		static std::atomic<int>		_new_id;
		SOCKET						_listen_socket;
		EXP_OVER					_accept_over;

		std::vector<std::thread>	_io_threads;
		std::vector<LogicWorker>	_logic_workers;

		std::atomic<bool>			_is_running;
		std::atomic<int>			_logic_thread_balancer; // 새 세션을 분배하기 위한 카운터

		// [추가] 서버가 관리하는 룸 목록
		std::vector<std::unique_ptr<Room>> _rooms;

		concurrency::concurrent_unordered_map<long long, std::shared_ptr<SESSION>> _sessions; // 임시 세션 저장소
	};


}