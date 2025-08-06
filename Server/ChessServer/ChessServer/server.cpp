#include "pch.h"
#include "server.h"

#include "PacketManager.h"

namespace chess::server
{
	

	SESSION::SESSION() : _state{ SESSION_STATE::ST_FREE }
	{
		ERROR("Default Constructor called, this should not happen!" << std::endl);
	}
	// server.cpp
	SESSION::SESSION(long long session_id, SOCKET s, int logic_index)
		: _c_socket{ s }, _id{ session_id }, _logic_thread_idx{ logic_index }, _hp{ 100 }, _max_hp{ 100 }
	{
		_state = SESSION_STATE::ST_LOBBY;
	}
	SESSION::~SESSION()
	{
		LOG("[SESSION " << _id << "] Session destroyed. Name: " << _name);
		
		// server.cpp의 SESSION::~SESSION() 내부
		packet::SC_PACKET_LEAVE leavePacket;
		leavePacket._type = packet::PacketType::S2C_P_LEAVE;
		leavePacket._size = sizeof(leavePacket); // SC_PACKET_LEAVE는 고정 크기
		leavePacket._id = _id;

		packet::PacketStream finalStream;
		finalStream << leavePacket; // leavePacket에 헤더 정보가 이미 포함되어 있음

		for (auto& [id, u] : g_users) 
		{
			if (_id != id)
			{
				std::shared_ptr<SESSION> user = u;
				if (user && user->_state == SESSION_STATE::ST_INGAME)
				{
					user->do_send(finalStream.Data(), finalStream.Size());
				}
			}
		}
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

		LOG("[SESSION " << _id << "] Sending " << size << " bytes. Type: " << static_cast<int>(reinterpret_cast<const packet::PacketHeader*>(data)->_type));
		//(enum을 바로 출력하기 위해 int로 캐스팅)

		WSASend(_c_socket, o->_wsabuf.data(), 1, &size_sent, 0, &(o->_over), NULL);
	}
	void SESSION::OnRecv(size_t len, Server* server_ptr)
	{
		_recv_buffer.insert(_recv_buffer.end(), _recv_over._buffer.data(), _recv_over._buffer.data() + len);
		size_t processed_bytes = 0;
		while (true)
		{
			if (_recv_buffer.size() - processed_bytes < sizeof(packet::PacketHeader))
				break;
			packet::PacketHeader * header = reinterpret_cast<packet::PacketHeader*>(&_recv_buffer[processed_bytes]);
			if (_recv_buffer.size() - processed_bytes < header->_size)
				break;
		
			// LogicPacket
			LogicPacket logic_packet;
			logic_packet.session = shared_from_this();
			logic_packet.packet_data.assign(&_recv_buffer[processed_bytes],&_recv_buffer[processed_bytes + header->_size]);
		
			server_ptr->get_logic_queue(_logic_thread_idx)->Push(logic_packet);
		
			processed_bytes += header->_size;
		}
		
		if (processed_bytes > 0)
		{
			_recv_buffer.erase(_recv_buffer.begin(), _recv_buffer.begin() + processed_bytes);
		}
	}

	void SESSION::send_player_info_packet()
	{
		packet::SC_PACKET_AVATAR_INFO avatarInfoPacket;
		avatarInfoPacket._type = packet::PacketType::S2C_P_AVATAR_INFO;
		avatarInfoPacket._size = sizeof(avatarInfoPacket); // 고정 크기
		avatarInfoPacket._id = _id;
		avatarInfoPacket._x = _x;
		avatarInfoPacket._y = _y;
		avatarInfoPacket._level = 1;
		avatarInfoPacket._hp = 100;
		avatarInfoPacket._exp = 200;
		
		packet::PacketStream finalStream;
		finalStream << avatarInfoPacket; // avatarInfoPacket에 헤더 정보가 이미 포함되어 있음

		// 4. 완성된 패킷 전송
		do_send(finalStream.Data(), finalStream.Size());
	}

	// --------- server class implementation ---------

