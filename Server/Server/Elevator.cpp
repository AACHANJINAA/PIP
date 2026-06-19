#include "pch.h"
#include "Elevator.h"
#include "JoltSetup.h"

namespace PIP::GAME
{
	Elevator::Elevator(int64_t id, const common::Vec3& startPos, const common::Vec3& endPos, float speed, float waitTime)
		: _startPos(startPos), _endPos(endPos), _speed(speed), _waitTime(waitTime)
	{
		SetId(id);
		_transform = AddComponent<TransformComponent>();
		_transform->SetPosition(startPos);
	}

	void Elevator::InitializePhysics(JPH::PhysicsSystem* physicsSystem, const JPH::Shape* shape)
	{
		_physics = AddComponent<PhysicsComponent>();
		// ELEVATOR 레이어 사용, Kinematic 타입으로 생성
		_physics->CreateBody(physicsSystem, shape, JPH::EMotionType::Kinematic, Layers::ELEVATOR);
	}

	void Elevator::Update(float deltaTime, JPH::TempAllocator* allocator)
	{
		if (!_physics) return;
		using namespace common::VectorHelper;
		common::Vec3 currentPos = GetPosition();
		common::Vec3 targetPos;
		common::Vec3 velocity(0, 0, 0);

		switch (_elevatorInternalState)
		{
		case ElevatorInternalState::IDLE_AT_START:
			_currentWaitTime += deltaTime;
			if (_currentWaitTime >= _waitTime)
			{
				_currentWaitTime = 0.0f;
				_elevatorInternalState = ElevatorInternalState::MOVING_UP;
				_state = common::packet::EntityState::MOVE;
			}
			break;

		case ElevatorInternalState::MOVING_UP:
			targetPos = _endPos;
			{
			   
				common::Vec3 dir = targetPos - currentPos;
				float dist = common::Length(dir);
				if (dist < 0.1f)
				{
					_elevatorInternalState = ElevatorInternalState::IDLE_AT_END;
					_state = common::packet::EntityState::IDLE;
					velocity = common::Vec3(0, 0, 0);
				}
				else
				{
					dir = common::Normalize(dir);
					velocity = dir * _speed;
				}
			}
			break;

		case ElevatorInternalState::IDLE_AT_END:
			// [수정] BossElevator는 위로 한 번 올라오면 다시 내려가지 않도록 고정
			if (GetName() == "BossElevator") {
				velocity = common::Vec3(0, 0, 0);
				break;
			}

			_currentWaitTime += deltaTime;
			if (_currentWaitTime >= _waitTime)
			{
				_currentWaitTime = 0.0f;
				_elevatorInternalState = ElevatorInternalState::MOVING_DOWN;
				_state = common::packet::EntityState::MOVE;
			}
			break;

		case ElevatorInternalState::MOVING_DOWN:
			targetPos = _startPos;
			{
				common::Vec3 dir = targetPos - currentPos;
				float dist = common::Length(dir);
				if (dist < 0.1f)
				{
					_elevatorInternalState = ElevatorInternalState::IDLE_AT_START;
					_state = common::packet::EntityState::IDLE;
					velocity = common::Vec3(0, 0, 0);
				}
				else
				{
					dir = common::Normalize(dir);
					velocity = dir * _speed;
				}
			}
			break;
		}

		_physics->SetVelocity(velocity);
		
		// PhysicsUpdate는 리지드바디의 결과를 트랜스폼에 반영함
		_physics->PhysicsUpdate(deltaTime);
		
		GameObject::Update(deltaTime, allocator);
	}

	common::Vec3 Elevator::GetPosition() const
	{
		return _transform ? _transform->GetPosition() : common::Vec3(0, 0, 0);
	}

	common::Vec3 Elevator::GetVelocity() const
	{
		return _physics ? _physics->GetVelocity() : common::Vec3(0, 0, 0);
	}
}
