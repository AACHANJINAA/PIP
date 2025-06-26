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
            auto it = _handlers.find(packetId);
            if (it != _handlers.end())
            {
                it->second(session, stream);
            }
        }
    };
}
