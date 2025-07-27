#pragma once

#include "stdafx.h"
class ClientPacketManager : public Singleton<ClientPacketManager>
{
    friend class Singleton<ClientPacketManager>; // 싱글톤 접근 허용

private:
    SOCKET _socket; // 클라이언트 소켓
    std::vector<char> _recvBuffer; // 수신 버퍼

    // 패킷 핸들러 함수 포인터 타입 정의
    using PacketHandler = std::function<void(char* payload_ptr)>;
    std::unordered_map<uint16_t, PacketHandler> _handlers;

    // 개별 패킷 처리 함수들 (private)
    void Handle_S2C_AVATAR_INFO(char* payload_ptr);
    void Handle_S2C_ENTER(char* payload_ptr);
    void Handle_S2C_MOVE(char* payload_ptr);
    void Handle_S2C_LEAVE(char* payload_ptr);
    void Handle_S2C_ATTACK(char* payload_ptr);

public:
    void Initialize(SOCKET client_socket);
    void SendPacket(const char* data, size_t size);
    void ProcessReceivedData(char* data, int size);

    // 클라이언트 -> 서버 패킷 전송 함수
    void SendLoginPacket(const std::string& name);
    void SendMovePacket(chess::packet::MOVE_TYPE direction);
    void SendAttackPacket(); // 공격 패킷 전송 함수 추가
};
