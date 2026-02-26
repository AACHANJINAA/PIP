#include "pch.h"
#include "PlayerControllerComponent.h"

#include "GameObject.h"
#include "TransformComponent.h"

namespace PIP::GAME
{
    void PlayerControllerComponent::PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator) {
        if (!_character) return;

        // 1. 임팩트 감쇄 및 속도 합성 (플레이어 조작 위주)
        float impactSpeed = common::Length(_impactVelocity);
        if (impactSpeed > 0.1f) _impactVelocity = common::Normalize(_impactVelocity) * std::max(0.0f, impactSpeed -
            ImpactFriction * deltaTime);
        else _impactVelocity = common::Vec3Zero;

        common::Vec3 horizontalVel = _moveVelocity + _impactVelocity;
        JPH::Vec3 finalJoltVel = Utils::ToJolt(horizontalVel);

        // 2. 중력 적용
        float currentYVel = _character->GetLinearVelocity().GetY();
        finalJoltVel.SetY(_character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround ? -0.5f :
            currentYVel + _physicsSystem->GetGravity().GetY() * deltaTime);

        _character->SetLinearVelocity(finalJoltVel);
        _character->Update(deltaTime, _physicsSystem->GetGravity(),
            _physicsSystem->GetDefaultBroadPhaseLayerFilter(_physicsLayer),
            _physicsSystem->GetDefaultLayerFilter(_physicsLayer), {}, {}, *allocator);

        // 3. Transform 동기화
        auto tc = GetOwner()->GetComponent<TransformComponent>();
        if (tc) tc->SetPosition(this->GetPosition());
    }
}
