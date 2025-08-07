#include "stdafx.h"
#include "ClientPacketManager.h"
#include "Chess_King.h"
#include "GameFramework.h"
#include "ObjectManager.h"
#include "Other_King.h"

// 외부 전역 변수 (Chess_Client.cpp에 정의된 g_hwnd)
extern HWND g_hwnd;
extern void error_display(const char* msg, int err_no);


void ClientPacketManager::Initialize(SOCKET client_socket)
{
    _socket = client_socket;

    // 패킷 핸들러 등록
    _handlers[chess::packet::PacketType::S2C_P_AVATAR_INFO] =
        std::bind(&ClientPacketManager::Handle_S2C_AVATAR_INFO, this, std::placeholders::_1);
    _handlers[chess::packet::PacketType::S2C_P_ENTER] =
        std::bind(&ClientPacketManager::Handle_S2C_ENTER, this, std::placeholders::_1);
    _handlers[chess::packet::PacketType::S2C_P_MOVE] =
        std::bind(&ClientPacketManager::Handle_S2C_MOVE, this, std::placeholders::_1);
    _handlers[chess::packet::PacketType::S2C_P_LEAVE] =
        std::bind(&ClientPacketManager::Handle_S2C_LEAVE, this, std::placeholders::_1);
    _handlers[chess::packet::PacketType::S2C_P_ATTACK] =
        std::bind(&ClientPacketManager::Handle_S2C_ATTACK, this, std::placeholders::_1);
    _handlers[chess::packet::PacketType::S2C_P_ROOM_LIST_ACK] =
        std::bind(&ClientPacketManager::Handle_S2C_ROOM_LIST_ACK, this, std::placeholders::_1);
    _handlers[chess::packet::PacketType::S2C_P_ENTER_ROOM_ACK] =
        std::bind(&ClientPacketManager::Handle_S2C_ENTER_ROOM_ACK, this, std::placeholders::_1);
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
        if (_recvBuffer.size() < sizeof(chess::packet::PacketHeader))
             break;
		chess::packet::PacketHeader * header = reinterpret_cast<chess::packet::PacketHeader*>(_recvBuffer.data());
        
		if (_recvBuffer.size() < header->_size)
			break;
        
		// [수정] PacketStream으로 감싸서 핸들러에 전달
		chess::packet::PacketStream stream(_recvBuffer.data(), header->_size);
        
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
    chess::packet::PacketStream stream;
    chess::packet::PacketHeader header;
    header._type = chess::packet::PacketType::C2S_P_LOGIN;
    header._size = 0; // 최종 크기를 모르므로 임시로 0으로 설정

    stream << header;
    stream << name;   // PacketStream이 알아서 [길이][내용]을 써 줌

    // 스트림에 모든 데이터를 쓴 후, 실제 크기를 계산하여 헤더에 덮어쓴다.
    auto* final_header = reinterpret_cast<chess::packet::PacketHeader*>(stream.mutable_data());
    final_header->_size = static_cast<uint16_t>(stream.Size());

    SendPacket(stream.mutable_data(), stream.Size());
}

