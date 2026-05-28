#pragma once
#include "Actor.h"
#include "GameObject.h"
#include "PhysicsComponent.h"
#include "CharacterControllerComponent.h"
#include "NPCControllerComponent.h"
#include "TransformComponent.h"

namespace PIP
{
	struct NPCSpawnData;
}

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
		common::Vec3 GetSpawnPosition() const { return _spawnPosition; }
		int32_t GetMaxHP() const { return _maxHp; }
		int64_t GetNpcId()          const { return GetId(); }
		NPCType GetNpcType()            const { return _npc_type; }

		common::packet::EntityState GetState() const { return _state; }
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
			return {}; // Invalid ID 반환
		}
		int32_t GetActionId() const { return _actionId; }

		// Setters
		void SetActionId(int32_t action_id) { _actionId = action_id; }
		void SetState(common::packet::EntityState new_state) { _state = new_state; }
		void SetRoom(int room_id) { _room_id = room_id; }
		void SetHP(int new_hp) { _hp = new_hp; }
		void SetLastUpdateTime(std::chrono::steady_clock::time_point t) { _lastUpdateTime = t; }

		// [리스폰] BT 재구성 후 전투/dirty 상태를 한꺼번에 초기화
		void ResetForRespawn();
		

		void SetPosition(common::Vec3 newPosition)
		{
			if (_transform) _transform->SetPosition(newPosition);
			if (_npcController) _npcController->SetPosition(newPosition);
		}

		void SetVelocity(const common::Vec3& v)
		{
			if (_npcController) _npcController->SetVelocity(v);
		}

		void SetRotation(const common::Quat& r)
		{
			if (_transform) _transform->SetRotation(r);
		}

		// [최적화] 캐싱된 포인터를 사용하여 GetComponent 및 맵 조회를 회피
		common::Vec3 GetPosition() const override {
			if (_npcController) return _npcController->GetPosition();
			if (_transform) return _transform->GetPosition();
			return { 0,0,0 };
		}

		void ApplySpawnData(const NPCSpawnData& data);
		const std::vector<common::Vec3>& GetPatrolPoints() const { return _patrolPoints; }

		common::Vec3 GetVelocity() const override {
			if (_npcController) return _npcController->GetVelocity();
			return { 0,0,0 };
		}

		common::Quat GetRotation() const override {
			if (_transform) return _transform->GetRotation();
			return { 0,0,0,1 };
		}

		bool IsDirty() const;
		
		NPCControllerComponent* GetNPCController() const { return _npcController; }
		TransformComponent* GetTransform() const { return _transform; }

		void SyncSentData()
		{
			_lastSentPos = GetPosition();
			_lastSentRot = GetRotation();
			_lastSentState = _state;
			_lastSentActionId = _actionId;
			_lastSentGrabbedById = GetGrabbedById();
			_lastSentGrabSlot = GetGrabSlot();
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
		common::packet::EntityState _state = common::packet::EntityState::IDLE;
		common::packet::EntityState _lastSentState = common::packet::EntityState::IDLE;

		common::Vec3	_lastSentPos = {0,0,0};
		common::Vec4	_lastSentRot = {0,0,0,1};

		std::chrono::steady_clock::time_point _lastUpdateTime;
		std::chrono::steady_clock::time_point _lastSentTime;

		int32_t _actionId = 0; // 현재 진행 중인 행동의 ID (0이면 없음)
		int32_t _lastSentActionId = 0; // 마지막으로 클라이언트에 전송한 행동 ID
		int64_t _lastSentGrabbedById = -1; // [추가]
		int8_t _lastSentGrabSlot = -1; // [추가]
		common::Vec3 _spawnPosition; // 리스폰 위치 저장 (죽었을 때 원래 위치로 돌아가기 위해)
		std::vector<common::Vec3> _patrolPoints; // [추가] 순찰 경로 포인트들

		// [최적화] 매 프레임 GetComponent(8%)를 피하기 위한 캐싱
		NPCControllerComponent* _npcController = nullptr;
		TransformComponent* _transform = nullptr;
		class AIComponent* _aiComponent = nullptr;
		class HitboxComponent* _hitboxComponent = nullptr;
	};

}
