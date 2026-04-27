#include "pch.h"
#include "PacketHandlers.h"

#include <algorithm>

#include "DBManager.h"
#include "InventoryComponent.h"
#include "MapDataManager.h"
#include "Player.h"
#include "server.h"
#include "Room.h"

namespace PIP::packet
{
	
	// 중복 코드를 줄이기 위한 Helper 함수
	PacketStream MakeSpawnPlayerPacket(const std::shared_ptr<PIP::SERVER::SESSION>& session)
	{
		// [수정] SC_PACKET_SPAWN_PLAYER 구조체 변수를 선언하고 멤버를 채웁니다.
		packet::SC_PACKET_SPAWN_PLAYER spawn_packet_data;
		spawn_packet_data._type = common::packet::PacketType::S2C_P_SPAWN_PLAYER; // 타입 설정
		spawn_packet_data._size = 0; // 임시 크기 (나중에 다시 계산)

		spawn_packet_data._id = session->_id;
		spawn_packet_data._position = session->_player->GetPosition();
		spawn_packet_data._rotation = session->_player->GetRotation();
		spawn_packet_data._hp = session->_player->GetHP();
		spawn_packet_data._level = session->_player->_level;
		spawn_packet_data._exp = session->_player->_exp;
		spawn_packet_data._state = session->_player->_state;

		packet::PacketStream finalStream;
		// [수정] 구조체 자체를 스트림에 씁니다.
		finalStream << spawn_packet_data;
		// [추가] 이름(가변 길이)을 스트림에 씁니다.
		finalStream << session->_player->GetName();

		// [수정] 최종 크기를 계산하여 패킷 헤더에 덮어씁니다.
		auto* final_header = reinterpret_cast<packet::PacketHeader*>(finalStream.mutable_data());
		final_header->_size = static_cast<uint16_t>(finalStream.Size());

		return finalStream; // finalStream을 반환
	}


	void Handle_C2S_LOGIN(const std::shared_ptr<SERVER::SESSION>& session, PIP::packet::PacketStream& stream)
	{
		std::string player_name;
		try
		{
			packet::CS_PACKET_LOGIN login_packet;
			stream >> login_packet;
			stream >> player_name;
		}
		catch (const std::runtime_error& e)
		{
			MYERROR("[Login] **ERROR**: Failed to read player name from stream. " << e.what());
			// 여기서 세션 접속을 끊는 등의 처리를 할 수 있습니다.
			return;
		}
		// 2. 세션 객체에 이름을 저장합니다.
		session->_player->SetName(player_name);
		
		//MYLOG("[Login] Session " << session->_id << " logged in as '" << session->_player->GetName() << "'.");

		// 아바타 정보 전송 로직 제거

		packet::SC_PACKET_LOGIN_ACK login_ack_packet;
		login_ack_packet._type = PacketType::S2C_P_LOGIN_ACK;
		login_ack_packet._size = sizeof(login_ack_packet);
		login_ack_packet._my_session_id = session->_id; // 클라이언트 자신의 ID를 알려줍니다.
		login_ack_packet._success = true;

		session->do_send(reinterpret_cast<const char*>(&login_ack_packet), sizeof(login_ack_packet));
		//MYLOG("[Login] Sent LOGIN_ACK to session " << session->_id << " with ID: " << session->_id);
	}

