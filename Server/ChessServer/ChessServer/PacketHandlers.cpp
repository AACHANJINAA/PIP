#include "pch.h"
#include "PacketHandlers.h"

namespace chess::packet
{
    // PacketHandlers.cpp 수정안
    void Handle_C2S_LOGIN(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream)
    {
		// 3. 다른 모든 유저에게 '나의 입장'을 알림
		{
			// 입장 패킷 조립 (가변 길이 name 포함)
			packet::PacketStream enterDataStream;
			enterDataStream << session->_id << (char)0 /*object_type*/ << session->_x << session->_y << session->_name;

			packet::PacketHeader enterHeader;
			enterHeader._type = static_cast<uint16_t>(packet::PacketType::S2C_P_ENTER);
			enterHeader._size = sizeof(enterHeader) + enterDataStream.Size();

			packet::PacketStream finalEnterStream;
			finalEnterStream << enterHeader;
			finalEnterStream.Write(enterDataStream.Data(), enterDataStream.Size());

			for (auto& user_pair : chess::g_users)
			{
				if (user_pair.first == session->_id) continue;

				auto other_session = user_pair.second;
				if (other_session && other_session->_state == server::SESSION_STATE::ST_INGAME)
				{
					other_session->do_send(finalEnterStream.Data(), finalEnterStream.Size());
				}
			}
		}

		// 4. 나에게 '다른 유저들의 정보'를 전송
		for (auto& user_pair : chess::g_users)
		{
			if (user_pair.first == session->_id) continue;

			auto other_session = user_pair.second;
			if (other_session && other_session->_state == server::SESSION_STATE::ST_INGAME)
			{
				// 다른 유저의 입장 패킷 조립
				packet::PacketStream otherEnterDataStream;
				otherEnterDataStream << other_session->_id << (char)0 << other_session->_x << other_session->_y << other_session->_name;

				packet::PacketHeader otherHeader;
				otherHeader._type = static_cast<uint16_t>(packet::PacketType::S2C_P_ENTER);
				otherHeader._size = sizeof(otherHeader) + otherEnterDataStream.Size();

				packet::PacketStream finalOtherStream;
				finalOtherStream << otherHeader;
				finalOtherStream.Write(otherEnterDataStream.Data(), otherEnterDataStream.Size());

				// '나'에게 전송
				session->do_send(finalOtherStream.Data(), finalOtherStream.Size());
			}
		}
    }

	void Handle_C2S_MOVE(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream)
    {
		// 1. 역직렬화: 스트림에서 이동 방향을 읽어온다.
		packet::MOVE_TYPE direction;
		try
		{
			stream >> direction;
		}
		catch (...) 
		{
			__debugbreak();
			return;
		}

		// 2. 세션의 좌표를 업데이트한다 (월드 경계 체크 포함)
		switch (direction)
		{
			case packet::MOVE_TYPE::MOVE_UP:    if (session->_y > 0) session->_y--; break;
			case packet::MOVE_TYPE::MOVE_DOWN:  if (session->_y < packet::MAP_HEIGHT - 1) session->_y++; break;
			case packet::MOVE_TYPE::MOVE_LEFT:  if (session->_x > 0) session->_x--; break;
			case packet::MOVE_TYPE::MOVE_RIGHT: if (session->_x < packet::MAP_WIDTH - 1) session->_x++; break;
			default: return; // 정의되지 않은 방향 값은 무시
		}

		// 3. 이동 결과를 모든 클라이언트에게 브로드캐스팅한다.
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

		// 게임중인 모든 유저에게 전송
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
