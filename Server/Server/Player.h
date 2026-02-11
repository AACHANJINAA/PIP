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

		common::Vec3 GetPosition() const
		{
			// const_cast는 GameObject::GetComponent가 const 버전을 지원하지 않을 경우 필요
			auto tc = const_cast<Player*>(this)->GetComponent<GAME::TransformComponent>();
			return tc ? tc->GetPosition() : common::Vec3{ 0,0,0 };;
		}
		common::Quat GetRotation() const
		{
			auto tc = const_cast<Player*>(this)->GetComponent<TransformComponent>();
			return tc ? tc->GetRotation() : common::Quat{ 0,0,0,1 };
		}

		bool ValidateHit(JPH::PhysicsSystem* physics, const JPH::Shape* attackShape, const JPH::RMat44& attackTransform,
		                 uint32_t timestamp, GameObject* attacker, int32_t damage) override;

		//common::Vec3				_position;
		//common::Quat				_rotation;
		std::string					_name;
		short						_hp;
		short						_max_hp;
		short						_level;
		uint32_t					_exp;
		int							_damage;
		common::packet::OBJECT_STATE _state = common::packet::OBJECT_STATE::IDLE;

		//JPH::BodyID _physicsBodyID;

	private:
		long long _owner_id; // 이 플레이어의 소유자 세션 ID	
	};
}


