#pragma once
#include "Actor.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "CharacterControllerComponent.h"

namespace PIP::GAME
{
	class Player : public Actor
	{
	public:
		Player(int64_t owner_id);
		~Player() override = default;
		bool ValidateHit(JPH::PhysicsSystem* physics, const JPH::Shape* attackShape, const JPH::RMat44& attackTransform,
		                 uint32_t timestamp, GameObject* attacker, int32_t damage) override;
		void Update(float deltaTime, JPH::TempAllocator* allocator) override;
		void PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator) override;
		void init(int64_t id);

		int64_t GetId() const override { return _owner_id; } 

		void SetPosition(const common::Vec3& pos)
		{
			if (auto tc = GetComponent<GAME::TransformComponent>())
			{
				tc->SetPosition(pos);
			}
			// 물리 컴포넌트가 있다면 동기화도 반영
			if (auto cc = GetComponent<GAME::CharacterControllerComponent>())
			{
				cc->SetPosition(pos);
			}
		}

		void SetRotation(const common::Quat& rot)
		{
			auto tc = GetComponent<TransformComponent>();
			if (tc)
			{
				tc->SetRotation(rot);
			}
		}

		// [추가] 패킷 동기화 시 필요한 임시 정보 Getter/Setter
		void SetLastClientTargetPos(const common::Vec3& pos) { _lastClientTargetPos = pos; }
		common::Vec3 GetLastClientTargetPos() const { return _lastClientTargetPos; }

		void SetLastSentPos(const common::Vec3& pos) { _lastSentPos = pos; }
		common::Vec3 GetLastSentPos() const { return _lastSentPos; }

		void SetLastSentState(common::packet::EntityState state) { _lastSentState = state; }
		common::packet::EntityState GetLastSentState() const { return _lastSentState; }



		void SetHP(int hp) override { _hp = hp; }
		int32_t GetHP() const override { return _hp; }

		void SetState(const common::packet::EntityState& state) { _state = state; }
		common::packet::EntityState GetState() const { return _state; }

		void SetActionId(int32_t action_id) { _actionId = action_id; }
		int32_t GetActionId() const { return _actionId; }

		void SetSpeed(float speed) { _speed = speed; }
		float GetSpeed() const { return _speed; }

		void SetLastSentRot(common::Quat rot) { _lastSentRot = rot; }
		const common::Quat& GetLastSentRot() const { return _lastSentRot; }
		bool IsDirty();
		void SyncSentData();
		//common::Vec3				_position;
		//common::Quat				_rotation;
		
		int32_t						_max_hp;
		int32_t						_level;
		int32_t						_exp;
		int32_t						_damage;
		common::packet::EntityState _state = common::packet::EntityState::IDLE;
		int32_t						_actionId = 0; // 현재 액션 ID (애니메이션 트리거용)
		float						_speed = 10.0f; //방향에 곱해줄 속도값

		// [추가] 위치 보정 관련
		common::Vec3 _lastClientTargetPos = { 0.0f, 0.0f, 0.0f }; // 클라이언트가 마지막으로 보냈다고 우기는 좌표
		common::Vec3 _lastSentPos = { 0.0f, 0.0f, 0.0f };         // 서버에서 클라이언트에게 마지막으로 확정해서 보낸 좌표
		common::packet::EntityState _lastSentState = common::packet::EntityState::IDLE; // 서버에서 클라이언트에게 마지막으로 보낸 상태
		int32_t _lastSentActionId = 0; // 서버에서 클라이언트에게 마지막으로 보낸 액션 ID
		common::Quat _lastSentRot = { 0,0,0,1 }; // 서버에서 클라이언트에게 마지막으로 보낸 회전

	private:
		float _hitCooldown = 0.0f;
		int64_t _owner_id; // 이 플레이어를 소유한 세션 ID	
		int32_t	_hp;
	};
}
