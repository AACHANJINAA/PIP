#pragma once
#include "PacketHandlers.h"
#include "server.h"

namespace PIP::packet
{
	using PacketHandler = std::function<void(std::shared_ptr<PIP::server::SESSION>, PIP::packet::PacketStream&)>;

	class PacketManager : public Singleton<PacketManager>
	{
	private:
		std::unordered_map<PacketType, PacketHandler> _handlers; // '주소록'

	public:
		void Initialize();
	   
		void RegisterHandler(PacketType packetId, PacketHandler handler)  // 서버 시작 시, 주소록에 "이 ID는 이 함수가 처리해" 라고 등록
		{
			_handlers[packetId] = std::move(handler);
		}

		void Dispatch(const std::shared_ptr<PIP::server::SESSION>& session, PIP::packet::PacketStream& stream);
		
	};
}
