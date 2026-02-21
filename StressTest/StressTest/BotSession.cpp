#include "pch.h"
#include "BotSession.h"

namespace PIP::BOT
{
    BotSession::BotSession(int64_t bot_id, int target_room_id)
        : _id(bot_id), _target_room_id(target_room_id), _recv_buffer(1024 * 16)
    {
    }

    BotSession::~BotSession()
    {
        Stop();
    }

    bool BotSession::Start(HANDLE iocp, const std::string& host, short port)
    {
        _socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (_socket == INVALID_SOCKET) return false;

        // IOCP 등록
        CreateIoCompletionPort((HANDLE)_socket, iocp, (ULONG_PTR)this, 0);

        // Bind for ConnectEx (Windows 특화)
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = 0;
        bind(_socket, (sockaddr*)&addr, sizeof(addr));

        // Connect (Async using ConnectEx or simple for now)
        // 스트레스 테스트는 동기 Connect 후 IOCP 등록하는 방식으로 간소화 가능
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);
        server_addr.sin_port = htons(port);

        if (connect(_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            Stop();
            return false;
        }

        OnConnect();
        return true;
    }

    void BotSession::Stop()
    {
        BotState old_state = _state.exchange(BotState::DISCONNECTED);
        if (old_state == BotState::DISCONNECTED) return;

        if (_socket != INVALID_SOCKET) {
            closesocket(_socket);
            _socket = INVALID_SOCKET;
        }
    }

    void BotSession::OnConnect()
    {
        _state = BotState::LOGGING_IN;

        // 1. 패킷 스트림 생성
        common::packet::PacketStream stream;

        // 2. 로그인 구조체 설정
        common::packet::CS_PACKET_LOGIN pkt;
        pkt._type = common::packet::PacketType::C2S_P_LOGIN;
        pkt._size = 0; // 나중에 갱신

        // 3. 봇의 고유 이름을 생성 (예: Bot_30001)
        std::string bot_name = "Bot_" + std::to_string(_id);

        // 4. 스트림에 구조체와 이름 밀어넣기
        stream << pkt;
        stream << bot_name;

        // 5. 최종 패킷 크기 갱신 및 전송
        auto* header = reinterpret_cast<common::packet::PacketHeader*>(stream.mutable_data());
        header->_size = static_cast<uint16_t>(stream.Size());

        DoWrite(stream.constable_data(), stream.Size());

        DoRead();
    }

    void BotSession::DoRead()
    {
        if (_state == BotState::DISCONNECTED) return;

        ZeroMemory(&_recv_over._over, sizeof(_recv_over._over));
        _recv_over._wsabuf.buf = (char*)(_recv_buffer.data() + _processed_size);
        _recv_over._wsabuf.len = (ULONG)(_recv_buffer.size() - _processed_size);

        DWORD flags = 0;
        DWORD bytes_received = 0;
        if (WSARecv(_socket, &_recv_over._wsabuf, 1, &bytes_received, &flags, &_recv_over._over, NULL) == SOCKET_ERROR) {
            if (WSAGetLastError() != WSA_IO_PENDING) {
                Stop();
            }
        }
    }

    void BotSession::OnRecv(size_t len)
    {
        _processed_size += len;
        while (_processed_size >= sizeof(common::packet::PacketHeader))
        {
            auto header = reinterpret_cast<const common::packet::PacketHeader*>(_recv_buffer.data());
            if (_processed_size < header->_size)
                break;

            HandlePacket(header);

            size_t packet_size = header->_size;
            std::copy(_recv_buffer.begin() + packet_size, _recv_buffer.begin() + _processed_size, _recv_buffer.begin());
            _processed_size -= packet_size;
        }
        DoRead();
    }

    void BotSession::DoWrite(const char* data, size_t size)
    {
        if (_state == BotState::DISCONNECTED) return;

        OVERLAPPED_EX* send_over = new OVERLAPPED_EX(IO_SEND);
        memcpy(send_over->_buffer, data, size);
        send_over->_wsabuf.len = (ULONG)size;

        DWORD bytes_sent = 0;
        if (WSASend(_socket, &send_over->_wsabuf, 1, &bytes_sent, 0, &send_over->_over, NULL) == SOCKET_ERROR) {
            if (WSAGetLastError() != WSA_IO_PENDING) {
                delete send_over;
                Stop();
            }
        }
    }

