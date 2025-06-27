#include "pch.h"
#include "server.h"

#include "PacketManager.h"

namespace chess::server
{
	EXP_OVER g_accept_over{ IO_ACCEPT };

	void do_accept(SOCKET s_socket, EXP_OVER& accept_over)
	{
		SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
		accept_over._accept_socket = c_socket;
		AcceptEx(s_socket, c_socket, accept_over._buffer.data(), 0,
				 sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
				 NULL, &accept_over._over);
	}

	void worker()
	{
		while (true)
		{
			DWORD io_size;
			WSAOVERLAPPED* o;
			ULONG_PTR key;
			BOOL ret = GetQueuedCompletionStatus(g_iocp, &io_size, &key, &o, INFINITE);
			EXP_OVER* eo = reinterpret_cast<EXP_OVER*>(o);

			if (FALSE == ret || (0 == io_size && (eo->_io_op == IO_RECV || eo->_io_op == IO_SEND)))
			{
				if (0 != g_users.count(key))
				{
					std::cout << "[WORKER] Client disconnected. Removing Session ID: " << key << std::endl;
					g_users.at(key) = nullptr; // 세션 제거
				}
				if (eo->_io_op == IO_SEND) delete eo;
				continue;
			}

			switch (eo->_io_op)
			{
				case IO_ACCEPT:
				{
					int new_id = g_new_id++;
					LOG("[WORKER] New client connected. Session ID: " << new_id);
					CreateIoCompletionPort(reinterpret_cast<HANDLE>(eo->_accept_socket), g_iocp, new_id, 0);
					std::shared_ptr<SESSION> p = std::make_shared<SESSION>(new_id, eo->_accept_socket);
					g_users.insert(std::make_pair(new_id, p));
					p->do_recv();
					do_accept(g_s_socket, g_accept_over);
					break;
				}
				case IO_SEND:
				{
					LOG("[WORKER] " << io_size << " bytes sent from Session ID: " << key);
					delete eo;
					break;
				}
				case IO_RECV:
				{
					LOG("[WORKER] Received " << io_size << " bytes from Session ID: " << key);
					auto user_iter = g_users.find(key);
					if (user_iter == g_users.end()) break;
					std::shared_ptr<SESSION> user = user_iter->second;
					if (nullptr == user) break;
					user->OnRecv(io_size);
					user->do_recv();
					break;
				}
			}
		}
	}

	SESSION::SESSION() : _state{ SESSION_STATE::ST_FREE }
	{
		ERROR("Default Constructor called, this should not happen!" << std::endl);
	}
	// server.cpp 수정안
	SESSION::SESSION(long long session_id, SOCKET s)
		: _c_socket{ s }, _id{ session_id }
	{
		_state = SESSION_STATE::ST_FREE;
	}
	SESSION::~SESSION()
	{
		LOG("[SESSION " << _id << "] Session destroyed. Name: " << _name);
		// 퇴장 패킷 생성
		packet::SC_PACKET_LEAVE leavePacket;
		leavePacket._id = _id;

		packet::PacketHeader header;
		header._type = static_cast<uint16_t>(packet::PacketType::S2C_P_LEAVE);
		header._size = sizeof(header) + sizeof(leavePacket);

		packet::PacketStream finalStream;
		finalStream << header;
		finalStream << leavePacket;

		// 다른 유저들에게 나간다고 알림
		for (auto& [id, u] : g_users) // 네임스페이스 명시
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

		// 항상 _recv_over의 내부 버퍼 처음부터, 전체 크기만큼 수신을 준비합니다.
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
				// exit(-1); // 직접 종료보다는 세션 정리 로직 호출이 더 좋습니다.
			}
		}
	}

	void SESSION::do_send(const char* data, size_t size)
	{
		EXP_OVER* o = new EXP_OVER(IO_SEND);
		memcpy(o->_buffer.data(), data, size);
		o->_wsabuf[0].len = static_cast<ULONG>(size);
		DWORD size_sent;
		LOG("[SESSION " << _id << "] Sending " << size << " bytes. Type: " << reinterpret_cast<const packet::PacketHeader*>(data)->_type);
		WSASend(_c_socket, o->_wsabuf.data(), 1, &size_sent, 0, &(o->_over), NULL);
	}
	void SESSION::OnRecv(size_t len)
	{
		_recv_buffer.insert(_recv_buffer.end(), _recv_over._buffer.data(), _recv_over._buffer.data() + len);
		size_t processed_bytes = 0;
		while (true)
		{
			if (_recv_buffer.size() - processed_bytes < sizeof(packet::PacketHeader))
				break;
			packet::PacketHeader* header = reinterpret_cast<packet::PacketHeader*>(&_recv_buffer[processed_bytes]);
			if (_recv_buffer.size() - processed_bytes < header->_size)
				break;
			LOG("[SESSION " << _id << "] Processing packet. Type: " << header->_type << ", Size: " << header->_size);
			packet::PacketStream stream(reinterpret_cast<char*>(header), header->_size);
			packet::PacketManager::Instance()->Dispatch(header->_type, shared_from_this(), stream);
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
		header._type = static_cast<uint16_t>(packet::PacketType::S2C_P_AVATAR_INFO);
		header._size = sizeof(header) + sizeof(avatarInfoPacket);

		// 3. PacketStream으로 최종 패킷 조립
		packet::PacketStream finalStream;
		finalStream << header;
		finalStream << avatarInfoPacket; // SC_PACKET_AVATAR_INFO는 고정 크기이므로 바로 스트림에 넣음

		// 4. 완성된 패킷 전송
		do_send(finalStream.Data(), finalStream.Size());
	}
	
}

