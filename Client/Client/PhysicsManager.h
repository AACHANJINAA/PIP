#pragma once
#include "JoltSetup.h"
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>

class GameObject;

struct CollisionEvent {
	std::shared_ptr<GameObject> obj1;
	std::shared_ptr<GameObject> obj2;
};

class PhysicsManager : public Singleton<PhysicsManager>{
	friend class Singleton<PhysicsManager>;
public:
	class MyContactListener : public JPH::ContactListener {
		virtual JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2,
			JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override
		{
			// 모든 충돌을 허용하지만, 센서(Trigger)로 동작하게 유도
			return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
		}
		void OnContactAdded(const JPH::Body& inBody1,
			const JPH::Body& inBody2, 
			const JPH::ContactManifold& inManifold, 
			JPH::ContactSettings& ioSettings) override;

		void OnContactPersisted(const JPH::Body& inBody1,
			const JPH::Body& inBody2,
			const JPH::ContactManifold& inManifold, 
			JPH::ContactSettings& ioSettings) override {}

		void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {}
	};
public:
	bool initialize();
	void update(float deltaTime);
	void cleanup();

	JPH::PhysicsSystem* get_physics_system() { return _physicsSystem; }
	JPH::BodyInterface& get_body_interface() { return _physicsSystem->GetBodyInterface(); }
private:
	JPH::PhysicsSystem* _physicsSystem = nullptr;
	JPH::TempAllocator* _tempAllocator = nullptr;
	JPH::JobSystem*		_jobSystem = nullptr;

	PIP::BPLayerInterfaceImpl               _bpLayerInterface;
	PIP::ObjectVsBroadPhaseLayerFilterImpl  _objVsBpLayerFilter;
	PIP::ObjectLayerPairFilterImpl          _objLayerPairFilter;

	MyContactListener                       _contactListener;

	Concurrency::concurrent_queue<CollisionEvent> _collisionQueue;
};
