#include "pch.h"
#include "AIManager.h"

#include <random>
#include "NPC.h"
#include "server.h"
#include "Timer.h"

namespace PIP
{
    // [수정] UpdateNPC 함수
    void AIManager::UpdateNPC(server::Room* room, int npcId)
    {
        if (room == nullptr) return;

        NPC* npc = room->GetNPC(npcId);
        if (npc == nullptr) return;

        // --- 1. 랜덤 이동 로직 ---
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(-1.0, 1.0);

        common::Vec3 oldPos = npc->GetPosition();
        common::Vec3 newPos = oldPos;
        newPos.x += static_cast<float>(dis(gen)) * 2.0f;
        newPos.z += static_cast<float>(dis(gen)) * 2.0f;

        // TODO: 맵 경계나 벽 충돌 체크 로직 추가 필요
        npc->SetPosition(newPos);

        // --- 2. 이동 패킷 브로드캐스팅 ---
        common::packet::SC_PACKET_NPC_MOVE movePacket;
        movePacket._size = sizeof(movePacket);
        movePacket._type = common::packet::PacketType::S2C_NPC_MOVE;
        movePacket.npcId = npcId;
        movePacket.position = newPos;

        // [수정] 파라미터로 받은 room에만 브로드캐스팅
        room->Broadcast(reinterpret_cast<const char*>(&movePacket), sizeof(movePacket));

        // --- 3. 다음 업데이트 예약 ---
        // 200ms (0.2초) 뒤에 이 NPC의 AI를 다시 실행하도록 타이머에 등록
		server::Timer::Instance()->AddTimerJob(std::chrono::milliseconds(200), [this, room, npcId]() {
            this->UpdateNPC(room, npcId);
        }); 
    }
}