	/// <summary>
	/// Server 생성자: IOCP를 생성하고 초기화합니다.
	/// </summary>
	Server::Server() : _accept_over{ IO_ACCEPT }, _is_running{ false }, _logic_thread_balancer{ 0 }
	{
		g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	}
	Server::~Server()
	{
		Stop();
		CloseHandle(g_iocp);
	}
	void Server::Start(int io_threads, int logic_threads)
	{
		_is_running = true;
		
		// 로직 워커 생성
		_logic_workers.reserve(logic_threads); // 미리 공간을 할당하여 불필요한 재할당 방지
		for (int i = 0; i < logic_threads; ++i)
		{
			// LogicWorker 생성자에 std::thread 객체를 이동시켜 전달합니다.
			_logic_workers.emplace_back(std::thread(&Server::Logic_worker, this, i));
		}
		
		for (int i = 0; i < 100; ++i)
		{
			int logic_idx = i % _logic_workers.size();
			_rooms.push_back(std::make_unique<Room>(i, logic_idx));
		}
		LOG("[SERVER] Logic threads: " << _logic_workers.size() << ", IO threads: " << io_threads << ", Room count: " << _rooms.size());


		// I/O 스레드 생성
		for (int i = 0; i < io_threads; ++i)
		{
			_io_threads.emplace_back(&Server::IO_worker, this);
		}
		
		LOG("Server started with " << io_threads << " I/O threads and " << _logic_workers.size() << " logic threads.");

		// 리슨 소켓 설정 및 Accept 준비
		_listen_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(_listen_socket), g_iocp, 0, 0);

