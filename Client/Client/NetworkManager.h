#pragma once
#include "stdafx.h"
void error_display(const char* msg, int err_no);
class NetworkManager : public Singleton<NetworkManager>
{
    friend class Singleton<NetworkManager>; // 싱글톤 접근 허용
    using PacketHandler = std::function<void(common::packet::PacketStream& stream)>;
public:
    bool init_network();
	void cleanup_network();
    bool connect_to_server(std::string_view server_addr, const int& port);
	void disconnect();

    // 메인 게임 루프에서 호출하여 큐에 쌓인 패킷 처리
    void process_queued_packets();

	// 서버로 패킷 전송 함수 즉시 전송(Blocking)
    void send_packet(const char* data, size_t size);

public:
    // 클라이언트 -> 서버 패킷 전송 함수
    void SendLoginPacket(const std::string& name);
    void SendMovePacket(common::Vec3 position, common::Quat rotation, common::packet::OBJECT_STATE state, uint32_t current_tick);
    void SendAttackPacket(); // 공격 패킷 전송 함수 추가
    void SendActionPacket(common::packet::ActionType type, int32_t actionID, int64_t targetID, common::Vec3 pos, common::Quat dir);
    void SendRoomListPacket();
    void SendEnterRoomPacket(int room_id_to_enter);

private:
	void network_worker(); // 네트워크 스레드 함수

    // 수신 및 패킹 함수
    void recv_packet();

    void RegisterHandler(common::packet::PacketType packet_type, PacketHandler packet_handler);

    // 개별 패킷 처리 함수들 (private)
    void HANDLE_S2C_LOGIN_ACK(common::packet::PacketStream& stream);
    void HANDLE_S2C_SPAWN_PLAYER(common::packet::PacketStream& stream);
    void HANDLE_S2C_MOVE(common::packet::PacketStream& stream);
    void HANDLE_S2C_LEAVE(common::packet::PacketStream& stream);
    void HANDLE_S2C_PLAYER_ATTACK(common::packet::PacketStream& stream);
    void HANDLE_S2C_NPC_ATTACK(common::packet::PacketStream& stream);
    void HANDLE_S2C_ROOM_LIST_ACK(common::packet::PacketStream& stream);
    void HANDLE_S2C_ENTER_ROOM_ACK(common::packet::PacketStream& stream);
    void HANDLE_S2C_SPAWN_NPC(common::packet::PacketStream& stream);
	void HANDLE_S2C_MOVE_NPC(common::packet::PacketStream& stream);
	void HANDLE_S2C_MOVE_NPC_BATCH(common::packet::PacketStream& stream);
    void HANDLE_S2C_DESPAWN_NPC(common::packet::PacketStream& stream);
	// --- 디버그용 함수들 ---
    void HANDLE_S2C_DEBUG_DRAW(common::packet::PacketStream& stream);
    void HANDLE_S2C_DEBUG_BT(common::packet::PacketStream& stream);

    //TODO: void Handle_S2C_ERROR(common::packet::PacketStream& stream); // 에러 패킷 처리 함수

private:
    SOCKET _socket{ INVALID_SOCKET }; // 클라이언트 소켓
    std::vector<char> _recvBuffer; // 수신 버퍼

    long long _my_session_id = -1; // 자신의 세션 ID (로그인 후 서버로부터 받음) [TODO: 임시로 여기에 저장하긴 했음]
    std::string _name;
    // 패킷 핸들러 함수 포인터 타입 정의
    std::unordered_map<common::packet::PacketType, PacketHandler> _handlers;

	std::thread _networkThread;
    std::atomic<bool> _isRunning{ false };

    // 완성된 패킷만 담는 큐 (네트워크 스레드 -> 메인 스레드)
	concurrency::concurrent_queue<std::vector<char>> _packetQueue; // 수신된 패킷을 저장하는 큐
};
