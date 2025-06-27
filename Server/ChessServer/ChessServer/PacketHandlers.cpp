#include "pch.h"
#include "PacketHandlers.h"

namespace chess::packet
{

	// 중복 코드를 줄이기 위한 Helper 함수
	PacketStream MakeEnterPacket(std::shared_ptr<chess::server::SESSION> session)
	{
		PacketStream dataStream;
		dataStream << session->_id << (char)0 /*object_type*/ << session->_x << session->_y << session->_name;

		PacketHeader header;
		header._type = static_cast<uint16_t>(PacketType::S2C_P_ENTER);
		header._size = sizeof(header) + dataStream.Size();

		PacketStream finalStream;
		finalStream << header;
		finalStream.Write(dataStream.Data(), dataStream.Size());
		return finalStream;
	}
	

	void Handle_C2S_LOGIN(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream)
	{
		// 1. 클라이언트가 보낸 데이터(name)를 추출
		std::string name;
		try
		{
			stream >> name;
		}
		catch (const std::runtime_error& e)
		{
			ERROR("[HANDLER C2S_LOGIN] **ERROR**: Failed to read name from stream for Session " << session->_id << ". " << e.what() << std::endl);
			return;
		}

		LOG("[HANDLER C2S_LOGIN] Session " << session->_id << " logged in with name: '" << name << "'");

		// 2. 서버에 세션 정보 채우기 (가장 중요한 부분!)
		session->_name = name;
		session->_x = 4;
		session->_y = 4;
		session->_state = server::SESSION_STATE::ST_INGAME;

		// 3. '나 자신'에게 나의 상세 정보(아바타 정보)를 보냄
		LOG("[HANDLER C2S_LOGIN] Sending AVATAR_INFO to " << session->_name << "." << std::endl);
		session->send_player_info_packet();

		// 4. '다른 모든 유저'에게 '나의 입장'을 알림
		{
			LOG("[HANDLER C2S_LOGIN] Broadcasting ENTER packet for " << session->_name << " to other players." << std::endl);
			PacketStream myEnterPacket = MakeEnterPacket(session);
			for (auto& user_pair : chess::g_users)
			{
				if (user_pair.first == session->_id) continue;

				auto other_session = user_pair.second;
				if (other_session && other_session->_state == server::SESSION_STATE::ST_INGAME)
				{
					other_session->do_send(myEnterPacket.Data(), myEnterPacket.Size());
				}
			}
		}

		// 5. '나 자신'에게 '이미 접속해 있던 다른 유저들의 정보'를 전송
		{
			LOG("[HANDLER C2S_LOGIN] Sending existing players' info to " << session->_name << "." << std::endl);
			for (auto& user_pair : chess::g_users)
			{
				if (user_pair.first == session->_id) continue;

				auto other_session = user_pair.second;
				if (other_session && other_session->_state == server::SESSION_STATE::ST_INGAME)
				{
					PacketStream otherEnterPacket = MakeEnterPacket(other_session);
					session->do_send(otherEnterPacket.Data(), otherEnterPacket.Size());
				}
			}
		}
	}

	void Handle_C2S_MOVE(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream)
	{
		packet::MOVE_TYPE direction;
		try
		{
			stream >> direction;
		}
		catch (...) { return; }

		LOG("[HANDLER C2S_MOVE] Session " << session->_id << " requests move in direction: " << static_cast<int>(direction) << std::endl);

		switch (direction)
		{
			case packet::MOVE_TYPE::MOVE_UP:    if (session->_y > 0) session->_y--; break;
			case packet::MOVE_TYPE::MOVE_DOWN:  if (session->_y < packet::MAP_HEIGHT - 1) session->_y++; break;
			case packet::MOVE_TYPE::MOVE_LEFT:  if (session->_x > 0) session->_x--; break;
			case packet::MOVE_TYPE::MOVE_RIGHT: if (session->_x < packet::MAP_WIDTH - 1) session->_x++; break;
			default: return;
		}

		LOG("[HANDLER C2S_MOVE] Session " << session->_id << " new position: (" << session->_x << ", " << session->_y << ")" << std::endl);

		packet::SC_PACKET_MOVE movePacket;
		movePacket._id = session->_id;
		movePacket._x = session->_x;
		movePacket._y = session->_y;

		packet::PacketHeader header;
		header._type = static_cast<uint16_t>(packet::PacketType::S2C_P_MOVE);
		header._size = sizeof(header) + sizeof(movePacket);

		packet::PacketStream finalMoveStream;
		finalMoveStream << header;
		finalMoveStream << movePacket;

		for (auto& user_pair : chess::g_users)
		{
			auto other_session = user_pair.second;
			if (other_session && other_session->_state == server::SESSION_STATE::ST_INGAME)
			{
				other_session->do_send(finalMoveStream.Data(), finalMoveStream.Size());
			}
		}
	}
}
