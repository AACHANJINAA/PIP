#include "stdafx.h"
#include "ClientPacketManager.h"
#include "MainPlayer.h"
#include "OtherPlayer.h"
#include "GameFramework.h"
#include "ObjectManager.h"

// 외부 전역 변수 (Chess_Client.cpp에 정의된 g_hwnd)
extern HWND g_hwnd;
extern void error_display(const char* msg, int err_no);


void ClientPacketManager::Initialize(SOCKET client_socket)
{
	_socket = client_socket;

	// 패킷 핸들러 등록
	RegisterHandler(common::packet::PacketType::S2C_P_MOVE, std::bind(&ClientPacketManager::HANDLE_S2C_MOVE, this, std::placeholders::_1));
	RegisterHandler(common::packet::PacketType::S2C_P_LEAVE, std::bind(&ClientPacketManager::HANDLE_S2C_LEAVE, this, std::placeholders::_1));
	RegisterHandler(common::packet::PacketType::S2C_P_ATTACK, std::bind(&ClientPacketManager::HANDLE_S2C_ATTACK, this, std::placeholders::_1));
	RegisterHandler(common::packet::PacketType::S2C_P_ROOM_LIST_ACK, std::bind(&ClientPacketManager::HANDLE_S2C_ROOM_LIST_ACK, this, std::placeholders::_1));
	RegisterHandler(common::packet::PacketType::S2C_P_ENTER_ROOM_ACK, std::bind(&ClientPacketManager::HANDLE_S2C_ENTER_ROOM_ACK, this, std::placeholders::_1));
	RegisterHandler(common::packet::PacketType::S2C_P_SPAWN_PLAYER, std::bind(&ClientPacketManager::HANDLE_S2C_SPAWN_PLAYER, this, std::placeholders::_1));
	RegisterHandler(common::packet::PacketType::S2C_P_LOGIN_ACK, std::bind(&ClientPacketManager::HANDLE_S2C_LOGIN_ACK, this, std::placeholders::_1));
}

void ClientPacketManager::SendPacket(const char* data, size_t size)
{
	send(_socket, data, static_cast<int>(size), 0);
}

