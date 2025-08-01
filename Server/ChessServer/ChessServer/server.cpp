#include "pch.h"
#include "server.h"

#include "PacketManager.h"

namespace chess::server
{
	

	SESSION::SESSION() : _state{ SESSION_STATE::ST_FREE }
	{
		ERROR("Default Constructor called, this should not happen!" << std::endl);
	}
	// server.cpp ������
	SESSION::SESSION(long long session_id, SOCKET s, int logic_index)
		: _c_socket{ s }, _id{ session_id }, _logic_thread_idx{ logic_index }, _hp{ 100 }, _max_hp{ 100 }
	{
		_state = SESSION_STATE::ST_LOBBY;
	}
	SESSION::~SESSION()
	{
		LOG("[SESSION " << _id << "] Session destroyed. Name: " << _name);
		// ���� ��Ŷ ����
		packet::SC_PACKET_LEAVE leavePacket;
		leavePacket._id = _id;

		packet::PacketHeader header;
		header._type = packet::PacketType::S2C_P_LEAVE;
		header._size = sizeof(header) + sizeof(leavePacket);

		packet::PacketStream finalStream;
		finalStream << header;
		finalStream << leavePacket;

		// �ٸ� �����鿡�� �����ٰ� �˸�
		for (auto& [id, u] : g_users) // ���ӽ����̽� ����
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

		// �׻� _recv_over�� ���� ���� ó������, ��ü ũ�⸸ŭ ������ �غ��մϴ�.
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
				// exit(-1); // ���� ���Ẹ�ٴ� ���� ���� ���� ȣ���� �� �����ϴ�.
			}
		}
	}

	void SESSION::do_send(const char* data, size_t size)
	{
		EXP_OVER* o = new EXP_OVER(IO_SEND);
		memcpy(o->_buffer.data(), data, size);
		o->_wsabuf[0].len = static_cast<ULONG>(size);
		DWORD size_sent;
		LOG("[SESSION " << _id << "] Sending " << size << " bytes. Type: " << static_cast<uint16_t>(reinterpret_cast<const packet::PacketHeader*>(data)->_type));
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
		
			// LogicPacket�� ����� ��� ���� ť�� �ִ´�.
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
		// 1. 패킷 데이터 구조체에 값 채우기
		packet::SC_PACKET_AVATAR_INFO avatarInfoPacket;
		avatarInfoPacket._id = _id;
		avatarInfoPacket._x = _x;
		avatarInfoPacket._y = _y;
		avatarInfoPacket._level = 1;
		avatarInfoPacket._hp = 100;
		avatarInfoPacket._exp = 200;

		// 2. 헤더 생성
		packet::PacketHeader header;
		header._type = packet::PacketType::S2C_P_AVATAR_INFO;
		header._size = sizeof(header) + sizeof(avatarInfoPacket);

		// 3. PacketStream으로 최종 패킷 조립
		packet::PacketStream finalStream;
		finalStream << header;
		finalStream << avatarInfoPacket; // SC_PACKET_AVATAR_INFO는 고정 크기이므로 바로 스트림에 넣음

		// 4. 완성된 패킷 전송
		do_send(finalStream.Data(), finalStream.Size());
	}

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
		
		// 로직 큐와 로직 스레드 생성
		for (int i = 0; i < logic_threads; ++i)
		{
		    _logic_queues.push_back(std::make_unique<ConcurrentQueue<LogicPacket>>());
		    _logic_threads.emplace_back(&Server::Logic_worker, this, i);
		}
		
		for (int i = 0; i < 100; ++i)
		{
			int logic_idx = i % logic_threads;
			_rooms.push_back(std::make_unique<Room>(i, logic_idx));
		}
        LOG("[SERVER] Logic threads: " << logic_threads << ", IO threads: " << io_threads << ", Room count: " << _rooms.size());


		// I/O 스레드 생성
		for (int i = 0; i < io_threads; ++i)
		{
		    _io_threads.emplace_back(&Server::IO_worker, this);
		}
		
		LOG("Server started with " << io_threads << " I/O threads and " << logic_threads << " logic threads.");

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
		_is_running = false;

		// TODO: 스레드를 안전하게 종료하는 로직 추가 필요

		for (auto& th : _io_threads)
		    if (th.joinable()) th.join();
		
		for (auto& th : _logic_threads)
		    if (th.joinable()) th.join();
	}
	auto Server::get_logic_queue(int queue_idx) -> ConcurrentQueue<LogicPacket>*
	{
		return _logic_queues[queue_idx].get();
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
				if (g_users.count(key))
				{
					LOG("[IO_WORKER] Client disconnected. Removing Session ID: " << key);
					g_users.unsafe_erase(key); // 유저 목록에서 제거
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
			// 자신의 큐에서 패킷이 올 때까지 대기
			_logic_queues[thread_idx]->WaitPop(packet_to_process);
			
			if (packet_to_process.session && packet_to_process.session->_state == SESSION_STATE::ST_INGAME)
			{
				packet::PacketHeader* header = reinterpret_cast<packet::PacketHeader*>(packet_to_process.packet_data.data());
				packet::PacketStream stream(packet_to_process.packet_data.data(), header->_size);

				// 패킷 매니저를 통해 실제 로직 처리
				packet::PacketManager::Instance()->Dispatch(static_cast<uint16_t>(header->_type), packet_to_process.session, stream);
			}
		}
	}
	void Server::register_new_session(SOCKET client_socket)
	{
		//세션에 할당할 로직 스레드를 라운드-로빈 방식으로 선택
		int logic_idx = _logic_thread_balancer.fetch_add(1) % _logic_threads.size();
		
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

