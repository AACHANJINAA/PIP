#include "stdafx.h"
#include "PhysicsManager.h"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include "PhysicsColliderComponent.h"
#include "GameObject.h"

bool PhysicsManager::initialize()
{
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

    return true;
}
void PhysicsManager::update(float deltaTime)
{
    if (!_physicsSystem) return;

    //static int logCounter = 0;
    //if (logCounter++ % 600 == 0) { // 10초마다
    //    std::cout << "[Physics] Active Bodies: " << _physicsSystem->GetNumActiveBodies(JPH::EBodyType::RigidBody)
    //        << std::endl;
    //}


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