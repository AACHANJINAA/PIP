#pragma once

namespace PIP
{
    class NPC;
    namespace server
    {
        class Room;
    }
}

namespace PIP
{
    class AIManager : public Singleton<AIManager>
    {
        friend class Singleton<AIManager>;
    private:
        AIManager() = default;
        ~AIManager() override = default;

    public:
        // [수정] AI 로직만 처리. 어떤 방의 어떤 NPC인지 명시적으로 받음.
        void UpdateNPC(server::Room* room, int npcId);

        // [추가] 중복되지 않는 NPC ID를 발급
        int GetNewNpcId() { return _next_NPC_id.fetch_add(1); }

    private:
        // NPC ID 발급기. 플레이어와 겹치지 않도록 높은 수에서 시작
        std::atomic<int> _next_NPC_id = 20000;
    };
}


