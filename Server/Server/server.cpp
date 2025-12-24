#include "pch.h"
#include "server.h"

#include "LuaManager.h"
#include "Player.h"
#include "MapDataManager.h"
#include "PacketManager.h"

namespace PIP::server
{
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
		case PacketType::C2S_P_ATTACK:
			return "C2S_P_ATTACK";
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
	SESSION::SESSION(long long session_id, SOCKET s, int logic_index)
		: _c_socket{ s }, _id{ session_id }, _logic_thread_idx{ logic_index }
	{
		_state = SESSION_STATE::ST_LOBBY;
		_player = Player(session_id);
	}
	SESSION::~SESSION()
	{
		MYLOG("[SESSION " << _id << "] Session destroyed. Name: " << _player._name);
		// TODO: Logic_Worker 에서 세션 종료 패킷을 보내는 로직 추가 필요
		closesocket(_c_socket);
	}
	void SESSION::do_recv()
	{
		DWORD recv_flag = 0;
		ZeroMemory(&_recv_over._over, sizeof(_recv_over._over));

		_recv_over._wsabuf[0].buf = reinterpret_cast<CHAR*>(_recv_over._buffer.data());
		_recv_over._wsabuf[0].len = static_cast<ULONG>(_recv_over._buffer.size());

		auto ret = WSARecv(_c_socket, _recv_over._wsabuf.data(), 1, NULL,
						   &recv_flag, &_recv_over._over, NULL);

		if (0 != ret)
		{
			auto err_no = WSAGetLastError();
			if (WSA_IO_PENDING != err_no)
			{
				print_error("WSARecv", err_no);
				// exit(-1);
			}
		}
	}

