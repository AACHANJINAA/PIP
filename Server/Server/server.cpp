#include "pch.h"
#include "server.h"

#include "LuaManager.h"
#include "Player.h"
#include "MapDataManager.h"
#include "PacketManager.h"

namespace PIP::SERVER
{
	// ... (기존 SESSION 및 Server 초기화 코드 동일) ...
	using packet::PacketType;
	std::string PacketTypeToString(PacketType type)
	{
		switch (type)
		{
		case PacketType::error:
			return "error";
		case PacketType::C2S_P_LOGIN:
			return "C2S_P_LOGIN";
		case PacketType::S2C_P_LOGIN_ACK:
			return "S2C_P_LOGIN_ACK";
		case PacketType::S2C_P_LEAVE:
			return "S2C_P_LEAVE";
		case PacketType::S2C_P_SPAWN_PLAYER:
			return "S2C_P_SPAWN_PLAYER";
		case PacketType::S2C_P_MOVE:
			return "S2C_P_MOVE";
		case PacketType::C2S_P_MOVE:
			return "C2S_P_MOVE";
		case PacketType::C2S_P_ACTION:
			return "C2S_P_ACTION";
		/*case PacketType::S2C_P_ATTACK:
			return "S2C_P_ATTACK";*/
		case PacketType::S2C_P_PLAYER_ATTACK:
			return "S2C_P_PLAYER_ATTACK";
		case PacketType::S2C_P_NPC_ATTACK:
			return "S2C_P_NPC_ATTACK";
		case PacketType::C2S_P_ENTER_ROOM:
			return "C2S_P_ENTER_ROOM";
		case PacketType::S2C_P_ENTER_ROOM_ACK:
			return "S2C_P_ENTER_ROOM_ACK";
		case PacketType::C2S_P_ROOM_LIST:
			return "C2S_P_ROOM_LIST";
		case PacketType::S2C_P_ROOM_LIST_ACK:
			return "S2C_P_ROOM_LIST_ACK";
		case PacketType::C2S_P_CHAT_IN_ROOM:
			return "C2S_P_CHAT_IN_ROOM";
		case PacketType::S2C_P_CHAT_IN_ROOM:
			return "S2C_P_CHAT_IN_ROOM";
		case PacketType::S2C_NPC_SPAWN:
			return "S2C_NPC_SPAWN";
		case PacketType::S2C_NPC_MOVE:
			return "S2C_NPC_MOVE";
		default:
			return "Unknown";
		}
	}

