#pragma once
#include "GameObject.h"
#include "PhysicsComponent.h"
#include "TransformComponent.h"

namespace PIP::GAME
{
	// NPC의 상태를 나타내는 열거형
	enum class NPCState : uint8_t
	{
		IDLE,
		MOVING,
		ATTACKING,
		DEAD
	};

	class NPC : public GameObject
	{
	public:
		NPC(int npc_id, int npc_type, int room_id, common::Vec3 position, int32_t hp);
		~NPC() override;

		// Getters
		int GetNpcId()              const    { return GetId(); }
		int GetNpcType()            const    { return _npc_type; }


		common::Vec3 GetPosition()  const
		{
			auto tc = const_cast<NPC*>(this)->GetComponent<TransformComponent>();
			return tc ? tc->GetPosition() : common::Vec3{ 0,0,0 };
		}
		NPCState GetState()         const    { return _state; }
		int GetRoomId()             const    { return _room_id; }
		int32_t GetHP()             const    { return _hp; }
		common::Vec3 GetVelocity()  const
		{
			auto pc = const_cast<NPC*>(this)->GetComponent<PhysicsComponent>();
			return pc ? pc->GetVelocity() : common::Vec3{ 0,0,0 };
		}
		common::Vec4 GetRotation()  const
		{
			auto tc = const_cast<NPC*>(this)->GetComponent<TransformComponent>();
			return tc ? tc->GetRotation() : common::Vec4{ 0,0,0,1 };
		}
		std::chrono::steady_clock::time_point GetLastUpdateTime() const { return _lastUpdateTime; }

		JPH::BodyID GetBodyID() const
		{
			auto pc = const_cast<NPC*>(this)->GetComponent<PhysicsComponent>();
			return pc ? pc->GetBodyID() : JPH::BodyID();
		}

		// Setters
		void SetPosition(common::Vec3 newPosition)
		{ 
			if (auto tc = GetComponent<TransformComponent>())
			{
				tc->SetPosition(newPosition);
			}
		}
		void SetState(NPCState newState)                { _state = newState; }
		void SetRoom(int room_id)                       { _room_id = room_id; }
		void SetHP(int new_hp)                          { _hp = new_hp; }
		void SetVelocity(const common::Vec3& v)
		{
			if (auto pc = GetComponent<PhysicsComponent>())
			{
				pc->SetVelocity(v);
			}
		}

		void SetRotation(const common::Quat& r)
		{
			if (auto tc = GetComponent<TransformComponent>()) tc->SetRotation(r);
		}
		void SetLastUpdateTime(std::chrono::steady_clock::time_point t) { _lastUpdateTime = t; }
	private:
		int32_t         _npc_type;
		int32_t         _room_id;
		int32_t         _hp;
		NPCState        _state = NPCState::IDLE;
		std::chrono::steady_clock::time_point _lastUpdateTime;
	};

}
