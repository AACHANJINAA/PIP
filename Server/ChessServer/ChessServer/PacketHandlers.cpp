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
		finalStream.Write(dataStream.constable_data(), dataStream.Size());
		return finalStream;
	}
	

	void Handle_C2S_LOGIN(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream)
	{
		std::string player_name;
		try
		{
			stream >> player_name;
		}
		catch (const std::runtime_error& e)
		{
			LOG("[Login] **ERROR**: Failed to read player name from stream. " << e.what());
			// 여기서 세션 접속을 끊는 등의 처리를 할 수 있습니다.
			return;
		}
		// 2. 세션 객체에 이름을 저장합니다.
		session->_name = player_name;
		
		LOG("[Login] Session " << session->_id << " logged in as '" << session->_name << "'.");
		
		// 3. 클라이언트에게 아바타(캐릭터) 정보를 보내줍니다.
		//    이를 통해 클라이언트는 자신의 플레이어 객체를 생성할 수 있습니다.
		packet::SC_PACKET_AVATAR_INFO avatar_info_packet;
		avatar_info_packet._type = PacketType::S2C_P_AVATAR_INFO;
		avatar_info_packet._size = sizeof(avatar_info_packet);
		avatar_info_packet._id = session->_id;
		avatar_info_packet._x = session->_x; // 세션 생성 시의 기본 위치
		avatar_info_packet._y = session->_y;
		avatar_info_packet._hp = session->_hp;
		avatar_info_packet._level = 1; // 임시 레벨
		avatar_info_packet._exp = 0;   // 임시 경험치
		
		// 4. 생성된 패킷을 전송합니다.
		session->do_send(reinterpret_cast<const char*>(&avatar_info_packet), sizeof(avatar_info_packet));
		
		LOG("[Login] Sent AVATAR_INFO to session " << session->_id);
		
		// MO 서버에서는 로비에서 다른 플레이어 정보를 보내줄 필요는 없습니다.
		// 방에 입장했을 때, 해당 방의 플레이어 정보만 받으면 됩니다.
	}

	void Handle_C2S_MOVE(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream)
	{
		if (session->_state != server::SESSION_STATE::ST_INGAME || session->_room_id == -1) return;
		server::Room * room = server::Server::Instance()->GetRoom(session->_room_id);
		if (room == nullptr) return;

		// 3. 이동 처리 (기존 로직 동일)
		packet::MOVE_TYPE direction;
		try { stream >> direction; }
		catch (...) { return; }

		switch (direction)
		{
			case packet::MOVE_TYPE::MOVE_UP:    if (session->_y < packet::MAP_HEIGHT - 1) session->_y++; break;
			case packet::MOVE_TYPE::MOVE_DOWN:  if (session->_y > 0) session->_y--; break;
			case packet::MOVE_TYPE::MOVE_LEFT:  if (session->_x > 0) session->_x--; break;
			case packet::MOVE_TYPE::MOVE_RIGHT: if (session->_x < packet::MAP_WIDTH - 1) session->_x++; break;
			default: return;
		}

		LOG("[Move] Session " << session->_id << " in Room " << session->_room_id << " moved to(" << session->_x << ", " << session->_y << ")");

		// 4. 이동 패킷 생성 (기존 로직 동일)
		packet::SC_PACKET_MOVE movePacket;
		movePacket._type = PacketType::S2C_P_MOVE;
		movePacket._size = sizeof(movePacket);
		movePacket._id = session->_id;
		movePacket._x = session->_x;
		movePacket._y = session->_y;

		// 5. [수정] 방에 있는 모든 플레이어에게 브로드캐스팅
		room->Broadcast(reinterpret_cast<const char*>(&movePacket), sizeof(movePacket));
	}

	void Handle_C2S_ATTACK(std::shared_ptr<chess::server::SESSION> session, chess::packet::PacketStream& stream)
	{
		// 1. 세션과 방의 유효성 검사
		if (session->_state != server::SESSION_STATE::ST_INGAME || session->_room_id == -1) 
			return;
		server::Room* room = server::Server::Instance()->GetRoom(session->_room_id);
		if (room == nullptr) return;

		// 2. 실제 공격 처리는 Room 객체에 위임
		room->HandleAttack(session);
	}

	void Handle_C2S_ENTER_ROOM(std::shared_ptr<server::SESSION> session, packet::PacketStream& stream)
	{
		LOG("[EnterRoomHandler] Stream received. Buffer size: " << stream.Size() << ", Pos: " << stream.Pos());
		int room_id;
		try
		{
			stream >> room_id;
		}
		catch (const std::runtime_error& e)
		{
			LOG("[EnterRoomHandler] **EXCEPTION CAUGHT**: " << e.what());
			// 예외가 발생했을 때, 스트림의 내부 상태를 다시 한번 확인합니다.
			// 이 로그는 throw 이후라 출력되지 않을 수 있으므로, try 블록 전에 확인하는 것이 더좋습니다.
			return;
		}

		server::Room* room = server::Server::Instance()->GetRoom(room_id);

		SC_PACKET_ENTER_ROOM_ACK ack_packet;
		ack_packet._type = PacketType::S2C_P_ENTER_ROOM_ACK;
		ack_packet._size = sizeof(ack_packet);
		ack_packet._room_id = room_id;

		if (room == nullptr) {
			ack_packet._success = false;
		}
		else if (room->IsFull() || room->GetRoomState() == server::RoomState::PLAYING) // [수정]
		{
		    ack_packet._success = false;
			LOG("[EnterRoom] Session " << session->_id << " failed to enter Room " << room->GetRoomId() << ". Reason: Full or Already Playing.");
		}
		else 
		{
			if (session->_room_id != -1)
			{
				server::Room* old_room = server::Server::Instance()->GetRoom(session->_room_id);
				if (old_room) old_room->RemovePlayer(session->_id);
			}
			room->AddPlayer(session);
			LOG("[EnterRoom] Session " << session->_id << " successfully entered Room " << room_id);
			session->_room_id = room_id;
			session->_state = server::SESSION_STATE::ST_INGAME;

			// [추가] 세션의 담당 로직 스레드 인덱스를 방의 인덱스와 동기화
			session->_logic_thread_idx = room->GetLogicThreadIndex();
			LOG("[EnterRoom] Session " << session->_id << " logic thread index updated to " << session->_logic_thread_idx);

			ack_packet._success = true;
		}

		PacketStream ack_stream;
		ack_stream << ack_packet;
		session->do_send(ack_stream.constable_data(), ack_stream.Size());
	}

	void Handle_C2S_ROOM_LIST(std::shared_ptr<server::SESSION> session, PacketStream& stream)
	{
		std::vector<RoomInfo> room_infos;
		for (int i = 0; i < 100; ++i)
		{
			server::Room* room = server::Server::Instance()->GetRoom(i);
			if (room)
			{
				RoomInfo info;
				info._room_id = room->GetRoomId();
				info._player_count = room->GetPlayerCount();
				room_infos.push_back(info);
			}
		}

		SC_PACKET_ROOM_LIST_ACK ack_packet;
		ack_packet._type = PacketType::S2C_P_ROOM_LIST_ACK;
		ack_packet._room_count = room_infos.size();
		ack_packet._size = sizeof(ack_packet) + (sizeof(RoomInfo) * ack_packet._room_count);

		PacketStream ack_stream;
		ack_stream << ack_packet;
		ack_stream.Write(reinterpret_cast<const char*>(room_infos.data()), sizeof(RoomInfo) * ack_packet._room_count);

		session->do_send(ack_stream.constable_data(), ack_stream.Size());
		LOG("Sent room list to session " << session->_id << ". Room count: " << ack_packet._room_count);
	}

	void Handle_C2S_CHAT_IN_ROOM(std::shared_ptr<server::SESSION> session, packet::PacketStream& stream)
	{
		// 1. 채팅 메시지 읽기
		// PacketStream의 >> 연산자는 먼저 길이를 읽고, 그 길이만큼 문자열을 읽어옵니다.
		std::string message;
		try
		{
			stream >> message;
		}
		catch (const std::runtime_error& e)
		{
			LOG("[CHAT] **ERROR**: Failed to read chat message from stream. " << e.what());
			return;
		}

		// 2. 세션이 방에 있는지 확인
		if (session->_state != server::SESSION_STATE::ST_INGAME || session->_room_id == -1)
		{
			LOG("[CHAT] Session " << session->_id << " sent chat message from outside a room.");
			return;
		}

		// 3. 방 객체 가져오기
		server::Room* room = server::Server::Instance()->GetRoom(session->_room_id);
		if (room == nullptr) return;

		// 4. 방에 있는 모든 사람에게 채팅 메시지 브로드캐스팅
		packet::SC_PACKET_CHAT_IN_ROOM chat_packet;
		chat_packet._type = packet::PacketType::S2C_P_CHAT_IN_ROOM;
		chat_packet._sender_id = session->_id;

		packet::PacketStream broadcast_stream;
		broadcast_stream << chat_packet;
		broadcast_stream << message; // string을 스트림에 쓰면 길이(uint16_t)가 먼저 쓰이고 내용이 쓰임

		// 최종 패킷 크기를 헤더에 다시 설정
		// broadcast_stream의 맨 앞을 PacketHeader*로 캐스팅하여 size 멤버를 수정
		packet::PacketHeader* header_ptr = reinterpret_cast<packet::PacketHeader*>(broadcast_stream.mutable_data());
		header_ptr->_size = broadcast_stream.Size();

		room->Broadcast(broadcast_stream.constable_data(), broadcast_stream.Size());
		LOG("[CHAT] Room " << room->GetRoomId() << " | " << session->_id << ": " << message);
	}
}
