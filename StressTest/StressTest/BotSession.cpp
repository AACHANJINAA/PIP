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
                _id = pkt->_my_session_id; // [중요] 서버에서 할당한 실제 세션 ID로 갱신
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
                _state = BotState::ROOM_WAIT;
            }
            break;
        }
        case PacketType::S2C_P_CHANGE_SCENE:
        {
            // 서버에서 씬 변경 명령이 오면 로딩 완료 후 Ready 패킷 전송 시뮬레이션
            _state = BotState::READY;

            common::packet::CS_PACKET_PLAYER_READY ready_pkt;
            ready_pkt._size = sizeof(ready_pkt);
            ready_pkt._type = PacketType::C2S_P_PLAYER_READY;
            DoWrite(reinterpret_cast<const char*>(&ready_pkt), sizeof(ready_pkt));
            break;
        }
        case PacketType::S2C_P_SPAWN_PLAYER:
        {
            auto pkt = reinterpret_cast<const SC_PACKET_SPAWN_PLAYER*>(header);
            if (pkt->_id == _id) {
                // [추가] 자신의 스폰 위치를 초기 위치로 설정
                _current_pos = pkt->_position;
                _current_rot = pkt->_rotation;
                _anchor_pos = _current_pos;
                _is_anchor_set = true;

                // 자신의 스폰 패킷을 받으면 실제 인게임 상태로 전환
                _state = BotState::INGAME;
            }
            break;
        }
        case PacketType::S2C_P_MOVE:
        {
            auto pkt = reinterpret_cast<const SC_PACKET_MOVE*>(header);
            if (pkt->_id == _id) {
                // [보정] 서버에서 온 위치로 동기화 (Client-Side Prediction Correction)
                _current_pos = pkt->_position;

                // [지연 시간 측정] 서버가 에코한 티크와 현재 티크의 차이 계산
                uint32_t current_tick = static_cast<uint32_t>(GetTickCount64());
                if (current_tick >= pkt->_client_tick) {
                    _last_latency = current_tick - pkt->_client_tick;
                }
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

        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        // 1. 공격(ACTION) 상태 처리
        if (_attack_duration > 0.0f) {
            _attack_duration -= dt;
            _entity_state = common::packet::EntityState::ACTION;
            _action_id = 1; // 기본 공격 ID
            _move_dir = { 0, 0, 0 };
        }
        else {
            // 2. 이동 로직 시뮬레이션
            if (_is_anchor_set) {
                // 일정 확률로 방향 변경 또는 정지
                static std::uniform_real_distribution<float> prob(0.0f, 1.0f);
                if (prob(gen) < 0.02f) { // 약 2% 확률로 방향 전환
                    _move_dir = { dis(gen), 0, dis(gen) };
                    if (common::LengthSq(_move_dir) > 0.001f) {
                        _move_dir = common::Normalize(_move_dir);
                        
                        // 이동 방향에 맞춰 회전 설정 (Yaw)
                        float yaw = atan2f(_move_dir.x, _move_dir.z);
                        // Jolt/Common Quat 구조에 맞춰 설정 (단순화를 위해 Yaw만 반영)
                        // 실제 클라이언트: current_transform->set_local_rotation(0.0f, yawDegrees, 0.0f);
                    }
                    else {
                        _move_dir = { 0, 0, 0 };
                    }
                }

                float speed = 0.0f;
                if (common::LengthSq(_move_dir) > 0.001f) {
                    // 50% 확률로 걷기 또는 달리기
                    if (prob(gen) > 0.5f) {
                        _entity_state = common::packet::EntityState::RUN;
                        speed = common::move_speed::player_run_speed;
                    }
                    else {
                        _entity_state = common::packet::EntityState::MOVE;
                        speed = common::move_speed::player_walk_speed;
                    }
                }
                else {
                    _entity_state = common::packet::EntityState::IDLE;
                    _action_id = 0;
                }

                _current_pos.x += _move_dir.x * speed * dt;
                _current_pos.z += _move_dir.z * speed * dt;

                // 앵커 기준 일정 범위 내로 제한
                float range = 30.0f;
                _current_pos.x = std::clamp(_current_pos.x, _anchor_pos.x - range, _anchor_pos.x + range);
                _current_pos.z = std::clamp(_current_pos.z, _anchor_pos.z - range, _anchor_pos.z + range);
            }
        }

        _move_timer += dt;
        _action_timer += dt;

        // 3. 약 33ms 주기로 서버에 현재 내 위치 전송
        if (_move_timer >= 0.033f) {
            _move_timer = 0.0f;
            SendMove();
        }

        // 4. 약 2~5초마다 공격 수행 시뮬레이션
        if (_action_timer >= (2.0f + dis(gen) * 1.0f)) {
            _action_timer = 0.0f;
            if (_entity_state != common::packet::EntityState::ACTION) {
                _attack_duration = 0.8f; // 공격 애니메이션 시간 시뮬레이션
                SendAction();
            }
        }
    }

    void BotSession::SendMove()
    {
        common::packet::CS_PACKET_MOVE pkt;
        pkt._size = sizeof(pkt);
        pkt._type = common::packet::PacketType::C2S_P_MOVE;
        pkt._position = _current_pos;
        pkt._move_dir = _move_dir; // 실제 이동 방향 벡터 전송
        pkt._rotation = _current_rot;
        pkt._state = _entity_state; // 시뮬레이션된 상태 전송
        pkt._action_id = _action_id;
        pkt._client_tick = static_cast<uint32_t>(GetTickCount64());

        DoWrite(reinterpret_cast<const char*>(&pkt), sizeof(pkt));
    }

    void BotSession::SendAction()
    {
        common::packet::CS_PACKET_ACTION pkt;
        pkt._size = sizeof(pkt);
        pkt._type = common::packet::PacketType::C2S_P_ACTION;
        pkt._action_id = common::packet::ActionID::Common::Attack;
        pkt._target_id = -1;
        pkt._direction = _current_rot;
        pkt._position = _current_pos;
        pkt._client_time_stamp = static_cast<uint32_t>(GetTickCount64());

        DoWrite(reinterpret_cast<const char*>(&pkt), sizeof(pkt));
    }
}