	void Handle_C2S_MOVE(const std::shared_ptr<SERVER::SESSION>& session, PIP::packet::PacketStream& stream)
	{

		if (session->_state != SERVER::SESSION_STATE::ST_INGAME || session->_room_id == -1) return;

		// 1. 데이터만 먼저 복사 (네트워크 스레드에서 수행)
		packet::CS_PACKET_MOVE move_packet;
		try
		{
			stream >> move_packet;
		}
		catch (...)
		{
			MYERROR("이동 패킷 읽는중 오류남 (패킷에러)");
			return;;
		}

		auto room = SERVER::Server::Instance()->GetRoom(session->_room_id);
		if (room)
		{
			// 2. 실제 좌표 수정 및 Jolt 물리 검증 로직을 '방의 잡 큐'로 던짐
			room->PushJob([session, move_packet, room]() {
				// [이 안의 코드는 로직 스레드에서 실행됨]
				// - Jolt 바디 위치 강제 이동
				// - Overlap 체크 및 롤백 판정
				// - 결과 브로드캐스트
				room->Execute_C2S_MOVE(session, move_packet);
				});
		}


		//if (session->_state != server::SESSION_STATE::ST_INGAME || session->_room_id == -1) return;

		//server::Room * room = server::Server::Instance()->GetRoom(session->_room_id);
		//if (room == nullptr) return;

		//packet::CS_PACKET_MOVE move_packet;
		//try
		//{
		//	stream >> move_packet;
		//}
		//catch (...)
		//{
		//	MYERROR("이동 패킷 읽는중 오류남 (패킷에러)");
		//	return;;
		//}
		//common::Vec3 targetPos = move_packet._position;
		//common::Vec4 targetRotation = move_packet._rotation;
		//common::Vec3 player_extents = { 0.5f, 0.9f, 0.5f };
		//session->_player->_state = move_packet._state;

		//// 1. 맵 범위 체크 (heightmap 범위 밖으로 나가지 못하게)
		//float groundHeight = MapDataManager::Instance()->GetGroundHeight(targetPos.x, targetPos.z);
		//// [로그 추가] 클라이언트가 보낸 Y와 서버 지형 높이 비교
		//if (std::abs(targetPos.y - groundHeight) > 0.1f) {
		//	MYLOG("[Move] Height Mismatch! ID: " << session->_id
		//		<< " | ClientY: " << targetPos.y
		//		<< " | ServerGroundY: " << groundHeight);
		//}
		//// groundHeight가 0이면 맵 밖! (GetGroundHeight가 범위 밖에서 0 반환)
		//if (!MapDataManager::Instance()->IsInsideMap(targetPos.x, targetPos.z))
		//{
		//	MYLOG("[Move] Out of Bounds! ID: " << session->_id << " at (" << targetPos.x << ", " << targetPos.z << ")");

		//	// 맵 밖으로 나가려는 시도 - 클라이언트에게 위치 보정 패킷 전송 (원위치 또는 안전한 위치)
		//	packet::SC_PACKET_MOVE correction_packet;
		//	correction_packet._type = common::packet::PacketType::S2C_P_MOVE;
		//	correction_packet._size = sizeof(correction_packet);
		//	correction_packet._id = session->_id;
		//	correction_packet._position = session->_player->_position; // 서버가 알고 있는 마지막 유효 위치
		//	correction_packet._rotation = targetRotation;
		//	correction_packet._state = session->_player->_state;
		//	session->_player->_rotation = targetRotation; // 회전은 일단 갱신

		//	room->Broadcast(reinterpret_cast<char*>(&correction_packet), sizeof(correction_packet));
		//	return;
		//}
		//groundHeight = MapDataManager::Instance()->GetGroundHeight(targetPos.x, targetPos.z);

		//targetPos.y = groundHeight;

		//// 3. 충돌 체크
		//if (MapDataManager::Instance()->CheckForCollision(targetPos, player_extents))
		//{
		//	// 충돌 발생! 안전한 위치 찾기
		//	common::Vec3 safe_pos = session->_player->_position;

		//	// 3-1. 이전 위치가 안전한지 체크
		//	if (!MapDataManager::Instance()->CheckForCollision(safe_pos, player_extents))
		//	{
		//		// 이전 위치가 안전함 - 그대로 사용
		//		MYLOG("Collision detected at (" << targetPos.x << ", " << targetPos.y << ", " << targetPos.z
		//			<< ") - reverting to previous position");
		//	}
		//	else
		//	{
		//		// 3-2. 이전 위치도 충돌! (플레이어가 오브젝트 안에 갇힘)
		//		// 방법 1: Y축만 위로 올려보기
		//		safe_pos.y = groundHeight + player_extents.y + 2.0f; // 2m 위로

		//		if (MapDataManager::Instance()->CheckForCollision(safe_pos, player_extents))
		//		{
		//			// 방법 2: 그래도 충돌이면 스폰 위치 근처로
		//			// 랜덤한 안전한 위치 찾기 시도
		//			bool found_safe = false;
		//			for (int attempt = 0; attempt < 10; ++attempt)
		//			{
		//				float offset_x = (rand() % 20 - 10) * 0.5f; // -5 ~ +5m
		//				float offset_z = (rand() % 20 - 10) * 0.5f;

		//				common::Vec3 test_pos = {
		//					safe_pos.x + offset_x,
		//					safe_pos.y,
		//					safe_pos.z + offset_z
		//				};

		//				float test_ground = MapDataManager::Instance()->GetGroundHeight(test_pos.x, test_pos.z);
		//				if (test_ground > 0.0f) // 맵 안
		//				{
		//					test_pos.y = test_ground + player_extents.y + 1.0f;

		//					if (!MapDataManager::Instance()->CheckForCollision(test_pos, player_extents))
		//					{
		//						safe_pos = test_pos;
		//						found_safe = true;
		//						MYLOG("Found safe position after " << (attempt + 1) << " attempts");
		//						break;
		//					}
		//				}
		//			}

		//			if (!found_safe)
		//			{
		//				// 최후의 수단: 맵 중앙 위쪽으로
		//				safe_pos = { 0.0f, 50.0f, 0.0f };
		//				MYERROR("Player stuck! Teleporting to emergency position (0, 50, 0)");
		//			}
		//		}
		//		else
		//		{
		//			MYLOG("Player was stuck, moved up to Y=" << safe_pos.y);
		//		}
		//	}

		//	// 보정 패킷 전송
		//	packet::SC_PACKET_MOVE correction_packet;
		//	correction_packet._type = common::packet::PacketType::S2C_P_MOVE;
		//	correction_packet._size = sizeof(correction_packet);
		//	correction_packet._id = session->_id;
		//	correction_packet._position = safe_pos;
		//	correction_packet._rotation = targetRotation;
		//	correction_packet._state = session->_player->_state;

		//	// 서버의 플레이어 위치도 안전한 위치로 갱신
		//	session->_player->_position = safe_pos;
		//	session->_player->_rotation = targetRotation;

		//	room->Broadcast(reinterpret_cast<char*>(&correction_packet), sizeof(correction_packet));

		//}
		//else
		//{
		//	// 4. 유효한 이동: 서버에 위치를 갱신하고 브로드캐스팅
		//	session->_player->_position = targetPos;
		//	session->_player->_rotation = targetRotation;

		//	packet::SC_PACKET_MOVE sync_packet;
		//	sync_packet._type = common::packet::PacketType::S2C_P_MOVE;
		//	sync_packet._size = sizeof(sync_packet);
		//	sync_packet._id = session->_id;
		//	sync_packet._position = targetPos;
		//	sync_packet._rotation = targetRotation;
		//	sync_packet._state = session->_player->_state;

		//	room->Broadcast(reinterpret_cast<char*>(&sync_packet), sizeof(sync_packet), sync_packet._id);
		//}


	}

