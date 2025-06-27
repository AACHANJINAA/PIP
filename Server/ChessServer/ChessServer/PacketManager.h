#pragma once
#include <utility>

#include "CommonHeader.h"
#include "Packet.h"
#include "PacketHandlers.h"

namespace chess::packet
{
    using PacketHandler = std::function<void(std::shared_ptr<chess::server::SESSION>, chess::packet::PacketStream&)>;

    class PacketManager : public Singleton<PacketManager>
    {
    private:
        std::unordered_map<uint16_t, PacketHandler> _handlers; // '주소록' (패킷 ID와 핸들러 함수를 연결)

    public:
        void Initialize();
        // 서버 시작 시, 주소록에 "이 ID는 이 함수가 처리해" 라고 등록
        void RegisterHandler(uint16_t packetId, PacketHandler handler)
        {
            _handlers[packetId] = std::move(handler);
        }

        void Dispatch(uint16_t packetId, const std::shared_ptr<chess::server::SESSION>& session, chess::packet::PacketStream& stream)
        {
            LOG("[DISPATCHER] Dispatching packet type " << packetId << " for Session ID: " << session->_id);

            auto it = _handlers.find(packetId);
            if (it != _handlers.end())
            {
                // 더미 변수에 헤더를 읽어 스트림의 읽기 위치(_pos)를 안전하게 이동시킵니다.
                PacketHeader dummyHeader;
                stream >> dummyHeader;

                // 이제 핸들러는 헤더 바로 다음부터 읽기 시작합니다.
                it->second(session, stream);
            }
            else
            {
                LOG("[DISPATCHER] **ERROR**: No handler found for packet type " << packetId);
            }
        }
    };
}
