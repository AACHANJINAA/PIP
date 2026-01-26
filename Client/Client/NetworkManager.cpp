#include "stdafx.h"
#include "NetworkManager.h"
#include "GameFramework.h"
#include "LayerManager.h"
#include "MainPlayerScript.h"
#include "NPCScript.h"
#include "ObjectManager.h"
#include "OtherPlayerScript.h"
#include "Renderer.h"
#include "ResourceManager.h"

//#include "HPRenderComponent.h"
#include "AnimationComponent.h"
#include "MonsterHPComponent.h"

void error_display(const char* msg, int err_no)
{
	WCHAR* lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(GameFramework::instance()->hWnd(), lpMsgBuf, (LPCWSTR)msg, MB_OK);
	LocalFree(lpMsgBuf);
}

void NetworkManager::send_packet(const char* data, size_t size)
{
	// Blocking 소켓이므로 루프를 돌며 모두 전송될 때까지 대기
	size_t total_sent = 0;
	while (total_sent < size)
	{
		int sent = send(_socket, data + total_sent, static_cast<int>(size - total_sent), 0);

		if (sent == SOCKET_ERROR) {
			error_display("send", WSAGetLastError());
			_isRunning = false; // 치명적 오류 시 루프 종료
			return;
		}
		total_sent += sent;
	}
}

void NetworkManager::process_queued_packets()
{
	// 이 함수는 '메인 스레드'의 게임 루프에서 호출됩니다.
	std::vector<char> packetData;
	while (_packetQueue.try_pop(packetData))
	{
		common::packet::PacketStream stream(packetData.data(), packetData.size());
		auto* header = reinterpret_cast<common::packet::PacketHeader*>(packetData.data());

		auto it = _handlers.find(header->_type);
		if (it != _handlers.end()) {
			it->second(stream); // 실제 게임 로직(MainPlayer 이동 등) 실행
		}
	}
}

void NetworkManager::network_worker()
{
	//CLOG("Network Worker Thread Started");

	while (_isRunning)
	{
		// 안전 장치: 소켓이 유효하지 않으면 즉시 종료                        
		if (_socket == INVALID_SOCKET) {
			CERROR("Network worker: Socket is invalid. Terminating thread.");
			_isRunning = false;
			break;
		}

		// Blocking RECV 호출
		recv_packet();
	}

	//CLOG("Network Worker Thread Ended");
}


void NetworkManager::recv_packet()
{
	char buf[4096];
	// 여기서 데이터가 올 때까지 스레드가 대기합니다 (Blocking)
	int len = recv(_socket, buf, sizeof(buf), 0);

	if (len == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK) {
			// 실제 에러 or 연결 끊김
			error_display("recv", err);
			_isRunning = false;
		}
		return;
	}

	if (len == 0) {
		// 정상적인 연결 종료 (Graceful Close)
		_isRunning = false;
		CLOG("Server closed the connection.");
		return;
	}

	// 데이터 수신 성공
	if (len > 0) {
		_recvBuffer.insert(_recvBuffer.end(), buf, buf + len);

		// 패킷 조립 (Framing)
		while (true)
		{
			if (_recvBuffer.size() < sizeof(common::packet::PacketHeader))
				break; // 헤더조차 다 못 받음

			auto* header = reinterpret_cast<common::packet::PacketHeader*>(_recvBuffer.data());

			if (_recvBuffer.size() < header->_size)
				break; // 패킷 바디가 다 안 옴

			// 완성된 패킷 하나 추출
			std::vector<char> singlePacket(_recvBuffer.begin(), _recvBuffer.begin() + header->_size);

			// 메인 스레드가 처리하도록 큐에 전달
			_packetQueue.push(std::move(singlePacket));

			// 버퍼에서 제거
			_recvBuffer.erase(_recvBuffer.begin(), _recvBuffer.begin() + header->_size);
		}
	}
}



void NetworkManager::SendLoginPacket(const std::string& name)
{
	common::packet::PacketStream stream;
	common::packet::CS_PACKET_LOGIN login_packet;
	login_packet._type = common::packet::PacketType::C2S_P_LOGIN;

	stream << login_packet;
	stream << name;   // PacketStream이 알아서 [길이][내용]을 써 줌
	_name = name;
	// 스트림에 모든 데이터를 쓴 후, 실제 크기를 계산하여 헤더에 덮어쓴다.
	auto* final_header = reinterpret_cast<common::packet::PacketHeader*>(stream.mutable_data());
	final_header->_size = static_cast<uint16_t>(stream.Size());

	send_packet(stream.mutable_data(), stream.Size());
}

