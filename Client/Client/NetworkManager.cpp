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
#include "ReplicationSystem.h"

//#include "HPRenderComponent.h"
#include "AnimationComponent.h"
#include "DebugDrawManager.h"
#include "UIRenderComponent.h"
#include "MonsterHPComponent.h"
#include "TainerScript.h"
#include "UIFrameRenderComponent.h"

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

	// 2. [핵심] 메인 윈도우에 종료 메시지 전송
// PostMessage를 사용하면 다른 스레드에서도 메인 스레드의 메시지 큐에 이벤트를 넣을 수 있습니다.
	HWND hWnd = GameFramework::instance()->hWnd();
	if (hWnd) {
		::PostMessage(hWnd, WM_CLOSE, 0, 0);
	}
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
	//// 메인 스레드에서 패킷 큐를 비우는 전체 시간 측정
	//auto start = std::chrono::high_resolution_clock::now();
	//// 타입별 누적 시간을 저장할 맵
	//static std::unordered_map<uint16_t, long long> typeAccumTime;
	//typeAccumTime.clear();

	// 이 함수는 '메인 스레드'의 게임 루프에서 호출됩니다.
	std::vector<char> packetData;
	while (_packetQueue.try_pop(packetData))
	{
		common::packet::PacketStream stream(packetData.data(), packetData.size());
		auto* header = reinterpret_cast<common::packet::PacketHeader*>(packetData.data());
		//auto pStart = std::chrono::high_resolution_clock::now();

		auto it = _handlers.find(header->_type);
		if (it != _handlers.end()) {
			it->second(stream); // 실제 게임 로직(MainPlayer 이동 등) 실행
		}
		//auto pEnd = std::chrono::high_resolution_clock::now();
		//typeAccumTime[(uint16_t)header->_type] += std::chrono::duration_cast<std::chrono::microseconds>(pEnd - pStart).count();
	}

	//// 여기서 어떤 타입이 가장 오래 걸렸는지 로그 출력
	//for (auto& pair : typeAccumTime) {
	//	if (pair.second > 500) // 0.5ms 이상 먹은 놈만 출력
	//		CLOG("Packet Type " << pair.first << " took " << pair.second << "us");
	//}
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
		// [해결] 서버 종료 시 알림 후 프로그램 종료
		MessageBox(GameFramework::instance()->hWnd(), L"Server connection closed.", L"Network Info", MB_OK);
		return;
	}

	// 데이터 수신 성공
	if (len > 0) {
		_recvBuffer.insert(_recvBuffer.end(), buf, buf + len);

		// 패킷 조립 (Framing)
		while (true)
		{
			if (_recvBuffer.size() < sizeof(common::packet::PacketHeader)) break;
			auto* header = reinterpret_cast<common::packet::PacketHeader*>(_recvBuffer.data());

			// [수정] 헤더 사이즈가 비정상적(0이거나 너무 작음)이면 스트림이 깨진 것임
			if (header->_size < sizeof(common::packet::PacketHeader)) {
				CLOG("Critical: Invalid Packet Size " << header->_size);
				_recvBuffer.clear(); // 스트림 초기화
				break;
			}

			if (_recvBuffer.size() < header->_size) break;

			std::vector<char> singlePacket(_recvBuffer.begin(), _recvBuffer.begin() + header->_size);
			_packetQueue.push(std::move(singlePacket));
			_recvBuffer.erase(_recvBuffer.begin(), _recvBuffer.begin() + header->_size);
		}
	}
}



void NetworkManager::SendLoginPacket()
{
	if (!_isLogin)
	{
		common::packet::PacketStream stream;
		common::packet::CS_PACKET_LOGIN login_packet;
		login_packet._type = common::packet::PacketType::C2S_P_LOGIN;

		stream << login_packet;
		stream << _name;   // PacketStream이 알아서 [길이][내용]을 써 줌
		// 스트림에 모든 데이터를 쓴 후, 실제 크기를 계산하여 헤더에 덮어쓴다.
		auto* final_header = reinterpret_cast<common::packet::PacketHeader*>(stream.mutable_data());
		final_header->_size = static_cast<uint16_t>(stream.Size());

		send_packet(stream.mutable_data(), stream.Size());
	}
}

