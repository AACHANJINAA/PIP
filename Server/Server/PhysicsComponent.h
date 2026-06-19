#pragma once
#include "Component.h"
#include "../../Common/JoltHelper.h"


namespace PIP::GAME
{
	class PhysicsComponent : public Component
	{
	public:
		PhysicsComponent(GameObject* owner) : Component(owner) {}
		~PhysicsComponent() override;
		

		// Jolt Body 생성 및 초기화
		void CreateBody(JPH::PhysicsSystem* physicsSystem, const JPH::Shape* shape,
		                JPH::EMotionType motionType, JPH::ObjectLayer layer, JPH::Vec3 positionOffset = JPH::Vec3::sZero(), float mass = 100.0f);
		

		// 물리 세계의 위치를 TransformComponent로 복사
		void PhysicsUpdate(float deltaTime) override;

		// 속도 제어 함수
		void SetVelocity(const common::Vec3& velocity);
		

		common::Vec3 GetVelocity() const;

		JPH::BodyID GetBodyID() const { return _bodyID; }
		JPH::Shape::ShapeToIDMap::value_type::first_type GetShape();

	private:
		JPH::PhysicsSystem* _physicsSystem = nullptr;
		JPH::BodyID			_bodyID;
		JPH::Vec3			_positionOffset = JPH::Vec3::sZero();
	};
}