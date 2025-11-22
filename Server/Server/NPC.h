#pragma once

namespace PIP
{
    // NPC의 상태를 나타내는 열거형
    enum class NPCState : uint8_t
    {
        IDLE,
        MOVING,
        ATTACKING,
        DEAD
    };

    class NPC
    {
    public:
        NPC(int npc_id, int npc_type, int room_id, common::Vec3 position, int32_t hp);
        ~NPC();

        // Getters
        int GetNpcId() const { return _npc_id; }
        int GetNpcType() const { return _npc_type; }
        common::Vec3 GetPosition() const { return _position; }
        NPCState GetState() const { return _state; }
		int GetRoomId() const { return _room_id; }
		std::string GetName() const { return _name; }
		int32_t GetHP() const { return _hp; }

        // Setters
        void SetPosition(common::Vec3 newPosition) { _position = newPosition; }
        void SetState(NPCState newState) { _state = newState; }
		void SetRoom(int room_id) { _room_id = room_id; }
		void SetName(const std::string& name) { _name = name; }
		void SetHP(int new_hp) { _hp = new_hp; }
    private:
        int32_t         _npc_id;
        int32_t         _npc_type;
        int32_t         _room_id;
        int32_t         _hp;
        std::string     _name;
        common::Vec3    _position;
        NPCState        _state = NPCState::IDLE;
    };
}