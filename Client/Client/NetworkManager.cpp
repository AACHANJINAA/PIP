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

#include "HPRenderComponent.h"
#include "AnimationComponent.h"

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
//void NetworkManager::Initialize(SOCKET client_socket)
//{
//	_socket = client_socket;
//
//	// 패킷 핸들러 등록
//	RegisterHandler(common::packet::PacketType::S2C_P_MOVE, std::bind(&NetworkManager::HANDLE_S2C_MOVE, this, std::placeholders::_1));
//	RegisterHandler(common::packet::PacketType::S2C_P_LEAVE, std::bind(&NetworkManager::HANDLE_S2C_LEAVE, this, std::placeholders::_1));
//	RegisterHandler(common::packet::PacketType::S2C_P_ATTACK, std::bind(&NetworkManager::HANDLE_S2C_ATTACK, this, std::placeholders::_1));
//	RegisterHandler(common::packet::PacketType::S2C_P_ROOM_LIST_ACK, std::bind(&NetworkManager::HANDLE_S2C_ROOM_LIST_ACK, this, std::placeholders::_1));
//	RegisterHandler(common::packet::PacketType::S2C_P_ENTER_ROOM_ACK, std::bind(&NetworkManager::HANDLE_S2C_ENTER_ROOM_ACK, this, std::placeholders::_1));
//	RegisterHandler(common::packet::PacketType::S2C_P_SPAWN_PLAYER, std::bind(&NetworkManager::HANDLE_S2C_SPAWN_PLAYER, this, std::placeholders::_1));
//	RegisterHandler(common::packet::PacketType::S2C_P_LOGIN_ACK, std::bind(&NetworkManager::HANDLE_S2C_LOGIN_ACK, this, std::placeholders::_1));
//}
void NetworkManager::process_network_events()
{
	if (INVALID_SOCKET == _socket)
	{

		return;
	}
	fd_set read_set, write_set;
	FD_ZERO(&read_set);
	FD_ZERO(&write_set);

	FD_SET(_socket, &read_set);
	if (!_sendBuffer.empty())
	{
		FD_SET(_socket, &write_set);
	}
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	int retval = select(0, &read_set, &write_set, NULL, &timeout);
	if (SOCKET_ERROR == retval)
	{
		error_display("select fail", WSAGetLastError());
		disconnect();
		return;
	}

	if (FD_ISSET(_socket, &read_set))
	{
		recv_packet();
	}
	if (FD_ISSET(_socket, &write_set))
	{
		process_send();
	}

	process_recv();

}
void NetworkManager::send_packet(const char* data, size_t size)
{
	_sendBuffer.insert(_sendBuffer.end(), data, data + size);
}
void NetworkManager::process_send()
{
	if (_sendBuffer.empty())
	{
		return;
	}
	int sent_bytes = send(_socket, _sendBuffer.data(), static_cast<int>(_sendBuffer.size()), 0);
	if (SOCKET_ERROR == sent_bytes)
	{
		int err = WSAGetLastError();
		if (WSAEWOULDBLOCK == err)
		{
			return;
		}
		error_display("send failed", err);
		disconnect();
		return;
	}
	if (sent_bytes > 0)
	{
		_sendBuffer.erase(_sendBuffer.begin(), _sendBuffer.begin() + sent_bytes);
	}
}


