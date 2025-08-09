#include "pch.h"
#include "PacketManager.h"

namespace chess::packet
{
	void PacketManager::Initialize()
	{
		// Packet.h에 정의된 이름으로 수정
		RegisterHandler(PacketType::C2S_P_LOGIN, Handle_C2S_LOGIN);
		RegisterHandler(PacketType::C2S_P_MOVE, Handle_C2S_MOVE);
		RegisterHandler(PacketType::C2S_P_ATTACK, Handle_C2S_ATTACK);
		RegisterHandler(PacketType::C2S_P_ENTER_ROOM, Handle_C2S_ENTER_ROOM);
		RegisterHandler(PacketType::C2S_P_ROOM_LIST, Handle_C2S_ROOM_LIST);
		RegisterHandler(PacketType::C2S_P_CHAT_IN_ROOM, Handle_C2S_CHAT_IN_ROOM);
	}

	void PacketManager::Dispatch(const std::shared_ptr<chess::server::SESSION>& session, chess::packet::PacketStream& stream)
    {
        packet::PacketHeader header = stream.PeekHeader();

		server::SESSION_STATE sessionState = session->_state;
       // LOG("[DISPATCHER] Dispatching packet type " << static_cast<int>(header._type) << " for Session ID : " 
       //     << session->_id << " in state " << static_cast<int>(sessionState));

		bool bIsValidPacket = false;

        // 세션 상태에 따라 처리 가능한 패킷인지 검증합니다.
        switch (sessionState)
        {
        case server::SESSION_STATE::ST_LOBBY:
                if (header._type == packet::PacketType::C2S_P_ROOM_LIST 
                    || header._type == packet::PacketType::C2S_P_ENTER_ROOM
                    || header._type == packet::PacketType::C2S_P_LOGIN)
                {
                    bIsValidPacket = true;
                }
                break;

            case server::SESSION_STATE::ST_INGAME:
                // 인게임에서 처리 가능한 모든 패킷 종류를 여기에 명시합니다.
                switch (header._type)
                {
                    case packet::PacketType::C2S_P_MOVE:
                    case packet::PacketType::C2S_P_ATTACK:
                    case packet::PacketType::C2S_P_CHAT_IN_ROOM:
                    case packet::PacketType::C2S_P_ENTER_ROOM: // 인게임 중 다른 방으로 이동
                    case packet::PacketType::C2S_P_ROOM_LIST:  // 인게임 중 방 목록 요청
                        bIsValidPacket = true;
                        break;
                }
                break;
			default:
                ERROR("[DISPATCHER] **ERROR**: Invalid session state " << static_cast<int>(sessionState) 
					<< " for packet type " << static_cast<int>(header._type) << " from session " << session->_id);
                break;
        }

        if (!bIsValidPacket)
        {
            ERROR("[DISPATCHER] **ERROR**: Invalid packet " << static_cast<int>(header._type) << "from session " 
                << session->_id << " in state " << static_cast<int>(sessionState));
                return;
        }

        // 유효한 패킷이라면, 등록된 핸들러를 찾아 호출합니다.
        auto it = _handlers.find(header._type);
        if (it != _handlers.end())
        {
            //LOG("[DISPATCHER] Handler found for type " << static_cast<int>(header._type) << "Calling handler function.");
            it->second(session, stream);
        }
        else
        {
            ERROR("[DISPATCHER] **ERROR**: No handler found for packet type " << static_cast<int>(header._type));
        }
    }
}

