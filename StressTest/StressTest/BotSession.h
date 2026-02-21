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

    private:
        common::Vec3 _anchor_pos = { 10.0f, 10.0f, 10.0f }; // 서버 스폰 위치 근처를 기본값으로 설정
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
        float               _move_timer = 0.0f;
        float               _action_timer = 0.0f;

        // For Smooth Movement (Simulated)
        common::Vec3        _target_pos = { 0, 0, 0 };
    };
}
