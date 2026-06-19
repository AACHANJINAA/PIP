#pragma once
#include "Actor.h"
#include "PhysicsComponent.h"
#include "TransformComponent.h"

namespace PIP::GAME
{
	class Elevator : public Actor
	{
	public:
		Elevator(int64_t id, const common::Vec3& startPos, const common::Vec3& endPos, float speed, float waitTime);
		virtual ~Elevator() override = default;

		void InitializePhysics(JPH::PhysicsSystem* physicsSystem, const JPH::Shape* shape);
		void Update(float deltaTime, JPH::TempAllocator* allocator) override;

		// Actor 인터페이스 구현
		int32_t GetHP() const override { return 1000000; }
		common::packet::EntityState GetState() const override { return _state; }
		
		bool ValidateHit(JPH::PhysicsSystem* physics, const JPH::Shape* attackShape, const JPH::RMat44& attackTransform,
						 uint32_t timestamp, GameObject* attacker, int32_t damage) override { return false; }

		common::Vec3 GetPosition() const override;
		common::Vec3 GetVelocity() const override;

		bool IsAtTop() const { return _elevatorInternalState == ElevatorInternalState::IDLE_AT_END; }

	private:
		common::Vec3 _startPos;
		common::Vec3 _endPos;
		float _speed;
		float _waitTime;
		float _currentWaitTime = 0.0f;
		
		enum class ElevatorInternalState {
			IDLE_AT_START,
			MOVING_UP,
			IDLE_AT_END,
			MOVING_DOWN
		};
		ElevatorInternalState _elevatorInternalState = ElevatorInternalState::IDLE_AT_START;
		common::packet::EntityState _state = common::packet::EntityState::IDLE;

		PhysicsComponent* _physics = nullptr;
		TransformComponent* _transform = nullptr;
	};
}