	void SESSION::do_send(const char* data, size_t size)
	{
		EXP_OVER* o = new EXP_OVER(IO_SEND);
		memcpy(o->_buffer.data(), data, size);
		o->_wsabuf[0].len = static_cast<ULONG>(size);
		DWORD size_sent;

		/*MYLOG("[SESSION " << _id << "] Sending " << size << " bytes. Type: " << 
			PacketTypeToString(reinterpret_cast<const packet::PacketHeader*>(data)->_type));*/

		WSASend(_c_socket, o->_wsabuf.data(), 1, &size_sent, 0, &(o->_over), NULL);
	}
	void SESSION::OnRecv(size_t len, Server* server_ptr)
	{
		_recv_buffer.insert(_recv_buffer.end(), _recv_over._buffer.data(), _recv_over._buffer.data() + len);
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


	// ---------------------------------------- server class implementation ---------------------------------
	std::atomic<int> Server::_new_id{ 0 }; // 전역 세션 ID 생성기

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
		SetProcessAffinityMask(GetCurrentProcess(), GetPCoresMask());

		_is_running = true;

		//MYLOG("[SERVER] Loading Map...");
		MapDataManager::Instance()->LoadMapData("../../Common/MapData/ExportedServerData.json");
		MapDataManager::Instance()->LoadHeightMapData("../../Common/MapData/Heightmap.json");
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
				SetThreadAffinityMask(GetCurrentThread(), GetPCoresMask());
				Logic_worker(i);
				});
		}
		MYLOG("[SERVER] Logic threads: " << _logic_workers.size() << ", IO threads: " << io_thread_count << ", Room count: " << _rooms.size());

		// I/O 스레드 생성
		for (int i = 0; i < io_thread_count; ++i)
		{
			_io_threads.emplace_back([this]() {
				// P-Core만 사용하도록 CPU 친화도 설정
				SetThreadAffinityMask(GetCurrentThread(), GetPCoresMask());
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

	void Server::AddSession(long long session_id, std::shared_ptr<SESSION> session)
	{
		_sessions.insert({ session_id, session });
	}
	std::shared_ptr<SESSION> Server::GetSession(long long session_id)
	{
		auto it = _sessions.find(session_id);
		if (it == _sessions.end())
		{
			return nullptr;
		}
		return it->second;
	}
	void Server::RemoveSession(long long session_id)
	{
		// TODO: [성능 최적화] 잦은 메모리 할당/해제를 피하기 위해
		//		 세션 객체를 삭제(erase)하는 대신, 상태를 초기화하고
		//		 별도의 free_list (객체 풀)에 넣어 재사용하는 방식을 고려 필요.
		//		 (Object Pooling 패턴)
		_sessions[session_id] = nullptr;
	}

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
			EXP_OVER* eo = reinterpret_cast<EXP_OVER*>(o);

			if (o == nullptr)
			{
				// 서버 종료 신호 등 특별 상황 처리
				continue;
			}

			// 클라이언트 연결 종료 또는 에러 처리
			if (FALSE == ret || (0 == io_size && (eo->_io_op == IO_RECV || eo->_io_op == IO_SEND)))
			{
				std::shared_ptr<SESSION> session = GetSession(key);
				if (session)
				{
					MYLOG("[IO_WORKER] Client disconnected. Session ID: " << session->_id);

					auto task = [this, session_id = session->_id]() {
						std::shared_ptr<SESSION> s = GetSession(session_id);
						if (s && s->_state == SESSION_STATE::ST_INGAME && s->_room_id != -1)
						{
							Room* room = GetRoom(s->_room_id);
							if (room)
							{
								// 방에서 플레이어 제거
								room->LeavePlayer(s->_id);
								MYLOG("[Logic_worker] Processed disconnect for session " << s->_id << " from room " << room->GetRoomId());
							}
						}
						RemoveSession(session_id);
					}; 
					get_logic_queue(session->_logic_thread_idx)->push({std::move(task)});
				}
				if (eo->_io_op == IO_SEND)
					delete eo;
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
					std::shared_ptr<SESSION> session = GetSession(key);
					if (session)
					{
						session->OnRecv(io_size, this); //수신
						session->do_recv(); // 수신 예약
					}
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
		MYLOG("[Thread] Logic worker thread #" << thread_idx << " started. ID: " << std::this_thread::get_id());
		auto& worker = _logic_workers[thread_idx];
		LogicJob job_to_process;
		while (_is_running)
		{
			while (not worker._timer_queue.empty() && 
				worker._timer_queue.top()._execute_time <= std::chrono::steady_clock::now())
			{

				TimerJob timer_job = worker._timer_queue.top();
				worker._timer_queue.pop(); 
				worker.queue.push({ std::move(timer_job._task) });
			}

			// 2. 일반 큐에서 작업 처리
			if (worker.queue.try_pop(job_to_process))
			{
				if (job_to_process._task)
				{
					job_to_process._task();
				}
			}
			else
			{
				// 대기 상태를 개선하는 더 나은 방법
				if (worker.queue.empty())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
			}
		}
	}
	void Server::register_new_session(SOCKET client_socket)
	{
		int logic_idx = _logic_thread_balancer.fetch_add(1) % _logic_workers.size();
		long long new_id = _new_id++;
		std::shared_ptr<SESSION> p = std::make_shared<SESSION>(new_id, client_socket, logic_idx);
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), _iocp, new_id, 0);
		// TCP_NODELAY 설정 추가
		int nodelay = 1;
		if (SOCKET_ERROR == setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY,
			(const char*)&nodelay, sizeof(nodelay)))
		{
			// 로그만 남기고 계속 진행
			MYERROR("Failed to set TCP_NODELAY for session " << new_id);
		}

		AddSession(new_id, p);
		p->do_recv();
		MYLOG("[SERVER] New client connected. Session ID: " << new_id << ", assigned to Logic Thread:" << logic_idx);
	}

	DWORD_PTR Server::GetPCoresMask()
	{
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);

		// Intel 12세대 이상: 보통 첫 8개 코어가 P-Core
		// 시스템에 따라 조정 필요
		DWORD_PTR pCoreMask = 0;
		for (int i = 0; i < std::min(8, (int)sysInfo.dwNumberOfProcessors); ++i)
		{
			pCoreMask |= (1ULL << i);
		}

		MYLOG("P-Core mask set to: 0x" << std::hex << pCoreMask);
		return pCoreMask;
	}
}