void ClientPacketManager::SendMovePacket(chess::packet::MOVE_TYPE direction)
{
    // 페이로드가 있는 고정 크기 패킷은 구조체를 바로 사용하는 것이 편리합니다.
    chess::packet::CS_PACKET_MOVE packet;
    packet._type = chess::packet::PacketType::C2S_P_MOVE;
    packet._size = sizeof(packet);
    packet._direction = direction;

    // 구조체 자체를 보내도 되지만, 일관성을 위해 PacketStream을 사용할 수 있습니다.
    // 여기서는 구조체를 바로 보내는 더 간단한 방식을 유지합니다.
    SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void ClientPacketManager::SendAttackPacket()
{
    chess::packet::CS_PACKET_ATTACK packet;
    packet._type = chess::packet::PacketType::C2S_P_ATTACK;
    packet._size = sizeof(packet);

    SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void ClientPacketManager::SendRoomListPacket()
{
    chess::packet::CS_PACKET_ROOM_LIST packet;
    packet._type = chess::packet::PacketType::C2S_P_ROOM_LIST;
    packet._size = sizeof(packet);
    SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}
void ClientPacketManager::SendEnterRoomPacket(int room_id_to_enter)
{
    chess::packet::CS_PACKET_ENTER_ROOM packet;
    packet._type = chess::packet::PacketType::C2S_P_ENTER_ROOM;
    packet._size = sizeof(packet);
    packet._room_id = room_id_to_enter;

    SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}


// 개별 패킷 처리 함수 구현
void ClientPacketManager::Handle_S2C_AVATAR_INFO(chess::packet::PacketStream& stream)
{
    // SC_PACKET_AVATAR_INFO는 헤더 외에 여러 멤버를 가집니다.
	// 이들을 순서대로 읽습니다.
    int64_t id;
    short x, y, hp, level;
    int exp;

    stream >> id >> x >> y >> hp >> level >> exp;

    auto player = std::dynamic_pointer_cast<CChess_King>(CObjectManager::GetManager()->GetPlayer());
    if (player == nullptr)
    {
        player = std::make_shared<CChess_King>();
    }

    player->SetID(id); // 읽어온 id 사용
    player->SetPos(x, y); // 읽어온 x, y 사용
    player->SetHP(hp); // 읽어온 hp 사용
    player->m_Mesh_Type = PLAYER_CHESS;
    CObjectManager::GetManager()->RequestObject(player);
}

void ClientPacketManager::Handle_S2C_ENTER(chess::packet::PacketStream& stream)
{
    // SC_PACKET_ENTER는 헤더 외에 여러 멤버와 가변 길이 이름을 가집니다.
    int64_t id;
    short x, y;
    std::string name;

    stream >> id >> x >> y >> name; // 순서대로 읽습니다.

    auto Other = std::make_shared<COther_King>(x, y);
    Other->SetID(id);
    Other->SetName(name);
    Other->m_Mesh_Type = ENEMY_CHESS;
    CObjectManager::GetManager()->RequestObject(Other);
}

void ClientPacketManager::Handle_S2C_MOVE(chess::packet::PacketStream& stream)
{
    // SC_PACKET_MOVE는 헤더 외에 여러 멤버를 가집니다.
	int64_t id;
	short x, y;
	stream >> id >> x >> y; // 순서대로 읽습니다.
	auto player = std::dynamic_pointer_cast<CChess_King>(CObjectManager::GetManager()->GetPlayer());
	if (player && id == player->GetID()) // 읽어온 id 사용
    {
		player->SetPos(x, y); // 읽어온 x, y 사용
	}
	else
    {
		auto other_players = CObjectManager::GetManager()->GetEnemy();
		auto it = std::ranges::find_if(other_players, [&](const std::shared_ptr<CGameObject>& other) 
		{
			return id == static_cast<COther_King*>(other.get())->GetID(); // 읽어온 id 사용
		});
		if (it != other_players.end())
		{
			dynamic_cast<COther_King*>(it->get())->SetPos(x, y); // 읽어온 x, y 사용
		}
    }
}

void ClientPacketManager::Handle_S2C_LEAVE(chess::packet::PacketStream& stream)
{
    int64_t id;
    stream >> id; // id만 읽습니다.

    auto other_players = CObjectManager::GetManager()->GetEnemy();
    auto it = std::ranges::find_if(other_players, [&](const std::shared_ptr<CGameObject>& other)
    {
        return id == static_cast<COther_King*>(other.get())->GetID(); // 읽어온 id 사용
    });
    if (it != other_players.end())
    {
        (*it)->m_Delete = true;
    }
}

void ClientPacketManager::Handle_S2C_ATTACK(chess::packet::PacketStream& stream)
{
    // SC_PACKET_ATTACK은 헤더 외에 여러 멤버를 가집니다.
    int64_t attacker_id, target_id;
    int32_t damage, target_current_hp;

    stream >> attacker_id >> target_id >> damage >> target_current_hp; // 순서대로 읽습니다.

    auto player = std::dynamic_pointer_cast<CChess_King>(CObjectManager::GetManager()->GetPlayer());
    if (player && target_id == player->GetID()) // 읽어온 target_id 사용
    {
        player->SetHP(target_current_hp); // 읽어온 target_current_hp 사용
    }
    else
    {
        auto other_players = CObjectManager::GetManager()->GetEnemy();
        auto it = std::ranges::find_if(other_players, [&](const std::shared_ptr<CGameObject>& other) 
        {
        	return target_id == static_cast<COther_King*>(other.get())->GetID(); // 읽어온 target_id 사용
        });
        if (it != other_players.end())
        {
            dynamic_cast<COther_King*>(it->get())->SetHP(target_current_hp); // 읽어온 target_current_hp 사용
        }
    }
}

void ClientPacketManager::Handle_S2C_ROOM_LIST_ACK(chess::packet::PacketStream& stream)
{
    uint16_t room_count;
    stream >> room_count; // room_count만 읽습니다.

    CLOG(L"[S->C] Received room list! Room count: %hu", room_count);

    for (int i = 0; i < room_count; ++i) // 읽어온 room_count 사용
    {
        chess::packet::RoomInfo room_info;
        stream >> room_info; // RoomInfo 구조체 하나를 읽습니다.
        CLOG(L"  - Room ID: %d, Players: %u", room_info._room_id, static_cast<unsigned int
        >(room_info._player_count));
    }
}
void ClientPacketManager::Handle_S2C_ENTER_ROOM_ACK(chess::packet::PacketStream& stream)
{
    extern CGameFramework gGameFramework;
    extern HWND g_hwnd;

    bool success;
    int room_id;
    stream >> success >> room_id; // success와 room_id를 읽습니다.

    if (success) // 읽어온 success 사용
    {
        CLOG(L"[S->C] Successfully entered room %d!", room_id); // 읽어온 room_id 사용
		//gGameFramework.ChangeScene(CGameFramework::SceneType::InGame); // 게임 씬으로 전환
    }
    else
    {
        MessageBox(g_hwnd, L"Failed to enter room.", L"Room Entry Error", MB_OK);
    }
}
