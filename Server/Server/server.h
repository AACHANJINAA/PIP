#pragma once


#include "Room.h"
#include "Player.h"

template<typename T>
class ThreadSafeStack {
	std::stack<T> _stack;
	std::mutex _mutex;
public:
	void push(T val) {
		std::lock_guard<std::mutex> lock(_mutex);
		_stack.push(val);
	}
	bool try_pop(T& val) {
		std::lock_guard<std::mutex> lock(_mutex);
		if (_stack.empty()) return false;
		val = _stack.top();
		_stack.pop();
		return true;
	}
};

namespace PIP::SERVER
{
	class Server;
	class SESSION;
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
		static constexpr size_t BUFFER_SIZE = 4096;

		EXP_OVER(IO_OP io_op, const std::shared_ptr<SESSION>& session = nullptr)
					:_io_op(io_op), _accept_socket(INVALID_SOCKET), _session_ref(session)
		{
			ZeroMemory(&_over, sizeof(_over));

			_wsabuf[0].buf = reinterpret_cast<CHAR*>(_buffer.data());
			_wsabuf[0].len = static_cast<ULONG>(_buffer.size());
		}

		WSAOVERLAPPED			_over;
		IO_OP					_io_op;
		SOCKET					_accept_socket;
		std::array<UCHAR, BUFFER_SIZE>	_buffer;
		std::array<WSABUF, 1>	_wsabuf;

		// [핵심] I/O가 진행되는 동안 세션이 파괴되거나 풀로 돌아가지 않게 보장
		std::shared_ptr<SESSION>     _session_ref;
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
		int64_t								_id;
		int									_logic_thread_idx; // 담당 로직 스레드의 인덱스
		int									_room_id = -1;

		//EXP_OVER							_recv_over{ IO_RECV };

		std::vector<char>					_recv_buffer; // 수신 버퍼: 클라이언트로부터 받은 데이터를 임시 저장

		std::atomic<SESSION_STATE>			_state;
		std::shared_ptr<GAME::Player>		_player;

		std::unordered_set<int>				_viewedNpcs;
	public:
		SESSION();
		SESSION(int64_t session_id, SOCKET s, int logic_index);
		~SESSION();

		void do_recv();
		void do_send(const char* data, size_t size);
		void on_recv(EXP_OVER* eo, size_t len, Server* server_ptr);

		// 세션 재사용을 위한 초기화 함수
		void init(SOCKET s, int64_t id, int logic_idx);
		void disconnect();
		void clear();
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
		void AddSession(int64_t session_id, std::shared_ptr<SESSION> session);
		std::shared_ptr<SESSION> GetSession(int64_t session_id);
		void RemoveSession(int64_t session_id);

		// 풀에서 세션을 꺼내 shared_ptr로 래핑 (커스텀 deleter 포함)
		std::shared_ptr<SESSION> AcquireSession(SOCKET s, int64_t id, int logic_idx);
		// 풀로 반납하는 함수 (deleter 호출)
		void ReleaseSession(SESSION* session);

	private:
		// 서버 내부 동작 함수들 (클래스 외부에서 호출될 필요 없음)
		void do_accept();
		void IO_worker();
		void Logic_worker(int thread_idx);
		void register_new_session(SOCKET client_socket);

	private:
		HANDLE						_iocp;
		static std::atomic<int64_t>	_new_id;
		SOCKET						_listen_socket;
		EXP_OVER					_accept_over;

		std::vector<std::thread>	_io_threads;
		std::vector<LogicWorker>	_logic_workers;

		std::atomic<bool>			_is_running;
		std::atomic<int>			_logic_thread_balancer; // 새 세션을 분배하기 위한 카운터

		// [추가] 서버가 관리하는 룸 목록
		std::vector<std::unique_ptr<Room>> _rooms;

		concurrency::concurrent_unordered_map<int64_t, std::shared_ptr<SESSION>> _sessions; // 임시 세션 저장소
		ThreadSafeStack<SESSION*> _session_pool;

		static std::atomic<int64_t> _actor_id_gen;
	};


}