	void Handle_C2S_ATTACK(const std::shared_ptr<SERVER::SESSION>& session, PIP::packet::PacketStream& stream)
	{
		packet::CS_PACKET_ATTACK attack_packet;
		try
		{
			stream >> attack_packet;
			MYLOG("[Attack] Session " << session->_id << " attack packet received.");
		}
		catch (const std::runtime_error& e)
		{
			MYERROR("[Attack] **ERROR**: Failed to read attack packet from stream. " << e.what());
			return;
		}

		// 1. 세션과 방의 유효성 검사
		MYERROR("[ERROR] <Legacy> -> CS_PACKET_ATTACK");
		//SERVER::Room* room = SERVER::Server::Instance()->GetRoom(session->_room_id);
		//if (room)
		//{
		//	room->PushJob([room, session]()
		//		{
		//			if (session->_state != SERVER::SESSION_STATE::ST_INGAME || session->_room_id == -1)
		//				return;
		//			// 2. 실제 공격 처리는 Room 객체에 위임
		//			room->HandleAttack(session);
		//		});
		//}
	}

	void Handle_C2S_ENTER_ROOM(const std::shared_ptr<SERVER::SESSION>& session, packet::PacketStream& stream)
	{
		packet::CS_PACKET_ENTER_ROOM enter_packet;
		stream >> enter_packet;

		SERVER::Room* target_room = SERVER::Server::Instance()->GetRoom(enter_packet._room_id);

		// [추가] 빈 방 자동 배정 로직
		if (!target_room || target_room->IsFull())
		{
			MYLOG("Requested room " << enter_packet._room_id << " is full or invalid. Searching for an empty room...");
			
			bool found_room = false;
			// 서버의 모든 방을 순회 (최대 100개 가정)
			for (int i = 0; i < 100; ++i)
			{
				SERVER::Room* room = SERVER::Server::Instance()->GetRoom(i);
				if (room && !room->IsFull())
				{
					target_room = room;
					enter_packet._room_id = i; // 패킷의 ID도 갱신하여 클라이언트에 알림
					found_room = true;
					MYLOG("Auto-assigned to Room " << i);
					break;
				}
			}

			if (!found_room)
			{
				MYERROR("All rooms are full! Session " << session->_id << " cannot enter.");
				SC_PACKET_ENTER_ROOM_ACK ack_packet;
				ack_packet._type = PacketType::S2C_P_ENTER_ROOM_ACK;
				ack_packet._size = sizeof(ack_packet);
				ack_packet._room_id = enter_packet._room_id;
				ack_packet._success = false;
				session->do_send(reinterpret_cast<const char*>(&ack_packet), sizeof(ack_packet));
				return;
			}
		}

		// [기존 로직] 방 이동 또는 입장 처리
		if (session->_room_id != -1) // [CASE 1] 이미 방에 있는 경우: 퇴장 후 입장
		{
			SERVER::Room* old_room = SERVER::Server::Instance()->GetRoom(session->_room_id);
			if (old_room)
			{
				old_room->PushJob([old_room, target_room, session, enter_packet]()
					{
						// 1. 이전 방 스레드에서 안전하게 퇴장 처리
						old_room->LeavePlayer(session->_id);

						// 2. 퇴장 완료 후, 대상 방 스레드에 입장 요청
						target_room->PushJob([target_room, session, enter_packet]()
							{
								target_room->Execute_C2S_ROOM_ENTER(session, enter_packet);
							});
					});
				return; // Job 체인으로 넘겼으므로 종료
			}
		}

		// --- [DB 작업 생성] ---
		// =========================================================
		// [3] DB 데이터 비동기 로드 및 룸 입장 (첫 입장 시)
		// =========================================================
		SERVER::DBTask task;
		task.type = SERVER::DBTaskType::LOGIN_LOAD;
		task.session_id = session->_id;

		// 이 세션이 할당될 룸의 로직 스레드 인덱스를 저장 (콜백 반환용)
		session->_logic_thread_idx = target_room->GetLogicThreadIndex();
		task.logic_thread_idx = target_room->GetLogicThreadIndex();

		// [핵심] DB 스레드가 데이터를 채워 넣을 빈 상자(포인터)를 생성
		auto loaded_data = std::make_shared<std::any>();
		task.data = loaded_data;

		// [콜백] DB 스레드가 작업을 마치고 로직 스레드로 던져줄 함수
		task.callback = [session, target_room, enter_packet, loaded_data]() {

			// 1. DB 스레드가 상자에 채워둔 스냅샷 데이터를 꺼냄
			if (loaded_data->has_value()) {
				try {
					auto snapshot = std::any_cast<SERVER::InventorySnapshot>(*loaded_data);

					// 2. 플레이어 인벤토리에 데이터 세팅 (이 시점은 로직 스레드이므로 안전함)
					if (auto inven = session->_player->GetComponent<GAME::InventoryComponent>()) {

						for (const auto& [id, count] : snapshot.materials) {
							inven->add_material(id, count);
						}

						for (const auto& [uid, equip] : snapshot.equipments) {
							inven->add_equipment(equip);
						}

						// DB에서 갓 꺼내온 최신 상태이므로 dirty 플래그 초기화
						inven->mark_saved();
					}
				}
				catch (const std::bad_any_cast& e) {
					MYERROR("[DB] 로드 데이터 캐스팅 실패");
				}
			}

			// 3. 인벤토리 세팅이 완료되었으므로 최종적으로 방 입장 실행
			target_room->PushJob([target_room, session, enter_packet]() {
				target_room->Execute_C2S_ROOM_ENTER(session, enter_packet);
				});
		};
		SERVER::DBManager::Instance()->push_task(std::move(task));

		//// [CASE 2] 첫 입장이거나 이전 방이 없는 경우: 즉시 입장 요청
		//target_room->PushJob([target_room, session, enter_packet]()
		//	{
		//		target_room->Execute_C2S_ROOM_ENTER(session, enter_packet);
		//	});
	}

