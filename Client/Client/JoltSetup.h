#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace PIP
{
    namespace Layers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0; // 정적 (클라이언트에서 레이캐스트나 충돌 시 활용 가능)
        static constexpr JPH::ObjectLayer MOVING = 1;     // 플레이어, NPC (동적객체)
        static constexpr JPH::ObjectLayer SENSOR = 2;     // 트리거 (센서객체)
        static constexpr JPH::ObjectLayer ELEVATOR = 3;   // 엘리베이터 (서버 동기화용)
        static constexpr JPH::ObjectLayer NUM_LAYERS = 4;
    }

    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING{ 0 };
        static constexpr JPH::BroadPhaseLayer MOVING{ 1 };
        static constexpr JPH::uint NUM_LAYERS{ 2 };
    }

    class BPLayerInterfaceImpl : public JPH::BroadPhaseLayerInterface
    {
    public:
        BPLayerInterfaceImpl() {
            _objectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            _objectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
            _objectToBroadPhase[Layers::SENSOR] = BroadPhaseLayers::MOVING;
            _objectToBroadPhase[Layers::ELEVATOR] = BroadPhaseLayers::MOVING;
        }

        virtual JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
        virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
            return _objectToBroadPhase[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
            return "Default";
        }
#endif
    private:
        JPH::BroadPhaseLayer _objectToBroadPhase[Layers::NUM_LAYERS];
    };

    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            switch (inLayer1)
            {
            case Layers::MOVING: return true;
            case Layers::SENSOR: return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::ELEVATOR: return inLayer2 == BroadPhaseLayers::MOVING;
            default: return false;
            }
        }
    };

    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
        {
            // 플레이어(MOVING)와 지형(NON_MOVING) 충돌 허용
            if ((inObject1 == Layers::MOVING && inObject2 == Layers::NON_MOVING) ||
                (inObject2 == Layers::MOVING && inObject1 == Layers::NON_MOVING)) return true;

            // 엘리베이터(ELEVATOR)와 플레이어(MOVING) 충돌 허용
            if ((inObject1 == Layers::ELEVATOR && inObject2 == Layers::MOVING) ||
                (inObject2 == Layers::ELEVATOR && inObject1 == Layers::MOVING)) return true;

            // NPC(SENSOR)와 플레이어(MOVING) 충돌 허용
            if ((inObject1 == Layers::SENSOR && inObject2 == Layers::MOVING) ||
                (inObject2 == Layers::SENSOR && inObject1 == Layers::MOVING)) return true;

            return false;
        }
    };
}
