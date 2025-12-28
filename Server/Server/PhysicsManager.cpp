#include "pch.h"
#include "PhysicsManager.h"

namespace PIP
{
	void PhysicsManager::Initialize()
	{
        // 1. Jolt 팩토리 및 타입 등록
        RegisterDefaultAllocator();
        Factory::sInstance = new Factory();
        RegisterTypes();

        // 2. 메모리 할당기 및 잡 시스템 생성
        _tempAllocator = new TempAllocatorImpl(10 * 1024 * 1024); // 10MB
        _jobSystem = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrenc - 1);

        // 3. 물리 시스템 초기화 (MaxBodies, NumBodyMutexes, MaxBodyPairs, MaxContactConstraints)
        const uint cMaxBodies = 1024;
        const uint cNumBodyMutexes = 0;
        const uint cMaxBodyPairs = 1024;
        const uint cMaxContactConstraints = 1024;

        _physicsSystem.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
            _bpLayerInterface, _objVsBpLayerFilter, _objLayerPairFilter);
	}
	void PhysicsManager::Update(float deltaTime) {}
	void PhysicsManager::Shutdown() {}

	void PhysicsManager::CreateTerrain(const float* heightData, int width, int height, float scaleX, float scaleZ,
		float minHeight, float maxHeight) {}
}
