#pragma once
#include "JoltSetup.h"
#include "../../Common/Vector3.h"
namespace PIP
{
	class PhysicsManager : Singleton<PhysicsManager> {
		friend class Singleton<PhysicsManager>;
    public:
        void Initialize();
        void Update(float deltaTime);
        void Shutdown();

        // 지형 생성 함수 (MapDataManager에서 호출)
        void CreateTerrain(const float* heightData,
            int width, int height, float scaleX, float scaleZ,
            float minHeight, float maxHeight);

        PhysicsSystem* GetPhysicsSystem() { return &_physicsSystem; }
        BodyInterface& GetBodyInterface() { return _physicsSystem.GetBodyInterface(); }
    private:
        PhysicsManager() = default;

        // Jolt 핵심 객체들
        TempAllocatorImpl*                  _tempAllocator = nullptr;
        JobSystemThreadPool*                _jobSystem = nullptr;
        PhysicsSystem                       _physicsSystem;

        // 필터 객체들
        BPLayerInterfaceImpl                _bpLayerInterface;
        ObjectVsBroadPhaseLayerFilterImpl   _objVsBpLayerFilter;
        ObjectLayerPairFilterImpl           _objLayerPairFilter;

        // 리스너 (필요시 구현)
        BodyActivationListener*             _bodyActivationListener = nullptr;
        ContactListener*                    _contactListener = nullptr;
	};
}

