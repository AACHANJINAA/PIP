#pragma once

#include "stdafx.h"
class ClientPacketManager : public Singleton<ClientPacketManager>
{
    friend class Singleton<ClientPacketManager>; // 싱글톤 접근 허용

private:
    SOCKET _socket; // 클라이언트 소켓
    std::vector<char> _recvBuffer; // 수신 버퍼
	long long _my_session_id = -1; // 자신의 세션 ID (로그인 후 서버로부터 받음) [TODO: 임시로 여기에 저장하긴 했음]

    // 패킷 핸들러 함수 포인터 타입 정의
    using PacketHandler = std::function<void(chess::packet::PacketStream& stream)>;
    std::unordered_map<chess::packet::PacketType, PacketHandler> _handlers;

    void RegisterHandler(chess::packet::PacketType packet_type, PacketHandler packet_handler);

    // 개별 패킷 처리 함수들 (private)
	void HANDLE_S2C_LOGIN_ACK(chess::packet::PacketStream& stream);
	void HANDLE_S2C_SPAWN_PLAYER(chess::packet::PacketStream& stream);
    void HANDLE_S2C_MOVE(chess::packet::PacketStream& stream);
    void HANDLE_S2C_LEAVE(chess::packet::PacketStream& stream);
    void HANDLE_S2C_ATTACK(chess::packet::PacketStream& stream);
    void HANDLE_S2C_ROOM_LIST_ACK(chess::packet::PacketStream& stream);
    void HANDLE_S2C_ENTER_ROOM_ACK(chess::packet::PacketStream& stream);
    //TODO: void Handle_S2C_ERROR(chess::packet::PacketStream& stream); // 에러 패킷 처리 함수

public:
    void Initialize(SOCKET client_socket);
    void SendPacket(const char* data, size_t size);
    void ProcessReceivedData(char* data, int size);

    // 클라이언트 -> 서버 패킷 전송 함수
    void SendLoginPacket(const std::string& name);
    void SendMovePacket(chess::packet::MOVE_TYPE direction);
    void SendAttackPacket(); // 공격 패킷 전송 함수 추가
    void SendRoomListPacket();
    void SendEnterRoomPacket(int room_id_to_enter);
};
