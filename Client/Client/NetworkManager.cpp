#include "stdafx.h"
#include "NetworkManager.h"
#include "GameFramework.h"
#include "LayerManager.h"
#include "MainPlayerScript.h"
#include "ObjectManager.h"
#include "OtherPlayerScript.h"

void error_display(const char* msg, int err_no)
{
	WCHAR* lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(GameFramework::Instance()->hWnd(), lpMsgBuf, (LPCWSTR)msg, MB_OK);
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

void NetworkManager::SendPacket(const char* data, size_t size)
{
	send(_socket, data, static_cast<int>(size), 0);
}

void NetworkManager::ProcessReceivedData(char* data, int size)
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

	SendPacket(stream.mutable_data(), stream.Size());
}

void NetworkManager::SendMovePacket(common::Vec3 position)
{
	// 페이로드가 있는 고정 크기 패킷은 구조체를 바로 사용하는 것이 편리합니다.
	common::packet::CS_PACKET_MOVE packet;
	packet._type = common::packet::PacketType::C2S_P_MOVE;
	packet._size = sizeof(packet);
	packet._position = position;

	// 구조체 자체를 보내도 되지만, 일관성을 위해 PacketStream을 사용할 수 있습니다.
	// 여기서는 구조체를 바로 보내는 더 간단한 방식을 유지합니다.
	SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void NetworkManager::SendAttackPacket()
{
	common::packet::CS_PACKET_ATTACK packet;
	packet._type = common::packet::PacketType::C2S_P_ATTACK;
	packet._size = sizeof(packet);

	SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void NetworkManager::SendRoomListPacket()
{
	common::packet::CS_PACKET_ROOM_LIST packet;
	packet._type = common::packet::PacketType::C2S_P_ROOM_LIST;
	packet._size = sizeof(packet);
	SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}
void NetworkManager::SendEnterRoomPacket(int room_id_to_enter)
{
	common::packet::CS_PACKET_ENTER_ROOM packet;
	packet._type = common::packet::PacketType::C2S_P_ENTER_ROOM;
	packet._size = sizeof(packet);
	packet._room_id = room_id_to_enter;

	SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
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
		CLOG("[S->C] Login successful! My Session ID is now: %lld" << _my_session_id);
	}
	else
	{
		CLOG("[S->C] Login failed!");
		MessageBox(GameFramework::Instance()->hWnd(), L"Login failed.", L"Login Error", MB_OK);
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
		auto main_player = ObjectManager::Instance()->create_game_object("MainPlayer");

		main_player->set_layer("Player");
		auto main_player_logic = main_player->add_component<MainPlayerScript>();

		if (main_player_logic)
			main_player_logic->set_position(spawn_data._position);

		main_player_logic->set_hp(spawn_data._hp);

		

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
		auto other_player = ObjectManager::Instance()->create_game_object(name);
		auto other_player_logic = other_player->add_component<OtherPlayerScript>();
		other_player->transform()->set_local_position(spawn_data._position);
		other_player->set_layer("Enemy");

		other_player_logic->set_hp(spawn_data._hp);
		other_player_logic->set_id(spawn_data._id);
		



		CLOG("[S->C] My player spawned/updated: ID=" << spawn_data._id
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
	auto player = ObjectManager::Instance()->find_by_name("MainPlayer");
	auto player_Trasnform = player->get_component<TransformComponent>();
	if (player && move_packet._id == _my_session_id && player_Trasnform) // 읽어온 id 사용
	{
		player_Trasnform->set_local_position(move_packet._position); // 읽어온 x, y 사용
	}
	else
	{
		auto enemy_layer = LayerManager::Instance()->get_layer_value("Enemy");
		auto other_players = ObjectManager::Instance()->find_by_layer(enemy_layer);
		auto it = std::ranges::find_if(other_players, [&](const std::shared_ptr<GameObject>& other) {
			return move_packet._id == other->get_component<OtherPlayerScript>()->id();
		});
		if (it != other_players.end())
		{
			(*it)->get_component<OtherPlayerScript>()->on_sync_position(move_packet._position);
		}
	}
}

void NetworkManager::HANDLE_S2C_LEAVE(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_LEAVE leave_packet;
	stream >> leave_packet; // id만 읽습니다.
	auto enemy_layer = LayerManager::Instance()->get_layer_value("Enemy");
	auto other_players = ObjectManager::Instance()->find_by_layer(enemy_layer);
	auto it = std::ranges::find_if(other_players, [&](const std::shared_ptr<GameObject>& other) {
		return leave_packet._id == other->get_component<OtherPlayerScript>()->id();
	});
	if (it != other_players.end())
	{
		(*it)->destroy();
	}
}

void NetworkManager::HANDLE_S2C_ATTACK(common::packet::PacketStream& stream)
{
	// SC_PACKET_ATTACK은 헤더 외에 여러 멤버를 가집니다.
	common::packet::SC_PACKET_ATTACK attack_packet;
	stream >> attack_packet; // 구조체 전체를 읽습니다.


	auto player = ObjectManager::Instance()->find_by_name("MainPlayer");
	auto player_logic = player->get_component<MainPlayerScript>();
	if (player && attack_packet._target_id == player_logic->id()) // 읽어온 target_id 사용
	{
		player_logic->set_hp(attack_packet._target_current_hp); // 읽어온 target_current_hp 사용
	}
	else
	{
		auto enemy_layer = LayerManager::Instance()->get_layer_value("Enemy");
		auto other_players = ObjectManager::Instance()->find_by_layer(enemy_layer);
		auto it = std::ranges::find_if(other_players, [&](const std::shared_ptr<GameObject>& other) {
			return attack_packet._target_id == other->get_component<OtherPlayerScript>()->id(); // 읽어온 target_id 사용
		});
		if (it != other_players.end())
		{
			(*it)->get_component<OtherPlayerScript>()->set_hp(attack_packet._target_current_hp); // 읽어온 target_current_hp 사용
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

bool NetworkManager::init_network()
{
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

	RegisterHandler(common::packet::PacketType::S2C_P_MOVE, 
		std::bind(&NetworkManager::HANDLE_S2C_MOVE, this, std::placeholders::_1));

	RegisterHandler(common::packet::PacketType::S2C_P_LEAVE,
		std::bind(&NetworkManager::HANDLE_S2C_LEAVE, this, std::placeholders::_1));

	RegisterHandler(common::packet::PacketType::S2C_P_ATTACK,
		std::bind(&NetworkManager::HANDLE_S2C_ATTACK, this, std::placeholders::_1));

	RegisterHandler(common::packet::PacketType::S2C_P_ROOM_LIST_ACK,
		std::bind(&NetworkManager::HANDLE_S2C_ROOM_LIST_ACK, this, std::placeholders::_1));

	RegisterHandler(common::packet::PacketType::S2C_P_ENTER_ROOM_ACK,
		std::bind(&NetworkManager::HANDLE_S2C_ENTER_ROOM_ACK, this, std::placeholders::_1));

	RegisterHandler(common::packet::PacketType::S2C_P_SPAWN_PLAYER,
		std::bind(&NetworkManager::HANDLE_S2C_SPAWN_PLAYER, this, std::placeholders::_1));

	RegisterHandler(common::packet::PacketType::S2C_P_LOGIN_ACK,
		std::bind(&NetworkManager::HANDLE_S2C_LOGIN_ACK, this, std::placeholders::_1));

	return true;
}

void NetworkManager::disconnect()
{
	if (_socket != INVALID_SOCKET)
	{
		closesocket(_socket);
		_socket = INVALID_SOCKET;
	}
}

void NetworkManager::receive_packets()
{
	if (_socket == INVALID_SOCKET)
	{
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

	// 받은 데이터를 처리 함수로 넘김
	ProcessReceivedData(recv_buffer, retval);
}

