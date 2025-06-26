#include "server.h"
#include "main.h"
#include <ranges>

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

			if (FALSE == ret)
			{
				auto err_no = WSAGetLastError();
				print_error("GetQueuedCompletionStatus", err_no);
				if (0 != g_users.count(key))
				{
					g_users.at(key) = nullptr; // 세션이 종료되었으므로 nullptr로 설정
				}
				continue;
			}

			if ((IO_RECV == eo->_io_op || IO_SEND == eo->_io_op) && 0 == io_size)
			{
				if (0 != g_users.count(key))
				{
					g_users.at(key) = nullptr; // 세션이 종료되었으므로 nullptr로 설정
				}
				continue;
			}

			switch (eo->_io_op)
			{
				case IO_ACCEPT:
					{
						int new_id = g_new_id++;
						CreateIoCompletionPort(reinterpret_cast<HANDLE>(eo->_accept_socket), g_iocp, new_id, 0);

						std::shared_ptr<SESSION> p = std::make_shared<SESSION>(new_id, eo->_accept_socket);

						g_users.insert(std::make_pair(new_id, p));
						p->do_recv(); // 새로운 세션에 대해 recv 시작
						do_accept(g_s_socket, g_accept_over);
					}
					break;
				case IO_SEND:
					delete eo;	// 전송 완료 후 메모리 해제
					break;
				case IO_RECV:
					{
						std::shared_ptr<SESSION> user = g_users[key];
						if (nullptr == user)
						{
							break;
						}

						unsigned char* p = eo->_buffer.data();
						int data_size = io_size + user->_remained();

						while (p < eo->_buffer.data() + data_size)
						{
							unsigned char packet_size = *p;
							if (p + packet_size > eo->_buffer.data() + data_size)
								break;

							user->process_packet(p);

							p = p + packet_size;
						}

						if (p < eo->_buffer.data() + data_size)
						{
							user->_remained = static_cast<unsigned char>(eo->_buffer.data() + data_size - p);
							memcpy(p, eo->_buffer.data(), user->_remained);
						}
						else
							user->_remained = 0;

						user->do_recv();
					}
						break;
			}
		}
	}

	SESSION::SESSION() : _state{ SESSION_STATE::ST_FREE }
	{
		std::cerr << "Default Constructor called, this should not happen!" << std::endl;
		exit(1);
	}
	SESSION::SESSION(long long session_id, SOCKET s)
			: _c_socket{ s }, _id{ session_id }, _remained{ 0 }
	{}
	SESSION::~SESSION()
	{
		packet::sc_packet_leave lp;
		lp._size = sizeof(lp);
		lp._type = packet::PacketType::S2C_P_LEAVE;
		lp._id = _id;

		for (auto& [id, u] : g_users)
		{
			if (_id != id)
			{
				std::shared_ptr<SESSION> user = u;
				if (user && user->_state == SESSION_STATE::ST_INGAME)
				{
					user->do_send(&lp);
				}
			}
		}	// 다른 유저들에게 나간다고 알림
		closesocket(_c_socket); // 소켓 닫기
	}
	void SESSION::do_recv()
	{
		DWORD recv_flag = 0;
		ZeroMemory(&_recv_over._over, sizeof(_recv_over._over));
		_recv_over._wsabuf[0].buf = reinterpret_cast<CHAR*>(_recv_over._buffer.data() + _remained);
		_recv_over._wsabuf[0].len = static_cast<ULONG>(_recv_over._buffer.size() - _remained);

		auto ret = WSARecv(_c_socket, _recv_over._wsabuf.data(), 1, NULL,
						   &recv_flag, &_recv_over._over, NULL);

		if (0 != ret)
		{
			auto err_no = WSAGetLastError();
			if (WSA_IO_PENDING != err_no)
			{
				print_error("WSARecv", err_no);
				exit(-1);
			}
		}
	}
	void SESSION::do_send(void* buff)
	{
		EXP_OVER* o = new EXP_OVER(IO_SEND); // 전송 작업을 위한 OVERLAPPED 구조체 생성
		const unsigned char packet_size = static_cast<unsigned char*>(buff)[0]; // 패킷 크기 추출
		memcpy(o->_buffer.data(), buff, packet_size); // 버퍼에 패킷 데이터 복사
		o->_wsabuf[0].len = packet_size; // WSABUF의 길이 설정
		DWORD size_sent; // 전송된 바이트 수를 저장할 변수
		WSASend(_c_socket, o->_wsabuf.data(), 1, &size_sent, 0, &(o->_over), NULL); // 비동기 전송 요청
	}
	void SESSION::send_player_info_packet()
	{
		packet::sc_packet_avatar_info p;
		p._size = sizeof(p);
		p._type = packet::PacketType::S2C_P_AVATAR_INFO;
		p._id = _id;
		p._x = _x;
		p._y = _y;
		p._level = 1;
		p._hp = 100;
		p._exp = 200;
		do_send(&p);
	}
	void SESSION::send_player_pos()
	{
		packet::sc_packet_move p;
		p._size = sizeof(p);
		p._type = packet::PacketType::S2C_P_MOVE;
		p._id = _id;
		p._x = _x;
		p._y = _y;
		do_send(&p);
	}
	void SESSION::process_packet(unsigned char* p)
	{
		const unsigned char packet_type = p[1];
		switch (packet_type)
		{

			case packet::PacketType::C2S_P_LOGIN:
			{
				packet::cs_packet_login* packet = reinterpret_cast<packet::cs_packet_login*>(p);
				_name = packet->_name;
				_x = 4;
				_y = 4;
				_state = SESSION_STATE::ST_INGAME;

				send_player_info_packet();

				packet::sc_packet_enter ep;
				ep._size = sizeof(ep);
				ep._type = packet::PacketType::S2C_P_ENTER;
				ep._id = _id;
				strcpy_s(ep._name, _name.c_str());
				ep._o_type = 0;
				ep._x = _x;
				ep._y = _y;

				for (auto& u : g_users)
				{
					if (u.first != _id)
					{
						std::shared_ptr<SESSION> p = u.second;

						if ((nullptr != p) && (p->_state == SESSION_STATE::ST_INGAME))
							p->do_send(&ep);
					}
				}

				for (auto& u : g_users)
				{
					if (u.first != _id)
					{
						std::shared_ptr<SESSION> p = u.second;

						if ((nullptr == p) || (p->_state != SESSION_STATE::ST_INGAME))
							continue;

						packet::sc_packet_enter ep;
						ep._size = sizeof(ep);
						ep._type = packet::PacketType::S2C_P_ENTER;
						ep._id = u.first;
						strcpy_s(ep._name, p->_name.c_str());
						ep._o_type = 0;
						ep._x = p->_x;
						ep._y = p->_y;
						do_send(&ep);

					}
				}
				break;
			}
			case packet::PacketType::C2S_P_MOVE:
			{
				packet::cs_packet_move* packet = reinterpret_cast<packet::cs_packet_move*>(p);
				switch (packet->_direction)
				{
					case packet::MOVE_TYPE::MOVE_UP: if (_y > 0) _y = _y - 1; break;
					case packet::MOVE_TYPE::MOVE_DOWN: if (_y < (packet::MAP_HEIGHT - 1)) _y = _y + 1; break;
					case packet::MOVE_TYPE::MOVE_LEFT: if (_x > 0) _x = _x - 1; break;
					case packet::MOVE_TYPE::MOVE_RIGHT: if (_x < (packet::MAP_WIDTH - 1)) _x = _x + 1; break;
				}

				packet::sc_packet_move mp;
				mp._size = sizeof(mp);
				mp._type = packet::PacketType::S2C_P_MOVE;
				mp._id = _id;
				mp._x = _x;
				mp._y = _y;

				for (auto& u : g_users)
				{
					std::shared_ptr<SESSION> p = u.second;

					if ((nullptr != p) && (p->_state == SESSION_STATE::ST_INGAME))
						p->do_send(&mp);
				}
				break;
			}
			default:
				std::cout << "Error Invalid Packet Type\n";
				exit(-1);
		}
	}
}

