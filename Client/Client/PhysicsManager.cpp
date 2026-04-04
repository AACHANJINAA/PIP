#include "stdafx.h"
#include "PhysicsManager.h"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h> // 추가
#include "PhysicsColliderComponent.h"
#include "GameObject.h"

bool PhysicsManager::initialize()
{

    // 이 줄들을 추가하세요
#ifdef _DEBUG
    JPH::Trace = TraceImpl;
    JPH::AssertFailed = AssertFailedImpl;
#endif
    // Jolt 초기화
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    _tempAllocator = new JPH::TempAllocatorImpl(10LL * 1024 * 1024);
    _jobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
        std::thread::hardware_concurrency() - 1);

    _physicsSystem = new JPH::PhysicsSystem();
    _physicsSystem->Init(1024, 0, 1024, 1024, 
        _bpLayerInterface, _objVsBpLayerFilter, _objLayerPairFilter);

    _physicsSystem->SetContactListener(&_contactListener);

    // 클라이언트 중력 설정 (서버와 동일하게)
    _physicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

    return true;
}
void PhysicsManager::update(float deltaTime)
{
    if (!_physicsSystem) return;

    // 1. 물리 시뮬레이션 수행
    _physicsSystem->Update(deltaTime, 1, _tempAllocator, _jobSystem);

    // 2. 메인 스레드에서 이벤트 큐 처리
    CollisionEvent ev;
    while (_collisionQueue.try_pop(ev)) // 큐가 빌 때까지 반복
    {
        if (auto c1 = ev.obj1->get_component<PhysicsColliderComponent>())
            c1->OnContact(ev.obj2);

        if (auto c2 = ev.obj2->get_component<PhysicsColliderComponent>())
            c2->OnContact(ev.obj1);
    }
}
void PhysicsManager::cleanup()
{
    // 메모리 해제 순서 중요
    delete _physicsSystem;  _physicsSystem = nullptr;
    delete _jobSystem;      _jobSystem = nullptr;
    delete _tempAllocator;  _tempAllocator = nullptr;
    delete JPH::Factory::sInstance; JPH::Factory::sInstance = nullptr;
}

void PhysicsManager::create_physics_terrain(const common::TerrainData& terrainData)
{
    const auto& info = terrainData.GetInfo();
    const auto& heightMap = terrainData.GetHeightData();

    JPH::HeightFieldShapeSettings settings;
    settings.mOffset = JPH::Vec3(info.min_x, 0.0f, info.min_z);

    float dx = (info.max_x - info.min_x) / (info.width - 1);
    float dz = (info.max_z - info.min_z) / (info.height - 1);
    settings.mScale = JPH::Vec3(dx, 1.0f, dz);
    settings.mSampleCount = static_cast<JPH::uint32>(info.width);

    settings.mHeightSamples.resize(heightMap.size());
    for (size_t i = 0; i < heightMap.size(); ++i) {
        settings.mHeightSamples[i] = heightMap[i];
    }

    auto result = settings.Create();
    if (result.HasError()) return;

    // Layers::NON_MOVING은 서버와 동일한 레이어 설정이어야 함
    JPH::BodyCreationSettings bodySettings(result.Get(), JPH::RVec3(0, 0, 0), JPH::Quat::sIdentity(),
        JPH::EMotionType::Static, PIP::Layers::NON_MOVING);

    JPH::BodyInterface& bodyInterface = _physicsSystem->GetBodyInterface();
    _terrainBodyID = bodyInterface.CreateAndAddBody(bodySettings, JPH::EActivation::DontActivate);
}

// -----------------------------------------------------------------------------
// Contact Listener Implementation
// -----------------------------------------------------------------------------

void PhysicsManager::MyContactListener::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2,
	const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
    // UserData에 GameObject 포인터를 저장했다고 가정 (나중에 컴포넌트에서 설정)
    uint64_t userData1 = inBody1.GetUserData();
    uint64_t userData2 = inBody2.GetUserData();

    if (userData1 == 0 || userData2 == 0) return;

    // GameObject 포인터로 복원
    // 주의: 실제 구현 시에는 GameObject ID를 쓰거나, 포인터 유효성을 엄격히 체크해야 안전함
    GameObject* rawObj1 = reinterpret_cast<GameObject*>(userData1);
    GameObject* rawObj2 = reinterpret_cast<GameObject*>(userData2);

    try {
        auto obj1 = rawObj1->shared_from_this();
        auto obj2 = rawObj2->shared_from_this();

        PhysicsManager::instance()->_collisionQueue.push({ obj1, obj2 });
    }
    catch (...) {
        // 혹시라도 죽은 객체라면 무시 (안전장치)
    }
}