void NetworkManager::SendMovePacket(const common::Vec3& position, const common::Vec3& dir, 
	const common::Quat& rotation, const common::packet::EntityState& state, const int32_t& action_id, const uint32_t& current_tick)
{
	// 페이로드가 있는 고정 크기 패킷은 구조체를 바로 사용하는 것이 편리합니다.
	common::packet::CS_PACKET_MOVE packet;
	packet._type = common::packet::PacketType::C2S_P_MOVE;
	packet._size = sizeof(packet);
	packet._position	= position;
	packet._move_dir	= dir;
	packet._rotation	= rotation;
	packet._state		= state;
	packet._action_id	= action_id;
	packet._client_tick = current_tick;
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
		_isLogin = true;
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
			auto hp_bar = ObjectManager::instance()->find_by_name("HP_Bar");
			if (hp_bar)
			{
				auto hp_ui = hp_bar->get_component<UIRenderComponent>();
				player_logic->set_hp_bar_ui(hp_ui);
			}
			player_logic->set_hp(spawn_data._hp);
			player_logic->set_id(_my_session_id);
			auto hp_frame_obj = ObjectManager::instance()->find_by_name("HP_Frame");
			if (hp_frame_obj) {
				auto hp_frame_ui = hp_frame_obj->get_component<UIFrameRenderComponent>();
				if (hp_frame_ui) {
					hp_frame_ui->set_other_player_id(_my_session_id);
				}
			}
			player_logic->set_position(spawn_data._position);
			player_logic->transform()->set_local_rotation(spawn_data._rotation);
		}
		

		
		
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

		// 2. [추가] 스크립트 내부 논리 좌표 초기화
		// on_sync_position을 호출하여 _logicalPosition을 서버 좌표로 맞춰줍니다.
		other_player_logic->on_sync_position(spawn_data._position);
		other_player_logic->on_sync_rotation(spawn_data._rotation);

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
	if (move_packet._id == _my_session_id) // 읽어온 id 사용
	{
		auto player = ObjectManager::instance()->find_by_name("MainPlayer");
		if (player) {
			auto script = player->get_component<MainPlayerScript>();
			if (script) {
				script->sync_with_server(move_packet);
			}
		}
		//CLOG("[S->C] Syncing MY player position. Server Pos=" << move_packet._position.x << "," << move_packet._position.y
		//	<< " My Pos=" << (player ? player->transform()->local_position().x : 0) << "," << (player ? player->transform()->local_position().y : 0));
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
	CLOG("[S->C] Received PLAYER_ATTACK. Attacker: " << attack_header._attacker_id << " HitCount: " <<
		(int)attack_header._hit_count);
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
					// [로그 추가] 패킷이 올 때마다 현재 HP와 새 HP를 출력
					CLOG("[Network] Hit Packet Received! Current Logic HP: " << player_logic->hp() 
						<< " -> New HP: " << hit_info._target_current_hp);
					player_logic->set_hp(hit_info._target_current_hp);
					player_logic->set_position(hit_info._target_position);
					//player_logic->apply_knockback(hit_info._knockback_vector);

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
void NetworkManager::HANDLE_S2C_NPC_COUNT(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_SCENE_AWAKE spawn_npc_count; // npc 카운트 읽기
	stream >> spawn_npc_count; // room_count만 읽습니다.
	int64_t npc_count = spawn_npc_count._npc_count;
	int64_t npc_start_id = spawn_npc_count._npc_start_id;
	int64_t boss_count = spawn_npc_count._boss_count;
	int64_t boss_start_id = spawn_npc_count._boss_start_id;
	
	// npc pool spawn
	for (int64_t i = 0; i < npc_count; ++i)
	{
		int64_t npc_id = npc_start_id + i;
		auto NPC = ObjectManager::instance()->create_game_object("npc_pool" + std::to_string(npc_id));
		NPC->set_layer("Enemy");

		// [핵심] 렌더링 및 애니메이션에 필요한 컴포넌트들을 먼저 추가해줘야 합니다!
		NPC->add_component<MonsterHPComponent>();
		NPC->add_component<AnimationComponent>();
		NPC->add_component<RenderComponent>();


		NPCScript* NPC_logic = nullptr;

		NPC_logic = NPC->add_component<NPCScript>().get();

		auto rs = GameFramework::instance()->get_replication_system();
		if (rs) rs->register_entity(npc_id, NPC_logic);

		ObjectManager::instance()->register_npc(npc_id, NPC);
	}

	// boss pool spawn
	for (int64_t i = 0; i < boss_count; ++i)
	{
		int64_t boss_id = boss_start_id + i;
		auto Boss = ObjectManager::instance()->create_game_object("boss_pool" + std::to_string(boss_id));
		Boss->set_layer("Enemy");

		// [핵심] 렌더링 및 애니메이션에 필요한 컴포넌트들을 먼저 추가해줘야 합니다!
		Boss->add_component<MonsterHPComponent>();
		Boss->add_component<AnimationComponent>();
		Boss->add_component<RenderComponent>();


		NPCScript* NPC_logic = nullptr;

		NPC_logic = Boss->add_component<TainerScript>().get();

		auto rs = GameFramework::instance()->get_replication_system();
		if (rs) rs->register_entity(boss_id, NPC_logic);

		ObjectManager::instance()->register_npc(boss_id, Boss);
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
		// 존재하면 위치만 강제 동기화 및 화면에 보이게 하기
		auto script = existingNPC->get_component<NPCScript>();
		existingNPC->get_component<RenderComponent>()->set_enabled(true);
		if (script) {
			script->initialize_from_server(npc_spawn_packet);
		}
		return;
	}

	auto NPC = ObjectManager::instance()->create_game_object(npc_name);
	NPC->set_layer("Enemy");

	// [핵심] 렌더링 및 애니메이션에 필요한 컴포넌트들을 먼저 추가해줘야 합니다!
	NPC->add_component<MonsterHPComponent>();
	NPC->add_component<AnimationComponent>();
	NPC->add_component<RenderComponent>();


	NPCScript* NPC_logic = nullptr;

	switch (npc_spawn_packet._npc_type)
	{
		case common::packet::NPCType::Basic:
			{
				NPC_logic = NPC->add_component<NPCScript>().get();
			}
			break;
		case common::packet::NPCType::Tainer:
			{
				NPC_logic = NPC->add_component<TainerScript>().get(); // TainerScript 부착
				//NPC->transform()->set_local_scale({ 25.f, 25.f, 25.f });
			}
			break;
		default:
			CLOG("Unknown NPC Type: " << (int)npc_spawn_packet._npc_type);
			NPC_logic = NPC->add_component<NPCScript>().get();
			break;
	}

	// 4. 공통 데이터 초기화
	if (NPC_logic) {
		NPC_logic->initialize_from_server(npc_spawn_packet);

		// [수정] ID가 설정된 후 명시적으로 ReplicationSystem에 등록
		auto rs = GameFramework::instance()->get_replication_system();
		if (rs) rs->register_entity(npc_spawn_packet._npc_id, NPC_logic);

		ObjectManager::instance()->register_npc(npc_spawn_packet._npc_id, NPC);
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
			script->on_server_update(move_packet);
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
	// 대량의 NPC 이동 정보를 처리하는 시간 측정
	auto start = std::chrono::high_resolution_clock::now();

	// 1. 헤더 읽기
	common::packet::SC_PACKET_NPC_MOVE_BATCH header;
	stream >> header;

	auto rs = GameFramework::instance()->get_replication_system();
	if (!rs) return;

	// 2. 개수만큼 반복하며 데이터 읽기
	for (uint16_t i = 0; i < header._count; ++i) {
		common::packet::NPCMoveData data;
		stream >> data;

		NetSnapshot snapshot;
		snapshot.pos = data._position;
		snapshot.vel = data._velocity;
		snapshot.rot = data._rotation;
		snapshot.state = data._state;
		snapshot.action_id = data._action_id;
		snapshot.timestamp = data._time_stamp;

		//[디버그 로그] 특정 NPC(예: 보스) 업데이트 확인
		if (data._npc_id % 1000 == 999) {
			//CLOG("[BATCH] Update for Boss " << data._npc_id << " Pos: (" << data._position.x << "," << data._position.y << "," << data._position.z << ")");
		}

		// 시스템에 전달 (매우 가벼운 직렬 처리)
		bool success = rs->on_packet_arrival(data._npc_id, snapshot);
		if (!success && i == 0) {
			CLOG("[BATCH] Failed to find NPC: " << data._npc_id);
		}
	}

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

	// 배치가 0.5ms(500us) 이상 소요된 경우 로그 출력
	if (duration > 500) {
		CLOG("[Profiling] NPC_MOVE_BATCH: " << duration << "us, Count: " << (int)header._count);
	}
}

void NetworkManager::HANDLE_S2C_DESPAWN_NPC(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_NPC_DESPAWN packet;
	stream >> packet;

	// DW수정 : 오브젝트 풀링 때문에 무시 테스트
	return;

	// NPC 찾아서 삭제
	auto npc = ObjectManager::instance()->find_npc(packet._npc_id);
	if (npc)
	{
		ObjectManager::instance()->unregister_npc(packet._npc_id);
		npc->destroy();
		// CLOG("[S->C] NPC Despawned (AOI): " << packet._npc_id);
	}
}

void NetworkManager::HANDLE_S2C_DEBUG_DRAW(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_DEBUG_DRAW packet;
	stream >> packet;

	//// [로그 추가]
	//CLOG("[DEBUG_DRAW] Received Packet: Type=" << (int)packet._shape_type
	//	<< " Pos=" << packet._position.x << "," << packet._position.y
	//	<< " Duration=" << packet._duration);

	DebugDrawManager::instance()->AddDebugRequest(packet);
}

void NetworkManager::HANDLE_S2C_DEBUG_BT(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_DEBUG_BT_INFO pkt;
	stream >> pkt;

	std::string debugText;
	stream >> debugText; // "Action_ChaseEnemy [RUNNING]" 형태

	// 해당 ID의 NPC(보스)를 찾아서 스크립트에 전달
	auto npc = ObjectManager::instance()->find_npc(pkt._actor_id);
	if (npc) {
		auto tainer = npc->get_component<TainerScript>();
		if (tainer) {
			tainer->on_debug_bt_info(debugText);
		}
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

	// NPC 카운트 패킷 핸들러 등록
	RegisterHandler(common::packet::PacketType::S2C_P_NPC_COUNT,
		std::bind(&NetworkManager::HANDLE_S2C_NPC_COUNT, this, std::placeholders::_1));
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

	RegisterHandler(common::packet::PacketType::S2C_P_DEBUG_DRAW,
		std::bind(&NetworkManager::HANDLE_S2C_DEBUG_DRAW, this, std::placeholders::_1));

	RegisterHandler(common::packet::PacketType::S2C_P_DEBUG_BT_INFO,
		std::bind(&NetworkManager::HANDLE_S2C_DEBUG_BT, this, std::placeholders::_1));

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
bool NetworkManager::connect_to_server()
{
	_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_socket == INVALID_SOCKET)
	{
		error_display("socket", WSAGetLastError());
		return false;
	}

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(common::packet::SERVER_PORT);
	inet_pton(AF_INET, _server_addr.data(), &addr.sin_addr);

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
