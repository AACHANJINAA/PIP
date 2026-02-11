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
		Player(long long owner_id);
		~Player() override = default;

		void SetPosition(const common::Vec3& pos)
		{
			if (auto tc = GetComponent<GAME::TransformComponent>())
			{
				tc->SetPosition(pos);
			}
			// 물리 엔진이 있다면 거기도 반영
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

		// [추가] 패킷 최적화 및 오차 보정용 Getter/Setter
		void SetLastClientTargetPos(const common::Vec3& pos) { _lastClientTargetPos = pos; }
		common::Vec3 GetLastClientTargetPos() const { return _lastClientTargetPos; }

		void SetLastSentPos(const common::Vec3& pos) { _lastSentPos = pos; }
		common::Vec3 GetLastSentPos() const { return _lastSentPos; }


		bool ValidateHit(JPH::PhysicsSystem* physics, const JPH::Shape* attackShape, const JPH::RMat44& attackTransform,
		                 uint32_t timestamp, GameObject* attacker, int32_t damage) override;
		void Update(float deltaTime, JPH::TempAllocator* allocator) override;

		//common::Vec3				_position;
		//common::Quat				_rotation;
		std::string					_name;
		short						_hp;
		short						_max_hp;
		short						_level;
		uint32_t					_exp;
		int							_damage;
		common::packet::OBJECT_STATE _state = common::packet::OBJECT_STATE::IDLE;

		// [추가] 상태 추적 변수
		common::Vec3 _lastClientTargetPos = { 0.0f, 0.0f, 0.0f }; // 클라이언트가 마지막으로 가겠다고 한 좌표
		common::Vec3 _lastSentPos = { 0.0f, 0.0f, 0.0f };         // 서버가 클라에게 마지막으로 확정해서 보낸 좌표

		//JPH::BodyID _physicsBodyID;

	private:
		float _hitCooldown = 0.0f;
		long long _owner_id; // 이 플레이어의 소유자 세션 ID	
	};
}