	SESSION::SESSION() : _state{ SESSION_STATE::ST_FREE }
	{
		MYERROR("Default Constructor called, this should not happen!" << std::endl);
	}
	// server.cpp
	SESSION::SESSION(int64_t session_id, SOCKET s, int logic_index)
		: _c_socket{ s }, _id{ session_id }, _logic_thread_idx{ logic_index }
	{
		_state = SESSION_STATE::ST_LOBBY;
		_player = std::make_shared<GAME::Player>(session_id);
	}
	SESSION::~SESSION()
	{
		MYLOG("[SESSION " << _id << "] Session destroyed. Name: " << _player->GetName());
		// TODO: Logic_Worker 에서 세션 종료 패킷을 보내는 로직 추가 필요
		closesocket(_c_socket);
	}
	void SESSION::do_recv()
	{
		// [수정] 멤버 변수 _recv_over 대신 동적으로 생성하여 shared_ptr 주입
		// 이렇게 해야 OS가 Recv를 완료할 때까지 세션 객체가 살아있음을 보장함
		EXP_OVER* eo = new EXP_OVER(IO_RECV, shared_from_this());

		DWORD recv_flag = 0;
		// eo->_buffer를 직접 사용하도록 수정
		auto ret = WSARecv(_c_socket, eo->_wsabuf.data(), 1, NULL, &recv_flag, &eo->_over, NULL);

		if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
			delete eo; // 즉시 실패 시 참조 해제
			// Disconnect 처리
		}
	}

	void SESSION::do_send(const char* data, size_t size)
	{
		if (size > EXP_OVER::BUFFER_SIZE) return;

		// [수정] shared_from_this()를 넘겨서 Send 완료 전까지 세션 보호
		EXP_OVER* o = new EXP_OVER(IO_SEND, shared_from_this());
		memcpy(o->_buffer.data(), data, size);
		o->_wsabuf[0].len = static_cast<ULONG>(size);

		DWORD size_sent;
		if (WSASend(_c_socket, o->_wsabuf.data(), 1, &size_sent, 0, &o->_over, NULL) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				delete o;
			}
		}
	}
	void SESSION::on_recv(EXP_OVER* eo, size_t len, Server* server_ptr)
	{
		_recv_buffer.insert(_recv_buffer.end(), eo->_buffer.data(), eo->_buffer.data() + len);
		size_t processed_bytes = 0;
		while (true)
		{
			if (_recv_buffer.size() - processed_bytes < sizeof(packet::PacketHeader)) break;

			packet::PacketHeader* header = reinterpret_cast<packet::PacketHeader*>(&_recv_buffer[processed_bytes]);
			constexpr int MAX_PACKET_SIZE = 4096;
			if (header->_size < sizeof(packet::PacketHeader) || header->_size > MAX_PACKET_SIZE)
			{
				_recv_buffer.clear();
				break;
			}
			if (_recv_buffer.size() - processed_bytes < header->_size) break;

			auto task =
				[session = shared_from_this(),
				stream = packet::PacketStream(_recv_buffer.data() + processed_bytes, header->_size)]
			() mutable
			{
				packet::PacketManager::Instance()->Dispatch(session, stream);
			};

			server_ptr->get_logic_queue(_logic_thread_idx)->push({ std::move(task) });

			processed_bytes += header->_size;
		}

		if (processed_bytes > 0)
		{
			_recv_buffer.erase(_recv_buffer.begin(), _recv_buffer.begin() + processed_bytes);
		}
	}

	void SESSION::init(SOCKET s, int64_t id, int logic_idx) {
		_c_socket = s;
		_id = id;
		_logic_thread_idx = logic_idx;
		_room_id = -1;
		_state = SESSION_STATE::ST_LOBBY;
		_recv_buffer.clear();
		_viewedNpcs.clear();
		// _player 객체도 재사용하거나 새로 생성
		if (!_player) _player = std::make_shared<GAME::Player>(id);
		else _player->init(id); // Player 클래스에도 id를 초기화하는 init() 필요
	}

	void SESSION::disconnect()
	{
		SESSION_STATE expected = SESSION_STATE::ST_LOBBY;
		// 중복 Close 방지
		if (_state.compare_exchange_strong(expected, SESSION_STATE::ST_CLOSE) 
			|| (expected = SESSION_STATE::ST_INGAME, _state.compare_exchange_strong(expected, SESSION_STATE::ST_CLOSE)))
		{
			closesocket(_c_socket);
		}
	}

	void SESSION::clear()
	{ // ReleaseSession에서 호출할 클린업
		_state = SESSION_STATE::ST_FREE;
		_id = -1;
		_room_id = -1;
		_recv_buffer.clear();
		_viewedNpcs.clear();
		// 소켓은 이미 disconnect에서 닫혔을 것임
	}


	// ---------------------------------------- server class implementation ---------------------------------
	std::atomic<int64_t> Server::_new_id{ 0 }; // 전역 세션 ID 생성기
	std::atomic<int64_t> Server::_actor_id_gen{ 1 }; // 전역 액터 ID 생성기

	/// <summary>
	/// Server 생성자: IOCP를 생성하고 초기화합니다.
	/// </summary>
	Server::Server() : _accept_over{ IO_ACCEPT }, _is_running{ false }, _logic_thread_balancer{ 0 }, _iocp{ nullptr }
	{
		_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	}
	Server::~Server()
	{
		Stop();
		CloseHandle(_iocp);
	}
	void Server::Start(int io_thread_count, int logic_thread_count)
	{
		MYLOG("=========================================");
		MYLOG("          Server Initializing...         ");
		MYLOG("=========================================");

		std::wcout.imbue(std::locale("korean"));
		WSAData wsadata;
		if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
		{
			MYERROR("WSAStartup 실패\n");
		}

		PIP::packet::PacketManager::Instance()->Initialize();
		MYLOG("PacketManager Initialized." << std::endl);

		// 프로세스 우선순위를 높임
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

		// P-Core만 사용하도록 프로세스 친화도 설정
		PinThreadToPerformanceCores();

		_is_running = true;

		//MYLOG("[SERVER] Loading Map...");
		MapDataManager::Instance()->LoadMapData("../../Common/MapData/ExportedServerData.json");
		MapDataManager::Instance()->LoadHeightMapData("../../Common/MapData/Heightmap.json");
		//MapDataManager::Instance()->LoadMainLandscapeData("../../Client/Client/Resource/MainLandscape");
		MYLOG("[SERVER] Successful Loaded the Map");
		_logic_workers.resize(logic_thread_count);
		for (int i = 0; i < 100; ++i)
		{
			int logic_idx = i % logic_thread_count;
			_rooms.push_back(std::make_unique<Room>(i, logic_idx));
			_rooms.back()->Initialize();
		}
		MYLOG("[SERVER] Room count: " << _rooms.size());

		// 로직 워커 생성
		for (int i = 0; i < logic_thread_count; ++i)
		{
			// 이미 있는 워커 객체의 thread 멤버에 새 스레드를 대입
			_logic_workers[i].thread = std::thread([this, i]() {
				PinThreadToPerformanceCores();
				Logic_worker(i);
				});
		}
		MYLOG("[SERVER] Logic threads: " << _logic_workers.size() << ", IO threads: " << io_thread_count << ", Room count: " << _rooms.size());

		// I/O 스레드 생성
		for (int i = 0; i < io_thread_count; ++i)
		{
			_io_threads.emplace_back([this]() {
				// P-Core만 사용하도록 CPU 친화도 설정
				PinThreadToPerformanceCores();
				IO_worker();
				});
		}
		MYLOG("Created " << io_thread_count << " I/O threads and " << _logic_workers.size() << " logic threads.");
		

		// 리슨 소켓 설정 및 Accept 준비
		_listen_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(_listen_socket), _iocp, 0, 0);

		SOCKADDR_IN server_addr;
		ZeroMemory(&server_addr, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(common::packet::SERVER_PORT); // 포트 번호, 필요시 수정
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		
		int retval = bind(_listen_socket, reinterpret_cast<SOCKADDR*>(&server_addr), sizeof(server_addr));
		if (retval == SOCKET_ERROR)
		{
			print_error("bind", WSAGetLastError());
			exit(-1);
		}
		listen(_listen_socket, SOMAXCONN);
		MYLOG("Server listening on port " << common::packet::SERVER_PORT << "...");

		do_accept();

		MYLOG("Server started with " << io_thread_count << " I/O threads and " << _logic_workers.size() << " logic threads.");
		
	}
	void Server::Stop()
	{
		if (not _is_running.exchange(false))
		{
			return; // 이미 Stop이 호출된 경우 중복 실행 방지
		}

		// 1. 모든 로직 스레드의 큐에 종료 신호(더미 패킷)를 보냄
		for (auto& worker : _logic_workers)
		{
			worker.queue.push({[](){}});
		}

		for (size_t i = 0; i < _io_threads.size(); ++i)
		{
			// [추가] GetQueuedCompletionStatus에서 블록된 스레드를 깨우기 위해 더미 이벤트를 보냄
			PostQueuedCompletionStatus(_iocp, 0, 0, NULL);
		}

		// 2. 모든 I/O 스레드가 종료될 때까지 대기
		for (auto& th : _io_threads)
		{
			if (th.joinable())
			{
				th.join();
			}
		}

		// 3. 모든 로직 스레드가 종료될 때까지 대기
		for (auto& worker : _logic_workers)
		{
			if (worker.thread.joinable())
			{
				worker.thread.join();
			}
		}

		MYLOG("Server stopped.");
	}

	void Server::AddTimerJob(int worker_idx, std::chrono::milliseconds delay, std::function<void()> task)
	{
		if (worker_idx < 0 || worker_idx >= _logic_workers.size()) return;

		TimerJob newJob;
		newJob._execute_time = std::chrono::steady_clock::now() + delay;
		newJob._task = std::move(task);

		// 타이머 작업은 경쟁상태가 존재하면 안됨
		_logic_workers[worker_idx]._timer_queue.push(std::move(newJob));
	}

	concurrency::concurrent_queue<LogicJob>* Server::get_logic_queue(int worker_idx)
	{
		if (worker_idx < 0 || worker_idx >= _logic_workers.size())
		{
			return nullptr; // 안전장치
		}
		return &(_logic_workers[worker_idx].queue);
	}

	Room* Server::GetRoom(int room_id)
	{
		if (room_id < 0 || room_id >= _rooms.size())
		{
			return nullptr;
		}
		return _rooms[room_id].get();
	}

	void Server::AddSession(int64_t session_id, std::shared_ptr<SESSION> session)
	{
		_sessions.insert({ session_id, session });
	}
	std::shared_ptr<SESSION> Server::GetSession(int64_t session_id)
	{
		auto it = _sessions.find(session_id);
		if (it == _sessions.end())
		{
			return nullptr;
		}
		return it->second;
	}
	void Server::RemoveSession(int64_t session_id)
	{
		// TODO: [성능 최적화] 잦은 메모리 할당/해제를 피하기 위해
		//		 세션 객체를 삭제(erase)하는 대신, 상태를 초기화하고
		//		 별도의 free_list (객체 풀)에 넣어 재사용하는 방식을 고려 필요.
		//		 (Object Pooling 패턴)
		// [수정] 맵에서 완전히 제거
		_sessions.unsafe_erase(session_id);
	}

	std::shared_ptr<SESSION> Server::AcquireSession(SOCKET s, int64_t id, int logic_idx)
	{
		SESSION* raw_ptr = nullptr;
		if (!_session_pool.try_pop(raw_ptr)) {
			raw_ptr = new SESSION(id, s, logic_idx); // 풀이 비었으면 생성
		}

		raw_ptr->init(s, id, logic_idx);

		// [커스텀 델리터] 참조 카운트가 0이 되면 ReleaseSession 호출
		return std::shared_ptr<SESSION>(raw_ptr, [this](SESSION* p) {
				this->ReleaseSession(p);
			});
	}
	void Server::ReleaseSession(SESSION* session)
	{
		session->clear(); // 소켓 닫기 확인 및 버퍼 정리
		// 세션 데이터 완전 초기화 후 풀에 삽입
		_session_pool.push(session);
	}

	/// <summary>
	/// TODO: session 재사용이 제대로 되는지 검증 필요!!!!!!!!!!!!!!!!
	/// </summary>

	void Server::do_accept()
	{
		SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
		_accept_over._accept_socket = c_socket; // EXP_OVER 구조체에 클라이언트 소켓 저장

		AcceptEx(_listen_socket, c_socket, _accept_over._buffer.data(), 0,
				 sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, &_accept_over._over);
	}
	void Server::IO_worker()
	{
		MYLOG("[Thread] I/O worker thread started. ID: " << std::this_thread::get_id());
		while (_is_running)
		{
			DWORD io_size{};
			WSAOVERLAPPED* o{};
			ULONG_PTR key{}; // Accept의 경우 0, Recv/Send의 경우 Session ID
			
			BOOL ret = GetQueuedCompletionStatus(_iocp, &io_size, &key, &o, INFINITE);
			// [수정] o(EXP_OVER)가 삭제되기 전에 세션 참조를 안전하게 획득
			EXP_OVER* eo = reinterpret_cast<EXP_OVER*>(o);

			if (eo == nullptr)
			{
				continue;
			}

			// CompletionKey에서 세션 포인터 획득
			SESSION* session_ptr = reinterpret_cast<SESSION*>(key);

			// 클라이언트 연결 종료 또는 에러 처리
			if (ret == FALSE || (0 == io_size && (eo->_io_op == IO_RECV || eo->_io_op == IO_SEND)))
			{
				if (session_ptr)
				{
					// [수정] shared_from_this()를 통해 안전하게 참조 획득
					auto session = session_ptr->shared_from_this();
					

					SESSION_STATE expected = SESSION_STATE::ST_LOBBY;
					if (session->_state.compare_exchange_strong(expected, SESSION_STATE::ST_CLOSE) ||
						(expected = SESSION_STATE::ST_INGAME, session->_state.compare_exchange_strong(expected,
							SESSION_STATE::ST_CLOSE)))
					{
						MYLOG("[IO_WORKER] Client disconnected. Session ID: " << session->_id);
						if (session->_room_id != -1) {
							Room* room = GetRoom(session->_room_id);
							// [수정] 람다에 session(shared_ptr)을 캡처하여 로직 끝날 때까지 보존
							room->PushJob([this, session, room]() {
								room->LeavePlayer(session->_id);
								// 여기서 RemoveSession(session->_id)를 호출하여 맵에서 제거
								this->RemoveSession(session->_id);
								// 람다가 종료되면서 session(shared_ptr)의 참조가 줄어듦 -> 0이 되면 풀로 반납
								});
						}
						else {
							RemoveSession(session->_id);
						}
						closesocket(session->_c_socket);
					}
				}
				// [해결] eo가 서버 멤버 변수인 _accept_over인 경우(IO_ACCEPT) delete 하지 않음
				if (eo && eo->_io_op != IO_ACCEPT) {
					delete eo;
				}
				continue;
			}

			switch (eo->_io_op)
			{
			case IO_ACCEPT:
				// 새 클라이언트 접속 처리
				register_new_session(eo->_accept_socket);
				do_accept(); // 다음 클라이언트를 받기 위해 다시 Accept 요청
				break;
						
			case IO_SEND:
				// Send 완료 처리
				delete eo;
				break;
			
			case IO_RECV:
				{
					auto session = eo->_session_ref;
					// [수정] 완료된 eo를 함께 넘겨줌
					session->on_recv(eo, io_size, this);

					session->do_recv(); // 다음 수신 예약

					delete eo; // 처리가 끝났으니 삭제 (참조 카운트 감소)
					break;
				}
			default:
				MYERROR("[IO_WORKER] Unknown IO_OP received: " << static_cast<int>(eo->_io_op));
				break;
			}
		}
	}
	void Server::Logic_worker(int thread_idx)
	{
		auto& worker = _logic_workers[thread_idx];
		auto lastTick = std::chrono::steady_clock::now();
		auto lastReportTime = std::chrono::steady_clock::now();
		JPH::TempAllocatorImpl tempAllocator(20 * 1024 * 1024);

		double accumulator = 0.0;
		const double physicsStep = 1.0 / 60.0; // 16.66ms 고정
		const int MAX_PHYSICS_STEPS = 5;

		using namespace std::chrono;
		while (_is_running)
		{
			auto t_loop_start = steady_clock::now();

			// 1. 공용 잡 큐 처리 (LOGIN, ENTER_ROOM 등) - 즉시 처리!
			auto t_job_start = steady_clock::now();
			LogicJob logic_job;
			while (worker.queue.try_pop(logic_job))
			{
				if (logic_job._task) logic_job._task();
			}
			worker.stats.job_profile.add(duration_cast<nanoseconds>(steady_clock::now() - t_job_start).count());


			// --- 2. 타이머 잡 처리 ---
			auto t_timer_start = steady_clock::now();
			while (!worker._timer_queue.empty() &&
				worker._timer_queue.top()._execute_time <= std::chrono::steady_clock::now())
			{
				TimerJob timer_job = worker._timer_queue.top();
				worker._timer_queue.pop();
				if (timer_job._task) timer_job._task();
			}
			worker.stats.timer_profile.add(duration_cast<nanoseconds>(steady_clock::now() - t_timer_start).count());

			// 2. 시간 계산
			auto now = std::chrono::steady_clock::now();
			std::chrono::duration<double> elapsed = now - lastTick;
			lastTick = now;
			accumulator += elapsed.count();

			// 3. 물리 엔진 업데이트 (밀린 시간만큼 여러 번 돌려서라도 60fps 보장)
			auto t_phys_start = steady_clock::now();
			int steps = 0;
			while (accumulator >= physicsStep && steps < MAX_PHYSICS_STEPS)
			{
				for (auto& room : _rooms) {
					if (room->GetLogicThreadIndex() == thread_idx) {
						room->UpdatePhysics(static_cast<float>(physicsStep), &tempAllocator);
					}
				}
				accumulator -= physicsStep;
				steps++;
			}
			// 만약 너무 많이 밀렸다면 accumulator를 초기화하여 "Death Spiral" 방지
			if (accumulator > physicsStep * MAX_PHYSICS_STEPS) {
				accumulator = 0;
				// MYLOG("[Warning] Physics lagging! Dropping frames on Thread " << thread_idx);
			}
			worker.stats.physics_profile.add(duration_cast<nanoseconds>(steady_clock::now() - t_phys_start).count());

			// 4. 게임 로직 업데이트 (남은 시간만큼)
			auto t_logic_start = steady_clock::now();
			float dt = static_cast<float>(elapsed.count());
			uint32_t currentTick = static_cast<uint32_t>(GetTickCount64());
			for (auto& room : _rooms) {
				if (room->GetLogicThreadIndex() == thread_idx) {
					// [변경] 할당자 전달
					room->UpdateLogics(dt, &tempAllocator);
				}
			}
			worker.stats.logic_profile.add(duration_cast<nanoseconds>(steady_clock::now() - t_logic_start).count());

			auto t_loop_end = steady_clock::now();
			worker.stats.total_loop_profile.add(duration_cast<nanoseconds>(t_loop_end - t_loop_start).count());

			//if (duration_cast<seconds>(t_loop_end - lastReportTime).count() >= 5) {
			//	lastReportTime = t_loop_end;

			//	auto report = [&](const std::string& name, ProfileData& data) {
			//		int count = data.call_count.load();
			//		if (count == 0) return;
			//		double avg = (data.total_time_ns.load() / (double)count) / 1000000.0; // ns to ms
			//		double max_val = data.max_time_ns.load() / 1000000.0;
			//		MYLOG("[" << thread_idx << "][" << name << "] Avg: " << avg << "ms, Max: " << max_val << "ms, Count: " << count);
			//		data.reset();
			//	};

			//	MYLOG("---------- Thread " << thread_idx << " Performance Report (Last 5s) ----------\n");
			//	report("Job    ", worker.stats.job_profile);
			//	report("Timer  ", worker.stats.timer_profile);
			//	report("Physics", worker.stats.physics_profile);
			//	report("Logic  ", worker.stats.logic_profile);
			//	report("Total  ", worker.stats.total_loop_profile);
			//	MYLOG("-----------------------------------------------------------------------\n");
			//}

			// 60FPS 유지를 위한 Sleep
			auto loopElapsed = steady_clock::now() - t_loop_start;
			auto frameDuration = milliseconds(16);
			auto sleepTime = frameDuration - loopElapsed;

			if (sleepTime.count() > 0) {
				std::this_thread::sleep_for(sleepTime);
			}
		}
	}
	void Server::register_new_session(SOCKET client_socket)
	{
		int logic_idx = _logic_thread_balancer.fetch_add(1) % _logic_workers.size();
		int64_t new_id = _new_id++;

		auto session = AcquireSession(client_socket, new_id, logic_idx);

		// [핵심] CompletionKey에 ID 대신 포인터를 직접 전달 (Map 조회 제거)
		CreateIoCompletionPort((HANDLE)client_socket, _iocp, (ULONG_PTR)session.get(), 0);

		AddSession(new_id, session); // 맵에는 여전히 보관 (관리용)
		session->do_recv();
	}
}