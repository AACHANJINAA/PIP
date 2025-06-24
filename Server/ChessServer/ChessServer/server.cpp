#include "server.h"
#include "main.h"
#include <ranges>

namespace chess::server
{
	//exp_over
	EXP_OVER::EXP_OVER(long long id, const std::vector<char>& data) : _id(id)
	{
		ZeroMemory(&_send_over, sizeof(_send_over));

		auto packet_size = data.size();
		if (packet_size > 1024)
		{
			std::cout << "MESSAGE TOO LONG";
			exit(-1);
		}
		memcpy(_send_buffer.data(), data.data(), packet_size);
		_send_wsabuf[0].buf = _send_buffer.data();
		_send_wsabuf[0].len = static_cast<ULONG>(packet_size);
	}

	//session
	SESSION::SESSION()
	{
		std::cout << "DEFAULT SESSION CONSTRUCTOR CALLED!!\n";
		exit(-1);
	}

	SESSION::SESSION(long long session_id, SOCKET s) : _id(session_id), _c_socket(s)
	{
		_recv_wsabuf[0].len = sizeof(_recv_buffer);
		_recv_wsabuf[0].buf = _recv_buffer.data();

		_recv_over.hEvent = reinterpret_cast<HANDLE>(session_id);

		

		// 클라이언트에게 ID 전송

		do_recv();
		
	}

	SESSION::~SESSION()
	{
		closesocket(_c_socket);

		std::cout << "클라이언트 [" << _id << "] 연결 종료" << std::endl;
	}

	void SESSION::do_recv()
	{
		DWORD recv_flag = 0;
		ZeroMemory(&_recv_over, sizeof(_recv_over));
		_recv_over.hEvent = reinterpret_cast<HANDLE>(_id);
		auto ret = WSARecv(_c_socket, _recv_wsabuf.data(), 1, NULL, &recv_flag, &_recv_over, g_recv_callback);
		if (0 != ret)
		{
			auto err_no = WSAGetLastError();
			main::error_display("WSARecv 에서", err_no);
		}
	}

	void SESSION::process_command(packet::CommandType cmd)
	{
		auto& position = main::g_positions[_id];
		switch (cmd)
		{
		case chess::packet::CommandType::MOVE_UP:
			position.y = (position.y + 7) % 8;
			break;
		case chess::packet::CommandType::MOVE_DOWN:
			position.y = (position.y + 1) % 8;
			break;
		case chess::packet::CommandType::MOVE_LEFT:
			position.x = (position.x + 7) % 8;
			break;
		case chess::packet::CommandType::MOVE_RIGHT:
			position.x = (position.x + 1) % 8;
			break;
		case chess::packet::CommandType::error:
			std::cout << "에러?" << std::endl;
			break;
		case chess::packet::CommandType::CONNECT:
			std::cout << "클라이언트 [" << _id << "] 연결" << std::endl;
			break;
		case chess::packet::CommandType::DISCONNECT:
			std::cout << "클라이언트 [" << _id << "] 연결 종료 요청" << std::endl;
			main::g_positions.erase(_id);
			main::g_users.erase(_id);
			break;
		default:
			std::cout << "Unknown command received" << std::endl;
			break;
		}
		return;
	}

	void SESSION::recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag)
	{
		if (err != 0 || num_bytes == 0)
		{
			// 에러 발생
			if (err == WSAECONNRESET)
			{
				std::cout << "클라이언트 [" << _id << "] 비정상 종료" << std::endl;
			}
			else
			{
				std::cout << "에러 발생: " << err << std::endl;
			}

			// 클라이언트 연결 종료 처리
			if (main::g_positions.contains(_id))
			{
				main::g_positions.erase(_id);
			}
			if (main::g_users.contains(_id))
			{
				main::g_users.erase(_id);
			}

			return;
		}

		auto command_packet = packet::CommandPacket::deserialize(_recv_buffer.data(), num_bytes);
		process_command(command_packet.command);

		spread_users();

		if (packet::CommandType::DISCONNECT == command_packet.command)
		{
			return;
		}
		do_recv();
	}

	void SESSION::spread_users()
	{
		std::vector<char> all_positions_data;
		//std::cout << "g_positions size: " << main::g_positions.size() << std::endl;
		for (const auto& [id, position] : main::g_positions)
		{
			auto position_packet = chess::packet::PositionPacket{ id, position.x, position.y };
			auto serialized_data = position_packet.serialize();
			all_positions_data.insert(all_positions_data.end(), serialized_data.begin(), serialized_data.end());
		}

		// 현재 세션에게 자신의 위치를 포함한 모든 유저의 좌표를 전송
		// 다른 모든 세션에게 데이터를 전송
		for (auto& u : main::g_users | std::views::values)
		{
			u.do_send(_id, all_positions_data); // 모든 유저에게 모든 유저의 좌표를 보내줌
		}
	}

	void SESSION::do_send(long long id, const std::vector<char>& data)
	{
		EXP_OVER* o = new EXP_OVER(id, data);
		DWORD size_sent;
		int ret = WSASend(_c_socket, o->_send_wsabuf.data(), 1, &size_sent, 0, &(o->_send_over), g_send_callback);
		if (SOCKET_ERROR == ret)
		{
			auto err_no = WSAGetLastError();
			main::error_display("WSASend 에서", err_no);
		}
	}

	void SESSION::send_id() // 추가
	{
		packet::PositionPacket packet{ _id, 0, 0 };
		
		std::vector<char> all_positions_data = packet.serialize();
		
		for (const auto& [id, position] : main::g_positions)
		{
			auto position_packet = chess::packet::PositionPacket{ id, position.x, position.y };
			auto serialized_data = position_packet.serialize();
			all_positions_data.insert(all_positions_data.end(), serialized_data.begin(), serialized_data.end());
		}

		// 현재 세션에게 자신의 위치를 포함한 모든 유저의 좌표를 전송
		// 다른 모든 세션에게 데이터를 전송
		do_send(_id, all_positions_data);
		for (auto& [id, u] : main::g_users )
		{
			if (id != _id )
			{
				u.do_send(_id, all_positions_data); // 모든 유저에게 모든 유저의 좌표를 보내줌
			}
		}
	}
}
