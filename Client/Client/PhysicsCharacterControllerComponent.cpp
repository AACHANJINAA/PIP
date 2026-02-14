#include "stdafx.h"
#include "PhysicsCharacterControllerComponent.h"
#include "PhysicsManager.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "../../Common/JoltHelper.h"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include <algorithm>

PhysicsCharacterControllerComponent::~PhysicsCharacterControllerComponent() {}
void PhysicsCharacterControllerComponent::initialize(float height, float radius)
{
    auto* pm = PhysicsManager::instance();
    auto* system = pm->get_physics_system();
    // 1. 서버와 동일한 캡슐 생성 로직
	// 전체 높이(height)에서 양쪽 구체 반지름(radius)을 뺀 것이 실린더의 높이입니다.
    float halfCylinderHeight = (height * 0.5f) - radius;
    halfCylinderHeight = std::max(halfCylinderHeight, 0.1f);

    _settings = new JPH::CharacterVirtualSettings();
    // [수정] RotatedTranslatedShape 쓰지 말고 순수 캡슐만 사용 (중심이 0,0,0)
    _settings->mShape = new JPH::CapsuleShape(halfCylinderHeight, radius);

    // 발바닥 위치 정의 (바닥 체크용)
    _settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -(height * 0.5f));

    _character = new JPH::CharacterVirtual(_settings, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), system);

    // 나중에 보정치를 쓰기 위해 저장해둡니다.
    _halfHeight = height * 0.5f;
}

void PhysicsCharacterControllerComponent::fixed_update(float deltaTime)
{
    if (!_character) return;
    auto* pm = PhysicsManager::instance();
    auto* system = pm->get_physics_system();

    // 중력 수동 가속 (서버와 동일)
    JPH::Vec3 currentVel = _character->GetLinearVelocity();
    float newYVel = currentVel.GetY();

    if (_character->GetGroundState() != JPH::CharacterVirtual::EGroundState::OnGround) {
        newYVel += system->GetGravity().GetY() * deltaTime;
    }
    else {
        newYVel = (std::max)(newYVel, -0.5f); // 접지 유지
    }

    currentVel.SetY(newYVel);
    _character->SetLinearVelocity(currentVel);

    // 업데이트
    _character->Update(deltaTime, system->GetGravity(),
        system->GetDefaultBroadPhaseLayerFilter(PIP::Layers::MOVING),
        system->GetDefaultLayerFilter(PIP::Layers::MOVING),
        {}, {}, *pm->get_temp_allocator());
}

void PhysicsCharacterControllerComponent::set_position(const common::Vec3& pos)
{
    if (_character)
    {
        // 입력받은 pos는 '발바닥' 위치입니다.
        // Jolt 캡슐의 중심은 (0,0,0)이므로 '반 높이'만큼 올려서 셋팅해야 발바닥이 땅에 닿습니다.
        JPH::RVec3 centerPos = PIP::Utils::ToJolt(pos);
        centerPos.SetY(centerPos.GetY() + _halfHeight);
        _character->SetPosition(centerPos);
    }
}
void PhysicsCharacterControllerComponent::set_velocity(const common::Vec3& vel)
{
    if (_character) _character->SetLinearVelocity(PIP::Utils::ToJolt(vel));
}
common::Vec3 PhysicsCharacterControllerComponent::get_position() const
{
    if (!_character) return { 0,0,0 };
    // 물리 엔진에서 준 '중심' 위치를 가져와서 다시 '반 높이'만큼 내려야 '발바닥' 좌표가 됩니다.
    JPH::RVec3 centerPos = _character->GetPosition();
    common::Vec3 footPos = PIP::Utils::FromJolt(centerPos);
    footPos.y -= _halfHeight;
    return footPos;
}
common::Vec3 PhysicsCharacterControllerComponent::get_velocity() const
{
    return _character ? PIP::Utils::FromJolt(_character->GetLinearVelocity()) : common::Vec3{};
}