void NetworkManager::SendMovePacket(common::Vec3 position, common::Quat rotation, common::packet::OBJECT_STATE state)
{
	// 페이로드가 있는 고정 크기 패킷은 구조체를 바로 사용하는 것이 편리합니다.
	common::packet::CS_PACKET_MOVE packet;
	packet._type = common::packet::PacketType::C2S_P_MOVE;
	packet._size = sizeof(packet);
	packet._position = position;
	packet._rotation = rotation;
	packet._state = state;
	// 구조체 자체를 보내도 되지만, 일관성을 위해 PacketStream을 사용할 수 있습니다.
	// 여기서는 구조체를 바로 보내는 더 간단한 방식을 유지합니다.
	send_packet(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void NetworkManager::SendAttackPacket()
{
	common::packet::CS_PACKET_ATTACK packet;
	//packet._type = common::packet::PacketType::C2S_P_ATTACK;
	packet._size = sizeof(packet);

	send_packet(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void NetworkManager::SendActionPacket(common::packet::ActionType type, int32_t actionID, int64_t targetID,
	common::Vec3 pos, common::Quat dir)
{
	common::packet::CS_PACKET_ACTION packet;
	packet._type = common::packet::PacketType::C2S_P_ACTION;
	packet._size = sizeof(packet);
	packet._action_type = type;
	packet._action_id = actionID;
	packet._target_id = targetID;
	packet._position = pos;
	packet._direction = dir;

	send_packet(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void NetworkManager::SendRoomListPacket()
{
	common::packet::CS_PACKET_ROOM_LIST packet;
	packet._type = common::packet::PacketType::C2S_P_ROOM_LIST;
	packet._size = sizeof(packet);
	send_packet(reinterpret_cast<const char*>(&packet), sizeof(packet));
}
void NetworkManager::SendEnterRoomPacket(int room_id_to_enter)
{
	common::packet::CS_PACKET_ENTER_ROOM packet;
	packet._type = common::packet::PacketType::C2S_P_ENTER_ROOM;
	packet._size = sizeof(packet);
	packet._room_id = room_id_to_enter;

	send_packet(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void NetworkManager::RegisterHandler(common::packet::PacketType packet_type, PacketHandler packet_handler)
{
	// 내부적으로 핸들러 등록하기 위해서 사용
	_handlers[packet_type] = packet_handler;
}

void NetworkManager::HANDLE_S2C_LOGIN_ACK(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_LOGIN_ACK ack_packet;
	stream >> ack_packet;

	if (ack_packet._success)
	{
		_my_session_id = ack_packet._my_session_id; // [핵심] 자신의 ID 저장
		CLOG("[S->C] Login successful! My Session ID is now: " << _my_session_id);
	}
	else
	{
		CLOG("[S->C] Login failed!");
		MessageBox(GameFramework::instance()->hWnd(), L"Login failed.", L"Login Error", MB_OK);
	}
}

void NetworkManager::HANDLE_S2C_SPAWN_PLAYER(common::packet::PacketStream& stream)
{
	// [수정] SC_PACKET_SPAWN_PLAYER 구조체 변수를 선언하고 스트림에서 읽어옵니다.
	common::packet::SC_PACKET_SPAWN_PLAYER spawn_data;
	stream >> spawn_data; // 구조체 전체를 읽습니다.

	// [추가] 이름(가변 길이)을 스트림에서 읽어옵니다.
	std::string name;
	stream >> name;
	if (name != _name)
	{
		_name = name;// 서버에서 보내준 이름으로 업데이트
		CLOG(" [S->C] Updated player name from server: " << _name << " 아마 에러임");
	}
	// 이제 spawn_data 구조체와 name 변수에 올바른 값이 들어있습니다.
	// 이 값들을 사용하여 플레이어 객체를 생성하거나 업데이트합니다.


	// 패킷의 ID가 내 플레이어 ID와 같은지 확인합니다.
	if (spawn_data._id == _my_session_id)
	{
		CLOG("[SPAWN_PLAYER] ID MATCH! Creating MY player (MainPlayer).");
		// 내 플레이어 정보 업데이트
		{
			auto playerObject = ObjectManager::instance()->create_game_object("MainPlayer");
			// MainPlayerScript추가
			playerObject->set_layer("Player");

			auto player_logic = playerObject->add_component<MainPlayerScript>();
			player_logic->set_name(name);
			player_logic->set_hp(spawn_data._hp);
			player_logic->set_id(_my_session_id);
			player_logic->set_position(spawn_data._position);
			player_logic->transform()->set_local_rotation(spawn_data._rotation);
			
			// Animationcomponent
			auto animation_component = playerObject->add_component<AnimationComponent>();
			
			
			// RenderComponent
			auto renderer = playerObject->add_component<RenderComponent>();

			auto playerMesh = ResourceManager::instance()->load_mesh("Resource/Character/Brute_Walk/Brute_Walk.gltf", true, "walk");
			renderer->set_mesh(playerMesh);

			auto idleMesh = 
				ResourceManager::instance()->load_mesh("Resource/Character/Brute_idle/Brute_idle.gltf", true,"idle");
			auto walkMesh = 
				ResourceManager::instance()->load_mesh("Resource/Character/Brute_Walk/Brute_Walk.gltf", true,"walk");

			animation_component->add_state_mapping(common::packet::OBJECT_STATE::IDLE, "idle", idleMesh);
			animation_component->add_state_mapping(common::packet::OBJECT_STATE::WALK, "walk", walkMesh);
			animation_component->add_state_mapping(common::packet::OBJECT_STATE::ATTACK, "attack", idleMesh);
			
			// 초기 상태 설정 (강제로 적용하여 메쉬/애니메이션 로드)
			animation_component->set_state(common::packet::OBJECT_STATE::WALK); // 잠시 WALK로 바꿨다가
			animation_component->set_state(common::packet::OBJECT_STATE::IDLE); // IDLE로 설정하면 로직이 돕니다.

			// ResourceManager을 통해 재질 생성 및 쉐이더 할당
			std::string material_name = "player_material"; // player는 고정된 재질
			ResourceManager::instance()->create_material(material_name);
			ResourceManager::instance()->set_shader_for_material(material_name, "skinned");

			// gltf
			renderer->set_pso_name("skinned");

			// 위치, 회전 정보
			//playerObject->transform()->set_local_rotation(-90.f, 0.f, 0.f);
			//playerObject->transform()->set_local_rotation(-90.f, 0.f, 0.f);
			//playerObject->transform()->set_local_scale({ 200.0f, 200.0f, 200.0f });
			playerObject->transform()->set_local_scale({ 2.0f, 2.0f, 2.0f });
		}
		

		// 매니저에 넣기
		
		// level, exp 등 추가 정보도 업데이트 가능
		
		CLOG("[S->C] My player spawned/updated: ID=" << spawn_data._id
			<< "Pos=" << spawn_data._position.x << "," << spawn_data._position.y
			<<"HP=" << spawn_data._hp 
			<<"Level=" << spawn_data._level
			<<"Exp=" << spawn_data._exp
			<< "Name=" << name.c_str());
	}
	else
	{
		CLOG("[OtherPlayer] ID MISMATCH! Creating OTHER player (OtherPlayer).");
		// 다른 플레이어 (적) 생성 또는 업데이트
		auto other_player = ObjectManager::instance()->create_game_object(name);
		auto other_player_logic = other_player->add_component<OtherPlayerScript>();
		other_player->transform()->set_local_position(spawn_data._position);
		other_player->transform()->set_local_rotation(spawn_data._rotation);
		other_player->set_layer("OtherPlayer");

		other_player_logic->set_hp(spawn_data._hp);
		other_player_logic->set_id(spawn_data._id);
		
		CLOG("[S->C] OtherPlayer spawned/updated: ID=" << spawn_data._id
			<< "Pos=" << spawn_data._position.x << "," << spawn_data._position.y
			<< "HP=" << spawn_data._hp
			<< "Level=" << spawn_data._level
			<< "Exp=" << spawn_data._exp
			<< "Name=" << name.c_str());
	}
}
void NetworkManager::HANDLE_S2C_MOVE(common::packet::PacketStream& stream)
{
	// SC_PACKET_MOVE는 헤더 외에 여러 멤버를 가집니다.
	common::packet::SC_PACKET_MOVE move_packet;
	stream >> move_packet; // 구조체 전체를 읽습니다.
	auto player = ObjectManager::instance()->find_by_name("MainPlayer");
	auto player_Transform = player->get_component<TransformComponent>();
	if (player && move_packet._id == _my_session_id && player_Transform) // 읽어온 id 사용
	{
		common::Vec3 currentPos = player_Transform->local_position();
		common::Vec3 serverPos = move_packet._position;

		// 현재 위치와 서버가 보낸 위치 사이의 거리(제곱) 계산
		float distSq = pow(currentPos.x - serverPos.x, 2) +
			pow(currentPos.y - serverPos.y, 2) +
			pow(currentPos.z - serverPos.z, 2);

		// [핵심] 오차가 허용 범위(예: 2.0f)보다 클 때만 위치를 강제 보정합니다.
		// 네트워크 지연으로 인한 미세한 차이는 무시하여 부드러운 움직임을 유지합니다.
		// 만약 벽을 뚫거나 심각한 위치 불일치가 발생하면(거리가 멀면) 그때 강제로 텔레포트 시킵니다.
		const float TOLERANCE_SQ = 2.0f * 2.0f; // 2.0 단위 거리 (필요에 따라 조절)

		if (distSq > TOLERANCE_SQ)
		{
			player_Transform->set_local_position(serverPos);
			// 디버깅용 로그: 큰 오차가 발생했을 때만 출력
			CLOG("Server Correction Applied! Distance: " << sqrt(distSq));
		}
	}
	else
	{
		auto enemy_layer = LayerManager::instance()->get_layer_value("OtherPlayer");
		auto other_players = ObjectManager::instance()->find_by_layer(enemy_layer);
		auto it = std::find_if(other_players.begin(), other_players.end(), 
			[&](const std::shared_ptr<GameObject>& other) 
			{
				auto other_script = other->get_component<OtherPlayerScript>();
				return other_script && move_packet._id == other_script->id();
			});
		if (it != other_players.end())
		{
			auto other_player_script = (*it)->get_component<OtherPlayerScript>();
			other_player_script->on_sync_position(move_packet._position);
			other_player_script->on_sync_rotation(move_packet._rotation);
			other_player_script->on_sync_state(move_packet._state);
		}
	}
}
void NetworkManager::HANDLE_S2C_LEAVE(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_LEAVE leave_packet;
	stream >> leave_packet; // id만 읽습니다.
	auto enemy_layer = LayerManager::instance()->get_layer_value("OtherPlayer");
	auto other_players = ObjectManager::instance()->find_by_layer(enemy_layer);
	auto it = std::ranges::find_if(other_players, [&](const std::shared_ptr<GameObject>& other) {
		auto other_script = other->get_component<OtherPlayerScript>();
		return other_script && leave_packet._id == other_script->id();
	});
	if (it != other_players.end())
	{
		(*it)->destroy();
	}
}
void NetworkManager::HANDLE_S2C_PLAYER_ATTACK(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_PLAYER_ATTACK attack_header;
	stream >> attack_header;

	for (uint8_t i = 0; i < attack_header._hit_count; ++i)
	{
		common::packet::PlayerHitInfo hit_info;
		stream >> hit_info;

		// 내 플레이어인지 확인
		if (_my_session_id == hit_info._target_id)
		{
			auto player = ObjectManager::instance()->find_by_name("MainPlayer");
			if (player)
			{
				auto player_logic = player->get_component<MainPlayerScript>();
				if (player_logic && hit_info._target_id == player_logic->id())
				{
					player_logic->set_hp(hit_info._target_current_hp);
					CLOG("나의 플레이어가 맞고 체력이 바뀌었다. SC_PACKET_PLAYER_ATTACK 패킷 처리");
					continue; // 다음 피격 정보로
				}
			}
		}

		// 다른 플레이어인지 확인
		auto enemy_layer = LayerManager::instance()->get_layer_value("OtherPlayer");
		auto other_players = ObjectManager::instance()->find_by_layer(enemy_layer);
		auto it = std::ranges::find_if(other_players, 
			[&](const std::shared_ptr<GameObject>& other) 
			{
				auto other_script = other->get_component<OtherPlayerScript>();
				return other_script && hit_info._target_id == other_script->id();
			});
		if (it != other_players.end())
		{
			(*it)->get_component<OtherPlayerScript>()->set_hp(hit_info._target_current_hp);
			CLOG("다른 플레이어가 맞고 체력이 바뀌었다. SC_PACKET_PLAYER_ATTACK 패킷 처리");
		}
	}
}

void NetworkManager::HANDLE_S2C_NPC_ATTACK(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_NPC_ATTACK attack_header;
	stream >> attack_header;

	for (uint8_t i = 0; i < attack_header._hit_count; ++i)
	{
		common::packet::NPCHitInfo hit_info;
		stream >> hit_info;

		// NPC 찾기 (ID로 찾아야 함)
		auto enemy_layer = LayerManager::instance()->get_layer_value("Enemy");
		auto npcs = ObjectManager::instance()->find_by_layer(enemy_layer);
		auto it = std::ranges::find_if(npcs, 
			[&](const std::shared_ptr<GameObject>& npc)
			{
				auto npc_script = npc->get_component<NPCScript>();
				return npc_script && hit_info._target_id == npc_script->id();
			});
		if (it != npcs.end())
		{
			(*it)->get_component<NPCScript>()->set_hp(hit_info._target_current_hp);
		}
	}
}
void NetworkManager::HANDLE_S2C_ROOM_LIST_ACK(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_ROOM_LIST_ACK room_list_ack;
	stream >> room_list_ack; // room_count만 읽습니다.

	//CLOG(L"[S->C] Received room list! Room count: %hu", room_list_ack._room_count);

	for (int i = 0; i < room_list_ack._room_count; ++i) // 읽어온 room_count 사용
	{
		common::packet::RoomInfo room_info;
		stream >> room_info; // RoomInfo 구조체 하나를 읽습니다.
		//CLOG(L"  - Room ID: %d, Players: %u", room_info._room_id, static_cast<unsigned int>(room_info._player_count));
	}
}
void NetworkManager::HANDLE_S2C_ENTER_ROOM_ACK(common::packet::PacketStream& stream)
{
	extern HWND g_hwnd;

	common::packet::SC_PACKET_ENTER_ROOM_ACK ack_packet;

	stream >> ack_packet; // SC_PACKET_ENTER_ROOM_ACK 구조체를 읽습니다.

	if (ack_packet._success) // 읽어온 success 사용
	{
		//CLOG(L"[S->C] Successfully entered room %d!", ack_packet._room_id); // 읽어온 room_id 사용
		//gGameFramework.ChangeScene(GameFramework::SceneType::InGame); // 게임 씬으로 전환
	}
	else
	{
		MessageBox(g_hwnd, L"Failed to enter room.", L"Room Entry Error", MB_OK);
	}
}
void NetworkManager::HANDLE_S2C_SPAWN_NPC(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_NPC_SPAWN npc_spawn_packet;
	stream >> npc_spawn_packet;

	std::string npc_name;
	stream >> npc_name;

	// [수정] 이미 존재하는지 확인 (AOI 재진입 대응)
	auto existingNPC = ObjectManager::instance()->find_npc(npc_spawn_packet._npc_id);
	if (existingNPC)
	{
		// 존재하면 위치만 강제 동기화
		auto script = existingNPC->get_component<NPCScript>();
		if (script) {
			script->initialize_from_server(npc_spawn_packet._position);
		}
		return;
	}

	if (npc_spawn_packet._npc_type == 1)
	{
		//CLOG("[SPAWN_NPC]");
		auto NPC = ObjectManager::instance()->create_game_object(npc_name);
		auto NPC_logic = NPC->add_component<NPCScript>();
		auto NPC_HP = NPC->add_component<MonsterHPComponent>();

		ObjectManager::instance()->register_npc(npc_spawn_packet._npc_id, NPC);

		NPC_logic->set_id(npc_spawn_packet._npc_id);
		NPC_logic->set_hp(npc_spawn_packet._hp);
		NPC_logic->set_position(npc_spawn_packet._position);
		NPC->set_layer("Enemy");


		auto npc_mesh = ResourceManager::instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");

		auto render_comp = NPC->add_component<RenderComponent>();
		render_comp->set_mesh(npc_mesh);

		// ResourceManager을 통해 재질 생성 및 쉐이더 할당
		std::string material_name = "npc_material"; // player는 고정된 재질
		ResourceManager::instance()->create_material(material_name);
		ResourceManager::instance()->set_shader_for_material(material_name, "gltf");

		// gltf
		render_comp->set_pso_name("gltf");

		/*CLOG("[S->C] Spawned NPC ID: " << npc_spawn_packet._npc_id
			<< " Name: " << npc_name
			<< " Type: " << npc_spawn_packet._npc_type
			<< " Position: " << npc_spawn_packet._position.x << "," << npc_spawn_packet._position.y);*/
	}
}
void NetworkManager::HANDLE_S2C_MOVE_NPC(common::packet::PacketStream& stream)
{
	// [수정] 이름까지 읽도록 전체 로직 수정
	common::packet::SC_PACKET_NPC_MOVE move_packet;
	

	std::string npc_name;
	try
	{
		stream >> move_packet;
		stream >> npc_name;
	}
	catch (const std::runtime_error& e)
	{
		CERROR("npc 이동 패킷 읽기 오류" << e.what());
	}
	

	// 받은 이름으로 게임 오브젝트를 찾아서 위치를 업데이트합니다.
	// ObjectManager에 이름으로 오브젝트를 찾는 기능(find_object)이 있다고 가정합니다.
	auto npc_object = ObjectManager::instance()->find_npc(move_packet._npc_id);
	if (npc_object)
	{
		auto script = npc_object->get_component<NPCScript>();
		if (script)
		{
			script->on_server_update(move_packet._position, move_packet._velocity, move_packet._rotation, move_packet._time_stamp);
		}
		else
		{
			// 디버깅을 위한 로그
			CLOG("[S->C] Move NPC Error: NPCScript not found on object: " << npc_name);
			npc_object->transform()->set_local_position(move_packet._position);
		}
	}
	else
	{
		// 디버깅을 위한 로그
		CLOG("[S->C] Move NPC Error: Object not found with name: " << npc_name);
	}
}

void NetworkManager::HANDLE_S2C_MOVE_NPC_BATCH(common::packet::PacketStream& stream)
{
	// 1. 헤더 읽기
	common::packet::SC_PACKET_NPC_MOVE_BATCH header;
	stream >> header;

	// 2. 개수만큼 반복하며 데이터 읽기
	for (int i = 0; i < header._count; ++i)
	{
		common::packet::NPCMoveData data;
		stream >> data;

		auto npc_object = ObjectManager::instance()->find_npc(data._npc_id);
		if (npc_object)
		{
			auto script = npc_object->get_component<NPCScript>();
			if (script) {
				// 아까 만든 on_server_update 호출 (타임스탬프 포함)
				script->on_server_update(data._position, data._velocity, data._rotation, data._time_stamp);

				// 상태(애니메이션) 업데이트가 필요하다면 추가
				script->set_state(data._state);
			}
		}
	}
}

void NetworkManager::HANDLE_S2C_DESPAWN_NPC(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_NPC_DESPAWN packet;
	stream >> packet;

	// NPC 찾아서 삭제
	auto npc = ObjectManager::instance()->find_npc(packet._npc_id);
	if (npc)
	{
		ObjectManager::instance()->unregister_npc(packet._npc_id);
		npc->destroy();
		// CLOG("[S->C] NPC Despawned (AOI): " << packet._npc_id);
	}
}

bool NetworkManager::init_network()
{
	// 이동 응답 패킷 핸들러 등록
	RegisterHandler(common::packet::PacketType::S2C_P_MOVE,
		std::bind(&NetworkManager::HANDLE_S2C_MOVE, this, std::placeholders::_1));

	// 퇴장 응답 패킷 핸들러 등록
	RegisterHandler(common::packet::PacketType::S2C_P_LEAVE,
		std::bind(&NetworkManager::HANDLE_S2C_LEAVE, this, std::placeholders::_1));

	// 공격 응답 패킷 핸들러 등록
	RegisterHandler(common::packet::PacketType::S2C_P_PLAYER_ATTACK,
		std::bind(&NetworkManager::HANDLE_S2C_PLAYER_ATTACK, this, std::placeholders::_1));
	RegisterHandler(common::packet::PacketType::S2C_P_NPC_ATTACK,
		std::bind(&NetworkManager::HANDLE_S2C_NPC_ATTACK, this, std::placeholders::_1));

	// 방 목록 응답 패킷 핸들러 등록
	RegisterHandler(common::packet::PacketType::S2C_P_ROOM_LIST_ACK,
		std::bind(&NetworkManager::HANDLE_S2C_ROOM_LIST_ACK, this, std::placeholders::_1));

	// 방 입장 응답 패킷 핸들러 등록
	RegisterHandler(common::packet::PacketType::S2C_P_ENTER_ROOM_ACK,
		std::bind(&NetworkManager::HANDLE_S2C_ENTER_ROOM_ACK, this, std::placeholders::_1));

	// 플레이어 스폰 패킷 핸들러 등록
	RegisterHandler(common::packet::PacketType::S2C_P_SPAWN_PLAYER,
		std::bind(&NetworkManager::HANDLE_S2C_SPAWN_PLAYER, this, std::placeholders::_1));

	// 로그인 응답 패킷 핸들러 등록
	RegisterHandler(common::packet::PacketType::S2C_P_LOGIN_ACK,
		std::bind(&NetworkManager::HANDLE_S2C_LOGIN_ACK, this, std::placeholders::_1));

	// NPC 스폰 패킷 핸들러 등록
	RegisterHandler(common::packet::PacketType::S2C_NPC_SPAWN,
		std::bind(&NetworkManager::HANDLE_S2C_SPAWN_NPC, this, std::placeholders::_1));
	// NPC 이동 패킷 핸들러 등록
	RegisterHandler(common::packet::PacketType::S2C_NPC_MOVE,
		std::bind(&NetworkManager::HANDLE_S2C_MOVE_NPC, this, std::placeholders::_1));
	// NPC 이동 Batch 패킷 핸들러 등록
	RegisterHandler(common::packet::PacketType::S2C_NPC_MOVE_BATCH,
		std::bind(&NetworkManager::HANDLE_S2C_MOVE_NPC_BATCH, this, std::placeholders::_1));

	RegisterHandler(common::packet::PacketType::S2C_NPC_DESPAWN, 
		std::bind(&NetworkManager::HANDLE_S2C_DESPAWN_NPC, this, std::placeholders::_1));


	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		error_display("WSAStartup failed", result);
		return false;
	}
	return true;
}
void NetworkManager::cleanup_network()
{
	_isRunning = false;

	// 1. 소켓 연결 종료 (블락킹 된 recv를 깨움)
	if (_socket != INVALID_SOCKET) {
		// SD_BOTH: 송신 및 수신을 모두 중단.
		// 이렇게 하면 대기 중인 recv가 0이나 SOCKET_ERROR를 반환하며 즉시 리턴됩니다.
		shutdown(_socket, SD_BOTH);
		closesocket(_socket);
		_socket = INVALID_SOCKET;
	}

	// 2. 네트워크 스레드 종료 대기
	if (_networkThread.joinable()) {
		_networkThread.join();
	}

	// 3. Winsock 정리
	WSACleanup();
}
bool NetworkManager::connect_to_server(std::string_view server_addr, const int& port)
{
	_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_socket == INVALID_SOCKET)
	{
		error_display("socket", WSAGetLastError());
		return false;
	}

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, server_addr.data(), &addr.sin_addr);

	if (connect(_socket, (SOCKADDR*)(&addr), sizeof(addr)) == SOCKET_ERROR) {
		// connect 에러 처리 (WSAEWOULDBLOCK은 정상)
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			error_display("connect", WSAGetLastError());
			disconnect();
			return false;
		}
	}
	// 네이글 끄는 코드
	int nodelay = 1;
	setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

	_isRunning = true;
	_networkThread = std::thread(&NetworkManager::network_worker, this);
	return true;
}
void NetworkManager::disconnect()
{
	_isRunning = false;

	if (_socket != INVALID_SOCKET)
	{
		shutdown(_socket, SD_BOTH); // 송수신 중단
		closesocket(_socket);
		_socket = INVALID_SOCKET;
	}

	// 필요하다면 PostQuitMessage(0);
}
