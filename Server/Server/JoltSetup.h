#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

using namespace JPH;

namespace PIP
{
    // 레이어 정의
    namespace Layers
    {
        // ObjectLayer = uint16
        static constexpr ObjectLayer NON_MOVING = 0; // 지형, 건물
        static constexpr ObjectLayer MOVING = 1; // 플레이어, 몬스터
        static constexpr ObjectLayer NUM_LAYERS = 2;
    }

    // 레이어 간 충돌 필터 (누가 누구랑 충돌할지)
    class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
    {
    public:
        virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
        {
            switch (inObject1)
            {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING; // 지형은 움직이는 것과만 충돌
            case Layers::MOVING:
                return true; // 움직이는 것은 다 충돌 (지형 + 다른 플레이어)
            default:
                return false;
            }
        }
    };

    // BroadPhase 레이어 정의 (성능 최적화용)
    namespace BroadPhaseLayers
    {
        // BroadPhaseLayer = uint8
        static constexpr BroadPhaseLayer NON_MOVING{ 0 };
        static constexpr BroadPhaseLayer MOVING{ 1 };

        // BroadPhaseLayer 개수
        static constexpr uint            NUM_LAYERS{ 2 };
    }

    // ObjectLayer -> BroadPhaseLayer 매핑
    class BPLayerInterfaceImpl : public BroadPhaseLayerInterface
    {
    public:
        BPLayerInterfaceImpl()
        {
            mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            mObjectToBroadPhase[Layers::MOVING]     = BroadPhaseLayers::MOVING;
        }

        virtual uint GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayers::NUM_LAYERS;
        }

        virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override
        {
            return mObjectToBroadPhase[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
        {
            switch (static_cast<BroadPhaseLayer::Type>(inLayer)) {
            case static_cast<BroadPhaseLayer::Type>(BroadPhaseLayers::NON_MOVING):
                return "NON_MOVING";
            case static_cast<BroadPhaseLayer::Type>(BroadPhaseLayers::MOVING):
                return "MOVING";
            default:
                return "INVALID";
            }
        }
#endif

    private:
        BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    // BroadPhase 레이어 간 충돌 필터
    class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
    {
    public:
        virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
        {
            switch (inLayer1)
            {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
            }
        }
    };
}
