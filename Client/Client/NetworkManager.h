#pragma once
#include "stdafx.h"
void error_display(const char* msg, int err_no);
class NetworkManager : public Singleton<NetworkManager>
{
    friend class Singleton<NetworkManager>; // 싱글톤 접근 허용

private:
    
    SOCKET _socket; // 클라이언트 소켓
    std::vector<char> _recvBuffer; // 수신 버퍼
	long long _my_session_id = -1; // 자신의 세션 ID (로그인 후 서버로부터 받음) [TODO: 임시로 여기에 저장하긴 했음]
    std::string _name;
    // 패킷 핸들러 함수 포인터 타입 정의
    using PacketHandler = std::function<void(common::packet::PacketStream& stream)>;
    std::unordered_map<common::packet::PacketType, PacketHandler> _handlers;

    void RegisterHandler(common::packet::PacketType packet_type, PacketHandler packet_handler);

    // 개별 패킷 처리 함수들 (private)
    void HANDLE_S2C_LOGIN_ACK(common::packet::PacketStream& stream);
    void HANDLE_S2C_SPAWN_PLAYER(common::packet::PacketStream& stream);
    void HANDLE_S2C_MOVE(common::packet::PacketStream& stream);
    void HANDLE_S2C_LEAVE(common::packet::PacketStream& stream);
    void HANDLE_S2C_ATTACK(common::packet::PacketStream& stream);
    void HANDLE_S2C_ROOM_LIST_ACK(common::packet::PacketStream& stream);
    void HANDLE_S2C_ENTER_ROOM_ACK(common::packet::PacketStream& stream);
    //TODO: void Handle_S2C_ERROR(common::packet::PacketStream& stream); // 에러 패킷 처리 함수

    void ProcessReceivedData(char* data, int size);
public:
    bool init_network();
	void cleanup_network();

    bool connect_to_server(std::string_view server_addr, const int& port);
	void disconnect();
    // 데이터 수신 및 처리
    void receive_packets();

    /*void Initialize(SOCKET client_socket);*/
    void SendPacket(const char* data, size_t size);

    // 클라이언트 -> 서버 패킷 전송 함수
    void SendLoginPacket(const std::string& name);
    void SendMovePacket(common::Vec3 position);
    void SendAttackPacket(); // 공격 패킷 전송 함수 추가
    void SendRoomListPacket();
    void SendEnterRoomPacket(int room_id_to_enter);
};
