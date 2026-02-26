#pragma once
#include "CharacterControllerComponent.h"

namespace PIP::GAME
{
    class PlayerControllerComponent : public CharacterControllerComponent {
    public:
        using CharacterControllerComponent::CharacterControllerComponent;
        void PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator) override;
        void SetMoveVelocity(const common::Vec3& velocity) { _moveVelocity = velocity; }
    private:
        common::Vec3 _moveVelocity = { 0,0,0 };
    };
}