void ClientPacketManager::ProcessReceivedData(char* data, int size)
{
	_recvBuffer.insert(_recvBuffer.end(), data, data + size);
	
	while (true)
	{
		if (_recvBuffer.size() < sizeof(common::packet::PacketHeader))
			break;
		common::packet::PacketHeader* header = reinterpret_cast<common::packet::PacketHeader*>(_recvBuffer.data());

		if (_recvBuffer.size() < header->_size)
			break;

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

void ClientPacketManager::SendLoginPacket(const std::string& name)
{
	common::packet::PacketStream stream;
	common::packet::CS_PACKET_LOGIN login_packet;
	login_packet._type = common::packet::PacketType::C2S_P_LOGIN;

	stream << login_packet;
	stream << name;   // PacketStream이 알아서 [길이][내용]을 써 줌

	// 스트림에 모든 데이터를 쓴 후, 실제 크기를 계산하여 헤더에 덮어쓴다.
	auto* final_header = reinterpret_cast<common::packet::PacketHeader*>(stream.mutable_data());
	final_header->_size = static_cast<uint16_t>(stream.Size());

	SendPacket(stream.mutable_data(), stream.Size());
}

void ClientPacketManager::SendMovePacket(common::Vec3 direction)
{
	// 페이로드가 있는 고정 크기 패킷은 구조체를 바로 사용하는 것이 편리합니다.
	common::packet::CS_PACKET_MOVE packet;
	packet._type = common::packet::PacketType::C2S_P_MOVE;
	packet._size = sizeof(packet);
	packet._direction = direction;

	// 구조체 자체를 보내도 되지만, 일관성을 위해 PacketStream을 사용할 수 있습니다.
	// 여기서는 구조체를 바로 보내는 더 간단한 방식을 유지합니다.
	SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void ClientPacketManager::SendAttackPacket()
{
	common::packet::CS_PACKET_ATTACK packet;
	packet._type = common::packet::PacketType::C2S_P_ATTACK;
	packet._size = sizeof(packet);

	SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void ClientPacketManager::SendRoomListPacket()
{
	common::packet::CS_PACKET_ROOM_LIST packet;
	packet._type = common::packet::PacketType::C2S_P_ROOM_LIST;
	packet._size = sizeof(packet);
	SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}
void ClientPacketManager::SendEnterRoomPacket(int room_id_to_enter)
{
	common::packet::CS_PACKET_ENTER_ROOM packet;
	packet._type = common::packet::PacketType::C2S_P_ENTER_ROOM;
	packet._size = sizeof(packet);
	packet._room_id = room_id_to_enter;

	SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void ClientPacketManager::RegisterHandler(common::packet::PacketType packet_type, PacketHandler packet_handler)
{
	// 내부적으로 핸들러 등록하기 위해서 사용
	_handlers[packet_type] = packet_handler;
}

void ClientPacketManager::HANDLE_S2C_LOGIN_ACK(common::packet::PacketStream& stream)
{
	common::packet::S2C_P_LOGIN_ACK ack_packet;
	stream >> ack_packet;

	if (ack_packet._success)
	{
		_my_session_id = ack_packet._my_session_id; // [핵심] 자신의 ID 저장
		CLOG("[S->C] Login successful! My Session ID is now: %lld" << _my_session_id);
	}
	else
	{
		CLOG("[S->C] Login failed!");
		MessageBox(g_hwnd, L"Login failed.", L"Login Error", MB_OK);
	}
}

void ClientPacketManager::HANDLE_S2C_SPAWN_PLAYER(common::packet::PacketStream& stream)
{
	// [수정] SC_PACKET_SPAWN_PLAYER 구조체 변수를 선언하고 스트림에서 읽어옵니다.
	common::packet::SC_PACKET_SPAWN_PLAYER spawn_data;
	stream >> spawn_data; // 구조체 전체를 읽습니다.

	// [추가] 이름(가변 길이)을 스트림에서 읽어옵니다.
	std::string name;
	stream >> name;

	// 이제 spawn_data 구조체와 name 변수에 올바른 값이 들어있습니다.
	// 이 값들을 사용하여 플레이어 객체를 생성하거나 업데이트합니다.


	// 패킷의 ID가 내 플레이어 ID와 같은지 확인합니다.
	if (spawn_data._id == _my_session_id)
	{
		CLOG("[SPAWN_PLAYER] ID MATCH! Creating MY player (MainPlayer).");
		// 내 플레이어 정보 업데이트
		std::shared_ptr<MainPlayer> my_king = std::make_shared<MainPlayer>();

		auto my_king_Transform = my_king->get_component<TransformComponent>();
		auto my_king_Render = my_king->get_component<RenderComponent>();

		if (my_king_Render)
			my_king_Render->CreateShaderVariables(GameFramework::Instance()->GetDevice().Get(), GameFramework::Instance()->GetCommandList().Get());
		if (my_king_Transform)
			my_king_Transform->set_position(spawn_data._position.x, spawn_data._position.y, spawn_data._position.z);

		my_king->SetHP(spawn_data._hp);
		my_king->SetName(name);
		my_king->SetID(_my_session_id); 

		std::shared_ptr<Mesh> Chess_Mesh = std::make_shared<ReadFbxMesh>(
			GameFramework::Instance()->GetDevice().Get(),
			GameFramework::Instance()->GetCommandList().Get(),
			"Resource/Test/testfbx_texture_included.fbx");
		
		if (my_king_Render)
			my_king_Render->set_mesh(Chess_Mesh);
		if (my_king_Transform)
			my_king_Transform->set_scale(0.01f, 0.01f, 0.01f);

		// 매니저에 넣기
		ObjectManager::Instance()->PushObject(my_king);
		ObjectManager::Instance()->SetPlayer(my_king);
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
		std::shared_ptr<OtherPlayer> other_king = std::make_shared<OtherPlayer>(spawn_data._position.x, spawn_data._position.y);

		auto other_king_Transform = other_king->get_component<TransformComponent>();
		auto other_king_Render = other_king->get_component<RenderComponent>();

		other_king_Render->CreateShaderVariables(GameFramework::Instance()->GetDevice().Get(), GameFramework::Instance()->GetCommandList().Get());
		
		other_king->SetID(spawn_data._id);
		if (other_king_Transform)
		{
			other_king_Transform->set_position(spawn_data._position.x, spawn_data._position.y, spawn_data._position.z); // 위치 설정
		}
		other_king->SetHP(spawn_data._hp); // HP 설정
		other_king->SetName(name); // 이름 설정
		// level, exp 등 추가 정보도 설정 가능
		other_king->_meshType = ENEMY; // 적 타입으로 설정
		std::shared_ptr<Mesh> Chess_Mesh = std::make_shared<ReadObjMesh>(
			GameFramework::Instance()->GetDevice().Get(),
			GameFramework::Instance()->GetCommandList().Get(),
			"Resource/Monster/test_monster.obj");

		// 색 설정
		Chess_Mesh->ChangeColor(GameFramework::Instance()->GetCommandList().Get(), 0.0f, 1.0f, 0.0f, 1.f);
		other_king_Render->set_mesh(Chess_Mesh);

		// 이동 거리 설정
		if (other_king_Transform)
			other_king_Transform->set_scale(1.f, 1.f, 1.f);

		// 매니저에 넣기
		ObjectManager::Instance()->PushEnemy(other_king);
		CLOG("[S->C] My player spawned/updated: ID=" << spawn_data._id
			<< "Pos=" << spawn_data._position.x << "," << spawn_data._position.y
			<< "HP=" << spawn_data._hp
			<< "Level=" << spawn_data._level
			<< "Exp=" << spawn_data._exp
			<< "Name=" << name.c_str());
	}
}

void ClientPacketManager::HANDLE_S2C_MOVE(common::packet::PacketStream& stream)
{
	// SC_PACKET_MOVE는 헤더 외에 여러 멤버를 가집니다.
	common::packet::SC_PACKET_MOVE move_packet;
	stream >> move_packet; // 구조체 전체를 읽습니다.
	auto player = std::dynamic_pointer_cast<MainPlayer>(ObjectManager::Instance()->GetPlayer());
	auto player_Trasnform = player->get_component<TransformComponent>();
	if (player && move_packet._id == player->GetID() && player_Trasnform) // 읽어온 id 사용
	{
		player_Trasnform->set_position(move_packet._position.x, move_packet._position.y, move_packet._position.z); // 읽어온 x, y 사용
	}
	else
	{
		auto other_players = ObjectManager::Instance()->GetEnemy();
		auto it = std::ranges::find_if(other_players, [&](const std::shared_ptr<GameObject>& other) {
			return move_packet._id == static_cast<OtherPlayer*>(other.get())->GetID(); // 읽어온 id 사용
									   });
		auto other_player_Trasnform = dynamic_cast<OtherPlayer*>(it->get())->get_component<TransformComponent>();
		if (it != other_players.end())
		{
			other_player_Trasnform->set_position(move_packet._position.x, move_packet._position.y, move_packet._position.z); // 읽어온 x, y 사용
		}
	}
}

void ClientPacketManager::HANDLE_S2C_LEAVE(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_LEAVE leave_packet;
	stream >> leave_packet; // id만 읽습니다.

	auto other_players = ObjectManager::Instance()->GetEnemy();
	auto it = std::ranges::find_if(other_players, [&](const std::shared_ptr<GameObject>& other) {
		return leave_packet._id == static_cast<OtherPlayer*>(other.get())->GetID(); // 읽어온 id 사용
								   });
	if (it != other_players.end())
	{
		(*it)->_shouldDelete = true;
	}
}

void ClientPacketManager::HANDLE_S2C_ATTACK(common::packet::PacketStream& stream)
{
	// SC_PACKET_ATTACK은 헤더 외에 여러 멤버를 가집니다.
	common::packet::SC_PACKET_ATTACK attack_packet;
	stream >> attack_packet; // 구조체 전체를 읽습니다.


	auto player = std::dynamic_pointer_cast<MainPlayer>(ObjectManager::Instance()->GetPlayer());
	if (player && attack_packet._target_id == player->GetID()) // 읽어온 target_id 사용
	{
		player->SetHP(attack_packet._target_current_hp); // 읽어온 target_current_hp 사용
	}
	else
	{
		auto other_players = ObjectManager::Instance()->GetEnemy();
		auto it = std::ranges::find_if(other_players, [&](const std::shared_ptr<GameObject>& other) {
			return attack_packet._target_id == static_cast<OtherPlayer*>(other.get())->GetID(); // 읽어온 target_id 사용
									   });
		if (it != other_players.end())
		{
			dynamic_cast<OtherPlayer*>(it->get())->SetHP(attack_packet._target_current_hp); // 읽어온 target_current_hp 사용
		}
	}
}

void ClientPacketManager::HANDLE_S2C_ROOM_LIST_ACK(common::packet::PacketStream& stream)
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
void ClientPacketManager::HANDLE_S2C_ENTER_ROOM_ACK(common::packet::PacketStream& stream)
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
