#include "pch.h"
#include "PhysicsComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
namespace PIP
{
	PhysicsComponent::~PhysicsComponent()
	{
		if (_physicsSystem && !_bodyID.IsInvalid())
		{
			JPH::BodyInterface& bodyInterface = _physicsSystem->GetBodyInterface();
			bodyInterface.RemoveBody(_bodyID);
			bodyInterface.DestroyBody(_bodyID);
		}
	}

	void PhysicsComponent::CreateBody(JPH::PhysicsSystem* physicsSystem, const JPH::Shape* shape,
	                                  JPH::EMotionType motionType, JPH::ObjectLayer layer)
	{
		_physicsSystem = physicsSystem;
		if (!_physicsSystem || !shape) return;

		JPH::RVec3 startPos(0, 0, 0);
		JPH::Quat startRot = JPH::Quat::sIdentity();

		auto transform = GetComponent<TransformComponent>();
		if (transform)
		{
			startPos = Utils::ToJolt(transform->GetPosition());
			startRot = Utils::ToJolt(transform->GetRotation());
		}

		JPH::BodyCreationSettings settings(shape, startPos, startRot, motionType, layer);
		settings.mAllowedDOFs = JPH::EAllowedDOFs::All;

		if (motionType == JPH::EMotionType::Dynamic)
		{
			settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
			settings.mMassPropertiesOverride.mMass = 100.0f;
		}

		JPH::BodyInterface& bodyInterface = _physicsSystem->GetBodyInterface();
		JPH::Body* body = bodyInterface.CreateBody(settings);
		if (body)
		{
			_bodyID = body->GetID();
			bodyInterface.AddBody(_bodyID, JPH::EActivation::Activate);
			body->SetUserData(reinterpret_cast<JPH::uint64>(GetOwner()));
		}
	}

	void PhysicsComponent::PhysicsUpdate(float deltaTime)
	{
		if (!_physicsSystem || _bodyID.IsInvalid()) return;

		JPH::BodyInterface& bodyInterface = _physicsSystem->GetBodyInterface();
		JPH::RVec3 joltPos = bodyInterface.GetCenterOfMassPosition(_bodyID);
		JPH::Quat joltRot = bodyInterface.GetRotation(_bodyID);

		auto transform = GetComponent<TransformComponent>();
		if (transform)
		{
			transform->SetPosition(joltPos);
			transform->SetRotation(joltRot);
		}
	}

	void PhysicsComponent::SetVelocity(const common::Vec3& velocity)
	{
		if (!_physicsSystem || _bodyID.IsInvalid()) return;

		JPH::BodyInterface& bodyInterface = _physicsSystem->GetBodyInterface();
		JPH::Vec3 v(velocity.x, velocity.y, velocity.z);

		bodyInterface.SetLinearVelocity(_bodyID, v);
		bodyInterface.ActivateBody(_bodyID);
	}

	common::Vec3 PhysicsComponent::GetVelocity() const
	{
		if (!_physicsSystem || _bodyID.IsInvalid()) return { 0, 0, 0 };

		JPH::BodyInterface& bodyInterface = _physicsSystem->GetBodyInterface();
		JPH::Vec3 v = bodyInterface.GetLinearVelocity(_bodyID);

		return { v.GetX(), v.GetY(), v.GetZ() };
	}
}
