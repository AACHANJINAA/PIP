#include "stdafx.h"
#include "ClientPacketManager.h"
#include "Chess_King.h"
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

        chess::packet::PacketHeader* header = reinterpret_cast<chess::packet::PacketHeader*>(_recvBuffer.data());

        if (_recvBuffer.size() < header->_size)
            break;

        char* payload_ptr = _recvBuffer.data() + sizeof(chess::packet::PacketHeader);
        chess::packet::PacketType type = static_cast<chess::packet::PacketType>(header->_type);

        auto it = _handlers.find(header->_type);
        if (it != _handlers.end())
        {
            it->second(payload_ptr);
        }
        else
        {
            // 알 수 없는 패킷 타입 처리 (선택 사항)
            // MessageBox(g_hwnd, L"Unknown Packet Type", L"Error", MB_OK);
        }

        _recvBuffer.erase(_recvBuffer.begin(), _recvBuffer.begin() + header->_size);
    }
}

void ClientPacketManager::SendLoginPacket(const std::string& name)
{
    uint16_t name_len = static_cast<uint16_t>(name.length());
    
    uint16_t packet_type = static_cast<uint16_t>(chess::packet::PacketType::C2S_P_LOGIN);
    uint16_t total_size = sizeof(chess::packet::PacketHeader) + sizeof(name_len) + name_len;

    std::vector<char> buffer(total_size);
    char* p = buffer.data();

    memcpy(p, &total_size, sizeof(total_size)); p += sizeof(total_size);
    memcpy(p, &packet_type, sizeof(packet_type)); p += sizeof(packet_type);
    
    memcpy(p, &name_len, sizeof(name_len)); p += sizeof(name_len);
    memcpy(p, name.c_str(), name_len);

    SendPacket(buffer.data(), total_size);
}

void ClientPacketManager::SendMovePacket(chess::packet::MOVE_TYPE direction)
{
    uint16_t packet_type = static_cast<uint16_t>(chess::packet::PacketType::C2S_P_MOVE);
    uint16_t total_size = sizeof(chess::packet::PacketHeader) + sizeof(direction);

    std::vector<char> buffer(total_size);
    char* p = buffer.data();

    memcpy(p, &total_size, sizeof(total_size)); p += sizeof(total_size);
    memcpy(p, &packet_type, sizeof(packet_type)); p += sizeof(packet_type);
    memcpy(p, &direction, sizeof(direction));

    SendPacket(buffer.data(), total_size);
}

void ClientPacketManager::SendAttackPacket()
{
    uint16_t packet_type = static_cast<uint16_t>(chess::packet::PacketType::C2S_P_ATTACK);
    uint16_t total_size = sizeof(chess::packet::PacketHeader); // 공격 패킷은 추가 페이로드가 없음

    std::vector<char> buffer(total_size);
    char* p = buffer.data();

    memcpy(p, &total_size, sizeof(total_size)); p += sizeof(total_size);
    memcpy(p, &packet_type, sizeof(packet_type));

    SendPacket(buffer.data(), total_size);
}


// 개별 패킷 처리 함수 구현
void ClientPacketManager::Handle_S2C_AVATAR_INFO(char* payload_ptr)
{
    chess::packet::SC_PACKET_AVATAR_INFO* pkt = reinterpret_cast<chess::packet::SC_PACKET_AVATAR_INFO*>(payload_ptr);
    auto player = std::dynamic_pointer_cast<CChess_King>(CObjectManager::GetManager()->GetPlayer());
    if (player == nullptr)
    {
        player = std::make_shared<CChess_King>();
    }
    player->SetID(pkt->_id);
    player->SetPos(pkt->_x, pkt->_y);
    player->m_Mesh_Type = PLAYER_CHESS;

    CObjectManager::GetManager()->RequestObject(player);
    
    // g_myPlayer.hp = pkt->_hp;
    // g_myPlayer.exp = pkt->_exp;
    // g_myPlayer.level = pkt->_level; //아직은 안씀
}

void ClientPacketManager::Handle_S2C_ENTER(char* payload_ptr)
{
    char* p = payload_ptr;

    int64_t new_id;
    memcpy(&new_id, p, sizeof(new_id)); p += sizeof(new_id);

    char obj_type;
    memcpy(&obj_type, p, sizeof(obj_type)); p += sizeof(obj_type);

    short x, y;
    memcpy(&x, p, sizeof(x)); p += sizeof(x);
    memcpy(&y, p, sizeof(y)); p += sizeof(y);

    uint16_t name_len;
    memcpy(&name_len, p, sizeof(name_len)); p += sizeof(name_len);

    {
	    std::string name(p, name_len);
	    auto Other = std::make_shared<COther_King>(x, y);
        Other->SetID(new_id);
        Other->SetName(name);
        Other->m_Mesh_Type = ENEMY_CHESS;

		CObjectManager::GetManager()->RequestObject(Other); // lazy loading
    }
}

void ClientPacketManager::Handle_S2C_MOVE(char* payload_ptr)
{
    chess::packet::SC_PACKET_MOVE* pkt = reinterpret_cast<chess::packet::SC_PACKET_MOVE*>(payload_ptr);
    auto player = std::dynamic_pointer_cast<CChess_King>(CObjectManager::GetManager()->GetPlayer());
    if (pkt->_id == player->GetID())
    {
        player->SetPos(pkt->_x, pkt->_y);
    }
    else
    {
        auto other_players = CObjectManager::GetManager()->GetEnemy();
        auto it = std::find_if(other_players.begin(), other_players.end(), [pkt](const std::shared_ptr<CGameObject>& other) {
            return pkt->_id == static_cast<COther_King*>(other.get())->GetID();
        });
        if (it != other_players.end())
        {
            dynamic_cast<COther_King*>(it->get())->SetPos(pkt->_x, pkt->_y);
        }
    }
}

void ClientPacketManager::Handle_S2C_LEAVE(char* payload_ptr)
{
    chess::packet::SC_PACKET_LEAVE* pkt = reinterpret_cast<chess::packet::SC_PACKET_LEAVE*>(payload_ptr);
    auto other_players = CObjectManager::GetManager()->GetEnemy();
    auto it = std::find_if(other_players.begin(), other_players.end(), [pkt](const std::shared_ptr<CGameObject>& other) {
        return pkt->_id == static_cast<COther_King*>(other.get())->GetID();
    });
    if (it != other_players.end())
    {
        (*it)->m_Delete = true;
    }
}

void ClientPacketManager::Handle_S2C_ATTACK(char* payload_ptr)
{
    chess::packet::SC_PACKET_ATTACK* pkt = reinterpret_cast<chess::packet::SC_PACKET_ATTACK*>(payload_ptr);
    auto player = std::dynamic_pointer_cast<CChess_King>(CObjectManager::GetManager()->GetPlayer());

    if (pkt->_target_id == player->GetID())
    {
        player->SetHP(pkt->_target_current_hp);
        // TODO: UI에 HP 변화 표시 로직 추가
    }
    else
    {
        auto other_players = CObjectManager::GetManager()->GetEnemy();
        auto it = std::find_if(other_players.begin(), other_players.end(), [pkt](const std::shared_ptr<CGameObject>& other) {
            return pkt->_target_id == static_cast<COther_King*>(other.get())->GetID();
        });
        if (it != other_players.end())
        {
            dynamic_cast<COther_King*>(it->get())->SetHP(pkt->_target_current_hp);
            // TODO: 다른 플레이어의 UI에 HP 변화 표시 로직 추가
        }
    }
}
