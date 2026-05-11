#pragma once

namespace PIP
{
    namespace Layers
    {
        // ObjectLayer = uint16
        static constexpr JPH::ObjectLayer NON_MOVING = 0; // 정적 지형, 건물
        static constexpr JPH::ObjectLayer MOVING = 1;     // 플레이어, 몬스터
        static constexpr JPH::ObjectLayer NPC = 2;        // NPC/AI용 레이어
        static constexpr JPH::ObjectLayer ELEVATOR = 3;   // [추가] 엘리베이터 (지형 통과, 캐릭터만 밀어냄)

        static constexpr JPH::ObjectLayer NUM_LAYERS = 4;
    }

    // BroadPhase 레이어 정의 (성능 최적화용)
    namespace BroadPhaseLayers
    {
        // BroadPhaseLayer = uint8
        static constexpr JPH::BroadPhaseLayer NON_MOVING{ 0 };
        static constexpr JPH::BroadPhaseLayer MOVING{ 1 };


        // BroadPhaseLayer 개수
        static constexpr JPH::uint            NUM_LAYERS{ 2 };
    }

    // Jolt에게 "세상에는 어떤 종류의 BroadPhase 레이어들이 있어?"라고 알려주는 인터페이스(설계도)입니다.
    // BP는 일종의 전역 필터?레이어? 같은 느낌 현재는 움직이는 것과 움직이지 않는 것만 구분
    // 각각 트리로서 오브젝트들을 관리 Quadtree로 관리함
    class BPLayerInterfaceImpl : public JPH::BroadPhaseLayerInterface
    {
    public:
        BPLayerInterfaceImpl()
        {
			// ObjectLayer와 BroadPhaseLayer 매핑 설정
            _objectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            _objectToBroadPhase[Layers::MOVING]     = BroadPhaseLayers::MOVING;
            _objectToBroadPhase[Layers::NPC]        = BroadPhaseLayers::MOVING;
            _objectToBroadPhase[Layers::ELEVATOR]   = BroadPhaseLayers::MOVING;
        }

        virtual JPH::uint GetNumBroadPhaseLayers() const override
        {
			// BroadPhaseLayer 개수 반환
            return BroadPhaseLayers::NUM_LAYERS;
        }

        // 내가 어떤 BP 레이어에 속하는지 반환
        virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            return _objectToBroadPhase[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		// 디버깅용: BroadPhaseLayer 이름 반환
        virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
        {
            switch (static_cast<JPH::BroadPhaseLayer::Type>(inLayer)) {
            case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::NON_MOVING):
                return "NON_MOVING";
            case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::MOVING):
                return "MOVING";
            default:
                return "INVALID";
            }
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
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
            case Layers::NPC:
            case Layers::ELEVATOR:
                return true;
            default:
                return false;
            }
        }
    };

    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override       
        {
            switch (inObject1)
            {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING || inObject2 == Layers::NPC;
            case Layers::MOVING:
                return true;
            case Layers::NPC:
                // [수정] NPC도 엘리베이터와 충돌해야 보스가 엘리베이터에 탈 수 있음
                return inObject2 == Layers::NON_MOVING || inObject2 == Layers::MOVING || inObject2 == Layers::ELEVATOR;
            case Layers::ELEVATOR:
                // 엘리베이터는 MOVING(플레이어) 및 NPC와만 충돌하고 NON_MOVING(지형)과는 충돌하지 않음
                return inObject2 == Layers::MOVING || inObject2 == Layers::NPC;
            default:
                return false;
            }
        }
    };

    class MyBodyActivationListener : public JPH::BodyActivationListener
    {
    public:
        virtual void OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override {}       
        virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override {}     
    };

    class MyContactListener : public JPH::ContactListener
    {
    public:
        virtual JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset,
                                                      const JPH::CollideShapeResult& inCollisionResult) override
        {
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold,
                                    JPH::ContactSettings& ioSettings) override {}

        virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold,
                                        JPH::ContactSettings& ioSettings) override {}

        virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {}
    };
}
