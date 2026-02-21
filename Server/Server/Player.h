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


		bool ValidateHit(JPH::PhysicsSystem* physics, const JPH::Shape* attackShape, const JPH::RMat44& attackTransform,
		                 uint32_t timestamp, GameObject* attacker, int32_t damage) override;
		void Update(float deltaTime, JPH::TempAllocator* allocator) override;

		//common::Vec3				_position;
		//common::Quat				_rotation;
		short						_hp;
		short						_max_hp;
		short						_level;
		uint32_t					_exp;
		int							_damage;
		common::packet::OBJECT_STATE _state = common::packet::OBJECT_STATE::IDLE;

		// [추가] 위치 보정 관련
		common::Vec3 _lastClientTargetPos = { 0.0f, 0.0f, 0.0f }; // 클라이언트가 마지막으로 보냈다고 우기는 좌표
		common::Vec3 _lastSentPos = { 0.0f, 0.0f, 0.0f };         // 서버에서 클라이언트에게 마지막으로 확정해서 보낸 좌표

		//JPH::BodyID _physicsBodyID;

	private:
		float _hitCooldown = 0.0f;
		int64_t _owner_id; // 이 플레이어를 소유한 세션 ID	
	};
}
