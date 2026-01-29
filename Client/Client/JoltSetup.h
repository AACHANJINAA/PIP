// Client/Client/JoltSetup.h
#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace PIP
{
    namespace Layers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0; // 지형 (클라에선 레이캐스트용 등으로 사용 가능)
        static constexpr JPH::ObjectLayer MOVING = 1;     // 플레이어, NPC (피격체)
        static constexpr JPH::ObjectLayer SENSOR = 2;     // 무기 (공격체)
        static constexpr JPH::ObjectLayer NUM_LAYERS = 3;
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
        }

        virtual JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
        virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
            return
                _objectToBroadPhase[inLayer];
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
            default: return false;
            }
        }
    };

    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
        {
            // [최적화] 클라이언트 물리 엔진은 오직 '무기(SENSOR)'와 '대상(MOVING)' 사이의 충돌만 체크합니다.
            // 캐릭터끼리 부딪히거나, 캐릭터가 벽에 막히는 등의 물리 시뮬레이션은 서버가 수행하므로 여기서는 모두 무시합니다.
        	if (inObject1 == Layers::SENSOR && inObject2 == Layers::MOVING) return true;
            if (inObject1 == Layers::MOVING && inObject2 == Layers::SENSOR) return true;

            return false; // 그 외 모든 물리 충돌 무시 (반발력 발생 방지)
        }
    };
}