void NetworkManager::recv_packet()
{
	if (_socket == INVALID_SOCKET)
	{
		error_display("client socket Invalid", 0);
		return;
	}

	char recv_buffer[4096];
	int retval = recv(_socket, recv_buffer, sizeof(recv_buffer), 0);

	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() == WSAEWOULDBLOCK)
		{
			// 데이터가 없는 정상적인 상황이므로 아무것도 하지 않음
			return;
		}

		// WSAEWOULDBLOCK 이외의 소켓 오류 발생
		error_display("recv failed", WSAGetLastError());
		disconnect(); // 소켓 정리
		// PostQuitMessage(0); // 앱 종료는 상위 레벨(예: 메인 루프)에서 결정
		return;
	}

	if (retval == 0)
	{
		// 서버가 연결을 정상적으로 종료함
		std::cout << "Server disconnected." << std::endl;
		disconnect();
		// PostQuitMessage(0);
		return;
	}
	if (retval > 0)
	{
		_recvBuffer.insert(_recvBuffer.end(), recv_buffer, recv_buffer + retval);
	}
}
void NetworkManager::process_recv()
{
	while (true)
	{
		if (_recvBuffer.size() < sizeof(common::packet::PacketHeader))
			return;
		common::packet::PacketHeader* header = reinterpret_cast<common::packet::PacketHeader*>(_recvBuffer.data());

		if (_recvBuffer.size() < header->_size)
			return;

		// [수정] PacketStream으로 감싸서 핸들러에 전달
		common::packet::PacketStream stream(_recvBuffer.data(), header->_size);

		auto it = _handlers.find(header->_type);
		if (it != _handlers.end())
		{
			it->second(stream); // stream을 그대로 전달
		}

		_recvBuffer.erase(_recvBuffer.begin(), _recvBuffer.begin() + header->_size);
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

void NetworkManager::SendMovePacket(common::Vec3 position, common::Quat rotation)
{
	// 페이로드가 있는 고정 크기 패킷은 구조체를 바로 사용하는 것이 편리합니다.
	common::packet::CS_PACKET_MOVE packet;
	packet._type = common::packet::PacketType::C2S_P_MOVE;
	packet._size = sizeof(packet);
	packet._position = position;
	packet._rotation = rotation;

	// 구조체 자체를 보내도 되지만, 일관성을 위해 PacketStream을 사용할 수 있습니다.
	// 여기서는 구조체를 바로 보내는 더 간단한 방식을 유지합니다.
	send_packet(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void NetworkManager::SendAttackPacket()
{
	common::packet::CS_PACKET_ATTACK packet;
	packet._type = common::packet::PacketType::C2S_P_ATTACK;
	packet._size = sizeof(packet);

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

			auto playerMesh = ResourceManager::instance()->load_mesh("Resource/Character/Bture_Walk/Bture_Walk.gltf", true, "walk");
			ResourceManager::instance()->upload_pending_meshes(GameFramework::instance()->device().Get(), GameFramework::instance()->command_list().Get());
			renderer->set_mesh(playerMesh);

			animation_component->set_mesh(playerMesh);
			animation_component->set_animation("walk");

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
		CLOG("[SPAWN_PLAYER] ID MISMATCH! Creating OTHER player (OtherPlayer).");
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
		auto it = std::ranges::find_if(other_players, [&](const std::shared_ptr<GameObject>& other) {
			auto other_script = other->get_component<OtherPlayerScript>();
			return other_script && move_packet._id == other_script->id();
		});
		if (it != other_players.end())
		{
			auto other_player_script = (*it)->get_component<OtherPlayerScript>();
			other_player_script->on_sync_position(move_packet._position);
			other_player_script->on_sync_rotation(move_packet._rotation);
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

	if (npc_spawn_packet._npc_type == 1)
	{
		//CLOG("[SPAWN_NPC]");
		auto NPC = ObjectManager::instance()->create_game_object(npc_name);
		auto NPC_logic = NPC->add_component<NPCScript>();
		NPC_logic->set_id(npc_spawn_packet._npc_id);
		NPC_logic->set_hp(npc_spawn_packet._hp);
		NPC_logic->set_position(npc_spawn_packet._position);
		NPC->set_layer("Enemy");


		auto npc_mesh = ResourceManager::instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");

		auto render_comp = NPC->add_component<HPRenderComponent>();
		render_comp->set_mesh(npc_mesh);

		// ResourceManager을 통해 재질 생성 및 쉐이더 할당
		std::string material_name = "npc_material"; // player는 고정된 재질
		ResourceManager::instance()->create_material(material_name);
		ResourceManager::instance()->set_shader_for_material(material_name, "gltf_hp");

		// gltf
		render_comp->set_pso_name("gltf_hp");

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
	auto npc_object = ObjectManager::instance()->find_object(npc_name);
	if (npc_object)
	{
		npc_object->transform()->set_local_position(move_packet._position);
	}
	else
	{
		// 디버깅을 위한 로그
		CLOG("[S->C] Move NPC Error: Object not found with name: " << npc_name);
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

	if (SOCKET_ERROR == connect(_socket, (SOCKADDR*)(&addr), sizeof(addr)))
	{
		error_display("connect", WSAGetLastError());
		disconnect();
		return false;
	}
	u_long on = 1;
	if (SOCKET_ERROR == ioctlsocket(_socket, FIONBIO, &on)) // 논블로킹 모드 설정
	{
		error_display("ioctlsocket", WSAGetLastError());
		disconnect();
		return false;
	}
	// TCP_NODELAY 설정 추가
	int nodelay = 1;
	if (SOCKET_ERROR == setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY,
		(const char*)&nodelay, sizeof(nodelay)))
	{
		error_display("TCP_NODELAY", WSAGetLastError());
		// 이건 치명적이지 않으므로 연결을 끊지는 않음
	}
	return true;
}
void NetworkManager::disconnect()
{
	if (_socket != INVALID_SOCKET)
	{
		closesocket(_socket);
		_socket = INVALID_SOCKET;
		PostQuitMessage(0);
	}
}