		SOCKADDR_IN server_addr;
		ZeroMemory(&server_addr, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(3000); // 포트 번호, 필요시 수정
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		
		bind(_listen_socket, reinterpret_cast<SOCKADDR*>(&server_addr), sizeof(server_addr));
		listen(_listen_socket, SOMAXCONN);
		
		do_accept();
	}
	void Server::Stop()
	{
		if (!_is_running.exchange(false))
		{
			return; // 이미 Stop이 호출된 경우 중복 실행 방지
		}

		// 1. 모든 로직 스레드의 큐에 종료 신호(더미 패킷)를 보냄
		for (auto& worker : _logic_workers)
		{
			LogicPacket dummy_packet;
			dummy_packet.session = nullptr; // 종료 신호로 nullptr 사용
			worker.queue.Push(dummy_packet);
		}

		for (size_t i = 0; i < _io_threads.size(); ++i)
		{
			// [추가] GetQueuedCompletionStatus에서 블록된 스레드를 깨우기 위해 더미 이벤트를 보냄
			PostQueuedCompletionStatus(g_iocp, 0, 0, NULL);
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

		LOG("Server stopped.");
	}
	auto Server::get_logic_queue(int worker_idx) -> ConcurrentQueue<LogicPacket>*
	{
		if (worker_idx < 0 || worker_idx >= _logic_workers.size())
		{
			return nullptr; // 안전장치
		}
		return &(_logic_workers[worker_idx].queue);
	}

	auto Server::GetRoom(int room_id) -> Room*
	{
		if (room_id < 0 || room_id >= _rooms.size())
		{
			return nullptr;
		}
		return _rooms[room_id].get();
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
		while (_is_running)
		{
			DWORD io_size;
			WSAOVERLAPPED* o;
			ULONG_PTR key; // Accept의 경우 0, Recv/Send의 경우 Session ID
			
			BOOL ret = GetQueuedCompletionStatus(g_iocp, &io_size, &key, &o, INFINITE);
			EXP_OVER* eo = reinterpret_cast<EXP_OVER*>(o);
			
			// 클라이언트 연결 종료 또는 에러 처리
			if (FALSE == ret || (0 == io_size && (eo->_io_op == IO_RECV || eo->_io_op == IO_SEND)))
			{
				auto user_iter = g_users.find(key);
				if (user_iter != g_users.end())
				{
					std::shared_ptr<SESSION> session = user_iter->second;
					LOG("[IO_WORKER] Client disconnected. Session ID: " << session->_id);
					
					// LogicPacket에 세션 정보만 담고 데이터는 비워서 "연결 끊김" 이벤트로 사용
					LogicPacket disconnect_packet;
					disconnect_packet.session = session;
					
					// 해당 세션을 담당하는 로직 스레드의 큐에 이벤트를 넣는다.
					get_logic_queue(session->_logic_thread_idx)->Push(disconnect_packet);
					
					// 이제 g_users에서 제거해도 로직 스레드는 shared_ptr로 안전하게 객체를 참조함
					g_users.unsafe_erase(user_iter);
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
					// Recv 완료 처리: 받은 데이터를 해당 로직 스레드의 큐로 전달
					auto user_iter = g_users.find(key);
					if (user_iter != g_users.end() && user_iter->second != nullptr)
					{
						user_iter->second->OnRecv(io_size, this);
						user_iter->second->do_recv(); // 다음 Recv 요청
					}
					break;
				}
			}
		}
	}
	void Server::Logic_worker(int thread_idx)
	{
		LogicPacket packet_to_process;
		while (_is_running)
		{
			_logic_workers[thread_idx].queue.WaitPop(packet_to_process);

			auto& session = packet_to_process.session;
			if (session == nullptr) continue;

			// 패킷 데이터 없으면 연결 끊김 처리
			if (packet_to_process.packet_data.empty())
			{
				if (session->_state == SESSION_STATE::ST_INGAME && session->_room_id != -1)
				{
					Room * room = GetRoom(session->_room_id);
					if (room)
					{
						room->RemovePlayer(session->_id);
						LOG("[Logic_worker] Processed disconnect for session " << session->_id << " from room " << room->GetRoomId());
					}
				}
				continue; // 연결 끊김 처리가 끝났으므로 루프의 처음으로 돌아감
			}

			packet::PacketHeader* header = reinterpret_cast<packet::PacketHeader*>(packet_to_process.packet_data.data());
			packet::PacketStream stream(packet_to_process.packet_data.data(), header->_size);

			switch (session->_state)
			{
				case SESSION_STATE::ST_LOBBY:
				{
					// 로비 상태에서 처리할 수 있는 패킷들
					switch (header->_type)
					{
						case packet::PacketType::C2S_P_ROOM_LIST:
						case packet::PacketType::C2S_P_ENTER_ROOM:
							packet::PacketManager::Instance()->Dispatch(header->_type, session, stream);
							break;
						default:
							// 로비에서 처리할 수 없는 패킷은 무시하거나 에러 처리
							LOG("[Logic_worker] Invalid packet " << static_cast<int>(header->_type) << " from session " << session->_id << " in LOBBY state.");
								break;
					}
					break;
				}
				case SESSION_STATE::ST_INGAME:
				{
					// 인게임 상태에서 처리할 수 있는 패킷들
					// 담당 로직 스레드가 일치하는지 한번 더 확인 (안전장치)
					Room* room = GetRoom(session->_room_id);
					if (room == nullptr || _logic_workers[thread_idx].thread.get_id() != _logic_workers[room->GetLogicThreadIndex()].thread.get_id())
					{
						// 세션 정보와 실제 방의 담당 스레드가 일치하지 않는 심각한 오류
						// 혹은 방이 없는 경우
						LOG("[Logic_worker] **ERROR** Session " << session->_id << " has invalid room info.");
						// 접속 종료 등의 예외 처리 필요
						continue;
					}

					switch (header->_type)
					{
						case packet::PacketType::C2S_P_MOVE:
						case packet::PacketType::C2S_P_ATTACK:
						case packet::PacketType::C2S_P_CHAT_IN_ROOM:
							// TODO: 채팅, 스킬 사용 등 인게임 관련 패킷들 추가
							packet::PacketManager::Instance()->Dispatch(header->_type, session, stream);
							break;

						case packet::PacketType::C2S_P_ENTER_ROOM: // 방 안에서 다른 방으로 이동하는 경우
						case packet::PacketType::C2S_P_ROOM_LIST:  // 방 안에서 방 목록을 다시 요청하는 경우
							packet::PacketManager::Instance()->Dispatch(header->_type, session, stream);
							break;

						default:
							// 인게임에서 처리할 수 없는 패킷은 무시
							LOG("[Logic_worker] Invalid packet " << static_cast<int>(header->_type) << " from session " << session->_id << " in INGAME state.");
						break;
					}
					break;
				}
			}
		}
	}
	void Server::register_new_session(SOCKET client_socket)
	{
		//세션에 할당할 로직 스레드를 라운드-로빈 방식으로 선택
		int logic_idx = _logic_thread_balancer.fetch_add(1) % _logic_workers.size();
		
		// 2. 세션 ID 발급
		long long new_id = g_new_id++;
		
		// 3. 새 세션 객체 생성
		std::shared_ptr<SESSION> p = std::make_shared<SESSION>(new_id, client_socket, logic_idx);
		
		// 4. 새 클라이언트 소켓을 IOCP에 연결하고, 세션 ID(new_id)를 Completion Key로 사용
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), g_iocp, new_id, 0);
		
		// 5. 전체 유저 목록에 새 세션 추가
		g_users.insert({ new_id, p });
		
		// 6. 첫 Recv 요청
		p->do_recv();
		
		LOG("[SERVER] New client connected. Session ID: " << new_id << ", assigned to Logic Thread:" << logic_idx);
	}
}