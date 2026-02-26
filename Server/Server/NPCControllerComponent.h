#pragma once
#include "CharacterControllerComponent.h"

namespace PIP::GAME
{
    class NPCControllerComponent : public CharacterControllerComponent {
    public:
        using CharacterControllerComponent::CharacterControllerComponent;
        void PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator) override;
        void LightPhysicsUpdate(float deltaTime); // LOD용 경량 물리
        void SetVelocity(const common::Vec3& velocity) { _aiVelocity = velocity; }
    private:
        common::Vec3 _aiVelocity = { 0,0,0 };
        float _verticalVelocity = 0.0f;
    };
}
