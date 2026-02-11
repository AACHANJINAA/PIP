#pragma once
#include "Actor.h"
#include "GameObject.h"
#include "PhysicsComponent.h"
#include "CharacterControllerComponent.h"
#include "TransformComponent.h"

namespace PIP::GAME
{
	struct NPCAttackConfig {
		JPH::Ref<JPH::Shape> shape;    // 공격 판정 모양 (Sphere, Box, Capsule 등)
		common::Vec3 posOffset;        // NPC 중심으로부터의 오프셋
		float damage;                  // 공격력
		float cooldown;                // 재사용 대기시간
		std::string animationKey;      // (선택) 클라이언트에 보낼 애니메이션 이름/번호
		// 추가 가능: 상태 이상, 넉백 수치 등
	};

	// NPC의 상태를 나타내는 열거형
	enum class NPCState : uint8_t
	{
		IDLE,
		MOVING,
		ATTACKING,
		DEAD
	};

	class NPC : public Actor
	{
	public:
		NPC(int npc_id, int npc_type, int room_id, common::Vec3 position, int32_t hp);
		~NPC() override;

		void SetupBT();
		

		// Getters
		int GetNpcId()              const { return GetId(); }
		int GetNpcType()            const { return _npc_type; }

		common::packet::OBJECT_STATE GetState() const { return _state; }
		int GetRoomId()             const { return _room_id; }
		int32_t GetHP()             const { return _hp; }
		std::chrono::steady_clock::time_point GetLastUpdateTime() const { return _lastUpdateTime; }

		common::Vec3 GetPosition()  const
		{
			auto tc = const_cast<NPC*>(this)->GetComponent<TransformComponent>();
			return tc ? tc->GetPosition() : common::Vec3{ 0,0,0 };
		}
		common::Vec3 GetVelocity()  const
		{
			auto cc = const_cast<NPC*>(this)->GetComponent<CharacterControllerComponent>();
			return cc ? cc->GetVelocity() : common::Vec3{ 0,0,0 };
		}
		common::Vec4 GetRotation()  const
		{
			auto tc = const_cast<NPC*>(this)->GetComponent<TransformComponent>();
			return tc ? tc->GetRotation() : common::Vec4{ 0,0,0,1 };
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
			if (auto cc = GetComponent<CharacterControllerComponent>())
			{
				cc->SetVelocity(v);
			}
		}

		void SetRotation(const common::Quat& r)
		{
			if (auto tc = GetComponent<TransformComponent>())
				tc->SetRotation(r);
		}

		bool IsDirty() const
		{
			common::Vec3 currentPos = GetPosition();
			common::Vec4 currentRot = GetRotation();
			float distSq =	(currentPos.x - _lastSentPos.x) * (currentPos.x - _lastSentPos.x) +
							(currentPos.y - _lastSentPos.y) * (currentPos.y - _lastSentPos.y) +
							(currentPos.z - _lastSentPos.z) * (currentPos.z - _lastSentPos.z);
			if (distSq > 0.0025f)
			{
				return true;
			}

			float rotDiff =	(currentRot.x - _lastSentRot.x) * (currentRot.x - _lastSentRot.x) +
							(currentRot.y - _lastSentRot.y) * (currentRot.y - _lastSentRot.y) +
							(currentRot.z - _lastSentRot.z) * (currentRot.z - _lastSentRot.z) +
							(currentRot.w - _lastSentRot.w) * (currentRot.w - _lastSentRot.w);

			if (rotDiff > 0.01f)
			{
				return true;
			}

			// [추가] 2초 동안 아무 정보도 안 보냈으면 강제로 보냄 (Heartbeat)
			auto now = std::chrono::steady_clock::now();
			if (std::chrono::duration<float>(now - _lastSentTime).count() > 1.0f) {
				return true;
			}

			return false;
		}

		void SyncSentData()
		{
			_lastSentPos = GetPosition();
			_lastSentRot = GetRotation();
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

	private:
		float			_hitCooldown = 0.0f;
		int32_t         _npc_type;
		int32_t         _room_id;
		int32_t         _hp;
		common::packet::OBJECT_STATE _state = common::packet::OBJECT_STATE::IDLE;
		common::Vec3	_lastSentPos = {0,0,0};
		common::Vec4	_lastSentRot = {0,0,0,1};
		std::chrono::steady_clock::time_point _lastUpdateTime;
		std::chrono::steady_clock::time_point _lastSentTime;
	};

}
