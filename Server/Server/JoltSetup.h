#pragma once

namespace PIP
{
    // 레이어 정의
    namespace Layers
    {
        // ObjectLayer = uint16
        static constexpr JPH::ObjectLayer NON_MOVING = 0; // 지형, 건물
        static constexpr JPH::ObjectLayer MOVING = 1; // 플레이어, 몬스터
		static constexpr JPH::ObjectLayer NPC = 2; // 충돌은 안하지만 닿으면 이벤트 발생

        //TODO: 나중에 추가할 가능성이 있는 레이어들
        //static constexpr ObjectLayer TRIGGER = 2; // 닿으면 이벤트만 발생, 몸이 통과됨)
        //static constexpr ObjectLayer PLAYER = 3;
        //static constexpr ObjectLayer MONSTER = 4;

		// ObjectLayer 개수
        static constexpr JPH::ObjectLayer NUM_LAYERS = 3;
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
        JPH::BroadPhaseLayer _objectToBroadPhase[Layers::NUM_LAYERS]; // ObjectLayer -> BroadPhaseLayer 매핑 배열
    };

	// ObjectLayer와 BroadPhaseLayer 간의 충돌 여부를 결정하는 필터 클래스 구현
    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            switch (inLayer1) // 물체의 종류
            {
            case Layers::NON_MOVING:
                // 1. NON_MOVING 물체는 지형과 충돌하면 안되니깐
				// MOVING 레이어일때만 true 반환 (지형vs지형 = false)
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
				// 2. MOVING 물체는 모든 레이어와 충돌
                return true;
			case Layers::NPC:
                return true;
            default:
                return false;
            }
        }
    };

	// ObjectLayer 간의 충돌 여부를 결정하는 필터 클래스 구현
    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
        // 1단계(BroadPhase Filter) : 물체가 속한 큰 바구니(Layer)끼리 비교. (매우 빠름)
        // 2단계(ObjectLayer Pair Filter) : 실제로 가까이 붙은 두 개별 객체끼리 비교. (정밀함)
    public:
        // 실제로 두 물체가 물리적으로 팅겨나가야 하는가?
        virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
        {
            switch (inObject1)
            {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING || inObject2 == Layers::NPC;
            case Layers::MOVING:
                return true;
            case Layers::NPC:
                // [핵심] NPC는 NPC끼리 충돌하지 않음 (inObject2가 NPC면 false)
                return inObject2 == Layers::NON_MOVING || inObject2 == Layers::MOVING;
            default:
                return false;
            }
        }
    };

    // 4. 리스너 (일단 비워둠)
    // 잠자기 깨어나기 알람
    class MyBodyActivationListener : public JPH::BodyActivationListener
    {
    public:
        // 물체가 멈춰 있다가 누가 건드려서 움직이기 시작할때 호출
        virtual void OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override {}
		// 물체가 움직이다가 완전히 멈춰서 잠들때 호출
        virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override {}
    };

    // 충돌 알림 게임 알림 **게임 로직의 핵심**
    // 실제 충돌이 일어났을때 호출됨 
    class MyContactListener : public JPH::ContactListener
    {
    public:
        // 진짜 충돌을 해도 되는지 검증
        // 예시로 아군같은 경우는 무시될것
        virtual JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, 
                                                      const JPH::CollideShapeResult& inCollisionResult) override
        {
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        // 두 물체가 처음 닿았을 때 호출됩니다.
        virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold,
                                    JPH::ContactSettings& ioSettings) override {}

        // 계속 닿아있을 때 (비비고 있을 때) 호출됩니다.
        virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold,
                                        JPH::ContactSettings& ioSettings) override {}

        // 떨어졌을 때 호출됩니다.
        virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {}
    };
}
