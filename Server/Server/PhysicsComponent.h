#pragma once
#include "Component.h"
#include "JoltHelper.h"


namespace PIP
{
	class PhysicsComponent : public Component
	{
	public:
		PhysicsComponent(GameObject* owner) : Component(owner) {}
		~PhysicsComponent() override;
		

		// Jolt Body 생성 및 초기화
		void CreateBody(JPH::PhysicsSystem* physicsSystem, const JPH::Shape* shape,
			JPH::EMotionType motionType, JPH::ObjectLayer layer);
		

		// 물리 세계의 위치를 TransformComponent로 복사
		void PhysicsUpdate(float deltaTime) override;

		// 속도 제어 함수
		void SetVelocity(const common::Vec3& velocity);
		

		common::Vec3 GetVelocity() const;
		

		JPH::BodyID GetBodyID() const { return _bodyID; }

	private:
		JPH::PhysicsSystem* _physicsSystem = nullptr;
		JPH::BodyID _bodyID;
	};
}