    void BotSession::OnSend(size_t len)
    {
        // For simplicity, just confirmation. Production code would handle buffer pooling.
    }

    void BotSession::HandlePacket(const common::packet::PacketHeader* header)
    {
        using namespace common::packet;
        switch (header->_type)
        {
        case PacketType::S2C_P_LOGIN_ACK:
        {
            auto pkt = reinterpret_cast<const SC_PACKET_LOGIN_ACK*>(header);
            if (pkt->_success)
            {
                _state = BotState::ENTER_ROOM;
                CS_PACKET_ENTER_ROOM enter_pkt;
                enter_pkt._size = sizeof(enter_pkt);
                enter_pkt._type = PacketType::C2S_P_ENTER_ROOM;
                enter_pkt._room_id = _target_room_id;
                DoWrite(reinterpret_cast<const char*>(&enter_pkt), sizeof(enter_pkt));
            }
            break;
        }
        case PacketType::S2C_P_ENTER_ROOM_ACK:
        {
            auto pkt = reinterpret_cast<const SC_PACKET_ENTER_ROOM_ACK*>(header);
            if (pkt->_success)
            {
                _state = BotState::INGAME;
            }
            break;
        }
        case PacketType::S2C_P_MOVE:
        {
            auto pkt = reinterpret_cast<const SC_PACKET_MOVE*>(header);
            if (pkt->_id == _id) {
                _current_pos = pkt->_position;
            }
            break;
        }
        default:
            break;
        }
    }

    void BotSession::Update(float dt)
    {
        if (_state != BotState::INGAME) return;

        _move_timer += dt;
        _action_timer += dt;

        if (_move_timer >= 0.033f) {
            _move_timer = 0.0f;
            SendMove();
        }

        if (_action_timer >= 0.5f) {
            _action_timer = 0.0f;
            SendAction();
        }
    }

    void BotSession::SendMove()
    {
        // 1. 초기 위치(Anchor) 설정 (처음 이동 패킷 보낼 때 현재 위치를 기준으로 삼음)
        if (!_is_anchor_set) {
            _anchor_pos = _current_pos;
            _is_anchor_set = true;
        }

        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        // 2. 랜덤 이동량 계산 (초당 약 3~5m 속도 느낌)
        float move_speed = 0.5f;
        _current_pos.x += dis(gen) * move_speed;
        _current_pos.z += dis(gen) * move_speed;

        // 3. 10x10 범위 내로 강제 제한 (Anchor +- 5.0f)
        float range = 5.0f;
        _current_pos.x = std::clamp(_current_pos.x, _anchor_pos.x - range, _anchor_pos.x + range);
        _current_pos.z = std::clamp(_current_pos.z, _anchor_pos.z - range, _anchor_pos.z + range);

        // 4. 패킷 생성 및 전송
        common::packet::CS_PACKET_MOVE pkt;
        pkt._size = sizeof(pkt);
        pkt._type = common::packet::PacketType::C2S_P_MOVE;
        pkt._position = _current_pos;
        pkt._rotation = _current_rot;
        pkt._state = common::packet::OBJECT_STATE::WALK;
        pkt._client_tick = static_cast<uint32_t>(GetTickCount64());

        DoWrite(reinterpret_cast<const char*>(&pkt), sizeof(pkt));
    }

    void BotSession::SendAction()
    {
        common::packet::CS_PACKET_ACTION pkt;
        pkt._size = sizeof(pkt);
        pkt._type = common::packet::PacketType::C2S_P_ACTION;
        pkt._action_type = common::packet::ActionType::NORMAL_ATTACK;
        pkt._action_id = 1;
        pkt._target_id = -1;
        pkt._direction = _current_rot;
        pkt._position = _current_pos;
        pkt._client_time_stamp = static_cast<uint32_t>(GetTickCount64());

        DoWrite(reinterpret_cast<const char*>(&pkt), sizeof(pkt));
    }
}
