#pragma once
#include "CharacterControllerComponent.h"

namespace PIP::GAME
{
    class PlayerControllerComponent : public CharacterControllerComponent {
    public:
        using CharacterControllerComponent::CharacterControllerComponent;
        void PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator) override;
        // void SetMoveVelocity(const common::Vec3& velocity) { _moveVelocity = velocity; }

        void SetMoveVelocity(const common::Vec3& velocity) { _targetMoveVelocity = velocity; }
    private:
        common::Vec3 _targetMoveVelocity = { 0, 0, 0 };  // 가야 할 목표 속도
        common::Vec3 _currentMoveVelocity = { 0, 0, 0 }; // 현재 물리 바디에 적용 중인 보간된 속도
        common::Vec3 _moveVelocity = { 0,0,0 };
        float _timer = 0;
    };
}
