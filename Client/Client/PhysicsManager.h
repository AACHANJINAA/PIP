#pragma once
#include "JoltSetup.h"
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include "../../Common/TerrainData.h" // TerrainData 참조 추가
#ifdef _DEBUG
static void TraceImpl(const char* inFMT, ...) {
	va_list list; va_start(list, inFMT);
	char buffer[1024]; vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);
	OutputDebugStringA(buffer); // 클라이언트는 출력창에 로그
}
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine)
{
	// Assert 발생 시 중단점
	__debugbreak();
	return true;
}
#endif
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

	// [추가] 물리 지형 생성 함수
	void create_physics_terrain(const common::TerrainData& terrainData);
	JPH::TempAllocator* get_temp_allocator() const { return _tempAllocator; }
	JPH::PhysicsSystem* get_physics_system() const { return _physicsSystem; }
	JPH::BodyInterface& get_body_interface() const { return _physicsSystem->GetBodyInterface(); }
private:
	JPH::PhysicsSystem* _physicsSystem = nullptr;
	JPH::TempAllocator* _tempAllocator = nullptr;
	JPH::JobSystem*		_jobSystem = nullptr;

	PIP::BPLayerInterfaceImpl               _bpLayerInterface;
	PIP::ObjectVsBroadPhaseLayerFilterImpl  _objVsBpLayerFilter;
	PIP::ObjectLayerPairFilterImpl          _objLayerPairFilter;

	MyContactListener                       _contactListener;

	Concurrency::concurrent_queue<CollisionEvent> _collisionQueue;

	JPH::BodyID _terrainBodyID; // [추가] 지형 바디 ID 저장
};
