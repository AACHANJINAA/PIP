#pragma once
#include "Actor.h"
#include "GameObject.h"
#include "PhysicsComponent.h"
#include "CharacterControllerComponent.h"
#include "NPCControllerComponent.h"
#include "TransformComponent.h"

namespace PIP::GAME
{
	// NPC의 상태를 나타내는 열거형
	using NPCType = common::packet::NPCType;
	
	class NPC : public Actor
	{
	public:
		NPC(int64_t npc_id, NPCType npc_type, int room_id, common::Vec3 position, int32_t hp);
		~NPC() override;

		virtual void SetupBT();
		

		// Getters
		int64_t GetNpcId()          const { return GetId(); }
		NPCType GetNpcType()            const { return _npc_type; }

		common::packet::OBJECT_STATE GetState() const { return _state; }
		int GetRoomId()             const { return _room_id; }
		int32_t GetHP()             const override { return _hp; }
		std::chrono::steady_clock::time_point GetLastUpdateTime() const { return _lastUpdateTime; }

		bool is_boss() const {
			return _npc_type == common::packet::NPCType::Tainer;
		}

		// [수정] 컴포넌트 종류에 상관없이 실제 사용하는 Jolt Shape 반환
		const JPH::Shape* GetPhysicsShape() const {
			// 1. 일반 리지드 바디 확인
			if (auto pc = const_cast<NPC*>(this)->GetComponent<PhysicsComponent>())
				return pc->GetShape(); // PhysicsComponent에 GetShape()가 있다고 가정

			// 2. 버추얼 캐릭터 확인
			if (auto cc = const_cast<NPC*>(this)->GetComponent<CharacterControllerComponent>())
				return cc->GetShape();

			return nullptr;
		}
		// [수정] 리지드 바디 ID 반환 (리지드 바디 기반일 때만 유효)
		JPH::BodyID GetBodyID() const {
			if (auto pc = const_cast<NPC*>(this)->GetComponent<PhysicsComponent>())
				return pc->GetBodyID();
			return JPH::BodyID(); // Invalid ID 반환
		}

		// Setters
		void SetState(common::packet::OBJECT_STATE new_state) { _state = new_state; }
		void SetRoom(int room_id) { _room_id = room_id; }
		void SetHP(int new_hp) { _hp = new_hp; }
		void SetLastUpdateTime(std::chrono::steady_clock::time_point t) { _lastUpdateTime = t; }

		void SetPosition(common::Vec3 newPosition)
		{
			if (auto tc = GetComponent<TransformComponent>())
			{
				tc->SetPosition(newPosition);
			}
		}

		void SetVelocity(const common::Vec3& v)
		{
			if (auto cc = GetComponent<NPCControllerComponent>())
			{
				cc->SetVelocity(v);
			}
		}

		void SetRotation(const common::Quat& r)
		{
			if (auto tc = GetComponent<TransformComponent>())
				tc->SetRotation(r);
		}

		bool IsDirty() const;
		

		void SyncSentData()
		{
			_lastSentPos = GetPosition();
			_lastSentRot = GetRotation();
			_lastSentState = _state;
			_lastSentTime = std::chrono::steady_clock::now(); // 시간 갱신
		}
		// [모듈화] 공격 검증 및 피격 처리 통합 함수
		bool ValidateHit(JPH::PhysicsSystem* physics,
		                 const JPH::Shape* attackShape,
		                 const JPH::RMat44& attackTransform,
		                 uint32_t timestamp,
		                 GameObject* attacker,
		                 int32_t damage) override;
		void Update(float deltaTime, JPH::TempAllocator* allocator) override;

	protected:
		float			_hitCooldown = 0.0f;
		NPCType         _npc_type;
		int32_t         _room_id;
		int32_t         _hp;
		int32_t			_maxHp;
		common::packet::OBJECT_STATE _state = common::packet::OBJECT_STATE::IDLE;
		common::packet::OBJECT_STATE _lastSentState = common::packet::OBJECT_STATE::IDLE;
		common::Vec3	_lastSentPos = {0,0,0};
		common::Vec4	_lastSentRot = {0,0,0,1};
		std::chrono::steady_clock::time_point _lastUpdateTime;
		std::chrono::steady_clock::time_point _lastSentTime;

	};

}
