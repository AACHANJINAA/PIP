#pragma once
#include "CharacterControllerComponent.h"

namespace PIP::GAME
{
	class TransformComponent;
    class NPCControllerComponent : public CharacterControllerComponent {
    public:
        using CharacterControllerComponent::CharacterControllerComponent;

        // 초기화 시 컴포넌트 캐싱 (OnAdd 또는 별도 Init에서 호출)
        void Initialize(JPH::PhysicsSystem* system, float height, float radius) override;
        void Initialize() override {}
        void PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator) override;
        void LightPhysicsUpdate(float deltaTime); // LOD용 경량 물리
        void SetVelocity(const common::Vec3& velocity) { _aiVelocity = velocity; }

	private:
        common::Vec3 _aiVelocity = { 0,0,0 };
        float _verticalVelocity = 0.0f;

        // 캐싱된 컴포넌트 포인터 (GetComponent 오버헤드 8% 제거)
        TransformComponent* _cachedTransform = nullptr;
    };
}
