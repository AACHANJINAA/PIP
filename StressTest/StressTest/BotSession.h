#pragma once

#include "pch.h"

namespace PIP::BOT
{
    enum class BotState
    {
        DISCONNECTED,
        CONNECTING,
        LOGGING_IN,
        ENTER_ROOM,
        ROOM_WAIT,
        READY,
        INGAME
    };

    class BotSession : public std::enable_shared_from_this<BotSession>
    {
    public:
        BotSession(int64_t bot_id, int target_room_id);
        ~BotSession();

        bool Start(HANDLE iocp, const std::string& host, short port);
        void Stop();

        // Networking (IOCP Events)
        void DoRead();
        void DoWrite(const char* data, size_t size);
        void HandlePacket(const common::packet::PacketHeader* header);

        // Completion Handlers
        void OnRecv(size_t len);
        void OnSend(size_t len);
        void OnConnect();

        // InGame Logic
        void Update(float dt);
        void SendMove();
        void SendAction();

        bool IsRunning() const { return _state != BotState::DISCONNECTED; }
        BotState GetState() const { return _state; }
        SOCKET GetSocket() const { return _socket; }
        uint32_t GetLastLatency() const { return _last_latency; }

    private:
        common::Vec3 _anchor_pos = { 10.0f, 10.0f, 10.0f }; // ���� ���� ��ġ ��ó�� �⺻������ ����
        bool         _is_anchor_set = false;


        SOCKET              _socket = INVALID_SOCKET;
        int64_t             _id;
        int                 _target_room_id;
        std::atomic<BotState> _state = BotState::DISCONNECTED;

        // Overlapped Objects
        OVERLAPPED_EX       _recv_over{ IO_RECV };

        // Received Data Processing
        std::vector<uint8_t> _recv_buffer;
        size_t               _processed_size = 0;

        // Movement & Logic State
        common::Vec3        _current_pos = { 0, 0, 0 };
        common::Quat        _current_rot = { 0, 0, 0, 1 };
        common::Vec3        _move_dir = { 0, 0, 0 }; // [추가] 현재 이동 방향
        common::packet::EntityState _entity_state = common::packet::EntityState::IDLE; // [추가] 엔티티 상태
        int32_t             _action_id = 0; // [추가] 액션 ID
        float               _attack_duration = 0.0f; // [추가] 공격 지속 시간용

        float               _move_timer = 0.0f;
        float               _action_timer = 0.0f;
        uint32_t            _last_latency = 0; // [추가] 마지막으로 측정된 RTT (ms)

        // For Smooth Movement (Simulated)
        common::Vec3        _target_pos = { 0, 0, 0 };
    };
}