	void Handle_C2S_ROOM_LIST(const std::shared_ptr<SERVER::SESSION>& session, PacketStream& stream)
	{
		packet::CS_PACKET_ROOM_LIST recv_packet;
		try
		{
			stream >> recv_packet;
		}
		catch (const std::runtime_error& e)
		{
			MYERROR("[RoomList] **ERROR**: Failed to read room list packet from stream. " << e.what());
			return;
		}

		std::vector<RoomInfo> room_infos;
		for (int i = 0; i < 100; ++i)
		{
			SERVER::Room* room = SERVER::Server::Instance()->GetRoom(i);
			if (room)
			{
				RoomInfo info;
				info._room_id = room->GetRoomId();
				info._player_count = static_cast<uint8_t>(room->GetPlayerCount());
				room_infos.push_back(info);
			}
		}

		SC_PACKET_ROOM_LIST_ACK ack_packet;
		ack_packet._type = PacketType::S2C_P_ROOM_LIST_ACK;
		ack_packet._room_count = static_cast<uint16_t>(room_infos.size());
		ack_packet._size = sizeof(ack_packet) + (sizeof(RoomInfo) * ack_packet._room_count);

		PacketStream ack_stream;
		ack_stream << ack_packet;
		ack_stream.Write(reinterpret_cast<const char*>(room_infos.data()), sizeof(RoomInfo) * ack_packet._room_count);

		session->do_send(ack_stream.constable_data(), ack_stream.Size());
		MYLOG("Sent room list to session " << session->_id << ". Room count: " << ack_packet._room_count);
	}

