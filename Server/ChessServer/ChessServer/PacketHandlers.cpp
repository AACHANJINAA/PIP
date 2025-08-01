#include "pch.h"
#include "PacketHandlers.h"
#include "server.h"
#include "Room.h"

namespace chess::packet
{

	// 중복 코드를 줄이기 위한 Helper 함수
	PacketStream MakeEnterPacket(std::shared_ptr<chess::server::SESSION> session)
	{
		PacketStream dataStream;
		dataStream << session->_id << (char)0 /*object_type*/ << session->_x << session->_y << session->_name;

		SC_PACKET_ENTER enterPacket;
		enterPacket._type = PacketType::S2C_P_ENTER;
		enterPacket._size = sizeof(enterPacket) + dataStream.Size();

		PacketStream finalStream;
		finalStream << enterPacket;
		finalStream.Write(dataStream.Data(), dataStream.Size());
		return finalStream;
	}
	

	void Handle_C2S_LOGIN(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream)
	{
		// ... (내용 수정 없음)
	}

	void Handle_C2S_MOVE(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream)
	{
		// ... (내용은 거의 동일, 패킷 생성 부분만 수정)
		packet::MOVE_TYPE direction;
		try { stream >> direction; } catch (...) { return; }

		switch (direction)
		{
			case packet::MOVE_TYPE::MOVE_UP:    if (session->_y < packet::MAP_HEIGHT - 1) session->_y++; break; 
			case packet::MOVE_TYPE::MOVE_DOWN:  if (session->_y > 0) session->_y--; break;
			case packet::MOVE_TYPE::MOVE_LEFT:  if (session->_x > 0) session->_x--; break;
			case packet::MOVE_TYPE::MOVE_RIGHT: if (session->_x < packet::MAP_WIDTH - 1) session->_x++; break;
			default: return;
		}

		packet::SC_PACKET_MOVE movePacket;
		movePacket._type = PacketType::S2C_P_MOVE;
		movePacket._size = sizeof(movePacket);
		movePacket._id = session->_id;
		movePacket._x = session->_x;
		movePacket._y = session->_y;

		packet::PacketStream finalMoveStream;
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

	void Handle_C2S_ATTACK(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream)
	{
		// ... (내용은 거의 동일, 패킷 생성 부분만 수정)
		int dx[] = { 0, 0, -1, 1 };
		int dy[] = { 1, -1, 0, 0 };

		for (int i = 0; i < 4; ++i)
		{
			int target_x = session->_x + dx[i];
			int target_y = session->_y + dy[i];

			if (target_x < 0 || target_x >= MAP_WIDTH || target_y < 0 || target_y >= MAP_HEIGHT) continue;

			std::shared_ptr<chess::server::SESSION> target_session = nullptr;
			for (auto& user_pair : chess::g_users)
			{
				auto other_session = user_pair.second;
				if (other_session && other_session->_state == server::SESSION_STATE::ST_INGAME &&
					other_session->_id != session->_id &&
					other_session->_x == target_x && other_session->_y == target_y)
				{
					target_session = other_session;
					break;
				}
			}

			if (target_session)
			{
				int32_t damage = 10;
				int32_t old_hp = target_session->_hp.fetch_sub(static_cast<short>(damage));
				int32_t new_hp = old_hp - damage;
				if (new_hp < 0) { new_hp = 0; target_session->_hp.store(0); }

				SC_PACKET_ATTACK attackPacket;
				attackPacket._type = PacketType::S2C_P_ATTACK;
				attackPacket._size = sizeof(attackPacket);
				attackPacket._attacker_id = session->_id;
				attackPacket._target_id = target_session->_id;
				attackPacket._damage = damage;
				attackPacket._target_current_hp = new_hp;

				PacketStream finalAttackStream;
				finalAttackStream << attackPacket;

				for (auto& val : chess::g_users | std::views::values)
				{
					auto broadcast_session = val;
					if (broadcast_session && broadcast_session->_state == server::SESSION_STATE::ST_INGAME)
					{
						broadcast_session->do_send(finalAttackStream.Data(), finalAttackStream.Size());
					}
				}
			}
		}
	}

	void handle_C2S_ENTER_ROOM(std::shared_ptr<server::SESSION> session, packet::PacketStream& stream)
	{
		CS_PACKET_ENTER_ROOM enter_packet;
		stream >> enter_packet;

		server::Room* room = server::Server::Instance()->GetRoom(enter_packet._room_id);

		SC_PACKET_ENTER_ROOM_ACK ack_packet;
		ack_packet._type = PacketType::S2C_P_ENTER_ROOM_ACK;
		ack_packet._size = sizeof(ack_packet);
		ack_packet._room_id = enter_packet._room_id;

		if (room == nullptr) {
			ack_packet._success = false;
		} else {
			if (session->_room_id != -1) {
				server::Room* old_room = server::Server::Instance()->GetRoom(session->_room_id);
				if (old_room) old_room->RemovePlayer(session->_id);
			}
			room->AddPlayer(session);
			session->_room_id = enter_packet._room_id;
			session->_state = server::SESSION_STATE::ST_INGAME;
			ack_packet._success = true;
		}

		PacketStream ack_stream;
		ack_stream << ack_packet;
		session->do_send(ack_stream.Data(), ack_stream.Size());
	}

	void handle_C2S_ROOM_LIST(std::shared_ptr<server::SESSION> session, PacketStream& stream)
	{
		std::vector<server::RoomInfo> room_infos;
		for (int i = 0; i < 100; ++i)
		{
			server::Room* room = server::Server::Instance()->GetRoom(i);
			if (room)
			{
				server::RoomInfo info;
				info._room_id = room->GetRoomId();
				info._player_count = room->GetPlayerCount();
				room_infos.push_back(info);
			}
		}

		SC_PACKET_ROOM_LIST_ACK ack_packet;
		ack_packet._type = PacketType::S2C_P_ROOM_LIST_ACK;
		ack_packet._room_count = room_infos.size();
		ack_packet._size = sizeof(ack_packet) + (sizeof(server::RoomInfo) * ack_packet._room_count);

		PacketStream ack_stream;
		ack_stream << ack_packet;
		ack_stream.Write(reinterpret_cast<const char*>(room_infos.data()), sizeof(server::RoomInfo) * ack_packet._room_count);

		session->do_send(ack_stream.Data(), ack_stream.Size());
		LOG("Sent room list to session " << session->_id << ". Room count: " << ack_packet._room_count);
	}
}