	void Handle_C2S_CHAT_IN_ROOM(const std::shared_ptr<SERVER::SESSION>& session, packet::PacketStream& stream)
	{
		// 1. 채팅 메시지 읽기
		// PacketStream의 >> 연산자는 먼저 길이를 읽고, 그 길이만큼 문자열을 읽어옵니다.
		packet::CS_PACKET_CHAT_IN_ROOM recv_chat_packet;
		try
		{
			stream >> recv_chat_packet;
		}
		catch (...)
		{
			MYERROR("[CHAT] **ERROR**: Failed to read chat packet from stream.");
		}

		std::string message;
		try
		{
			stream >> message;
		}
		catch (...)
		{
			MYERROR("[CHAT] **ERROR**: Failed to read chat message from stream. ");
		}

		// 2. 세션이 방에 있는지 확인
		if (session->_state != SERVER::SESSION_STATE::ST_INGAME || session->_room_id == -1)
		{
			MYLOG("[CHAT] Session " << session->_id << " sent chat message from outside a room.");
			return;
		}

		// 3. 방 객체 가져오기
		SERVER::Room* room = SERVER::Server::Instance()->GetRoom(session->_room_id);
		if (room == nullptr) return;

		// 4. 방에 있는 모든 사람에게 채팅 메시지 브로드캐스팅
		packet::SC_PACKET_CHAT_IN_ROOM send_chat_packet;
		send_chat_packet._type = packet::PacketType::S2C_P_CHAT_IN_ROOM;
		send_chat_packet._sender_id = session->_id;

		packet::PacketStream broadcast_stream;
		broadcast_stream << send_chat_packet;
		broadcast_stream << message; // string을 스트림에 쓰면 길이(uint16_t)가 먼저 쓰이고 내용이 쓰임

		// 최종 패킷 크기를 헤더에 다시 설정
		// broadcast_stream의 맨 앞을 PacketHeader*로 캐스팅하여 size 멤버를 수정
		packet::PacketHeader* header_ptr = reinterpret_cast<packet::PacketHeader*>(broadcast_stream.mutable_data());
		header_ptr->_size = static_cast<uint16_t>(broadcast_stream.Size());

		room->Broadcast(broadcast_stream.constable_data(), broadcast_stream.Size());
		MYLOG("[CHAT] Room " << room->GetRoomId() << " | " << session->_id << ": " << message);
	}

	void Handle_C2S_ACTION(const std::shared_ptr<SERVER::SESSION>& session, PIP::packet::PacketStream& stream)
	{
		packet::CS_PACKET_ACTION action_packet;
		
		stream >> action_packet;
		
		

		SERVER::Room* room = SERVER::Server::Instance()->GetRoom(session->_room_id);
		if (room) {
			room->PushJob([session, action_packet, room]() {
				if (session->_state != SERVER::SESSION_STATE::ST_INGAME) return;
				// Room 클래스에 새로 만들 함수 호출
				room->Execute_C2S_ACTION(session, action_packet);
			});
		}
	}

	void Handle_C2S_PLAYER_READY(const std::shared_ptr<SERVER::SESSION>& session, PIP::packet::PacketStream& stream)
	{
		packet::CS_PACKET_PLAYER_READY ready_packet;
		stream >> ready_packet;
		SERVER::Room* room = SERVER::Server::Instance()->GetRoom(session->_room_id);
		if (room) {
			room->PushJob([session, ready_packet, room]() {
				if (session->_state != SERVER::SESSION_STATE::ST_INGAME) return;
				// Room 클래스에 새로 만들 함수 호출
				room->Execute_C2S_PLAYER_READY(session, ready_packet);
				});
		}
	}
}
