#include "stdafx.h"
#include "PhysicsColliderComponent.h"

#include "GameObject.h"
#include "JoltHelper.h"
#include "TransformComponent.h"
#include "PhysicsManager.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
PhysicsColliderComponent::PhysicsColliderComponent() : _bodyID{} {}

PhysicsColliderComponent::~PhysicsColliderComponent()
{
    if (!_bodyID.IsInvalid() && PhysicsManager::instance()->get_physics_system())
    {
        PhysicsManager::instance()->get_body_interface().RemoveBody(_bodyID);
        PhysicsManager::instance()->get_body_interface().DestroyBody(_bodyID);
    }
}

void PhysicsColliderComponent::initialize(ShapeType type, const XMFLOAT3& size, const f3& center, const f3& rotation_offset,
                                          bool isSensor)
{
    _shapeType = type;
    _size = size;
    _center = center;
    _rotationOffset = rotation_offset;
    _isSensor = isSensor;
    create_body();
}

void PhysicsColliderComponent::create_body()
{
    
    switch (_shapeType)
    {
    case ShapeType::Box:
        _shape = new JPH::BoxShape(JPH::Vec3(_size.x * 0.5f, _size.y * 0.5f, _size.z * 0.5f));
        break;
    case ShapeType::Sphere:
        _shape = new JPH::SphereShape(_size.x);
        break;
    case ShapeType::Capsule:
        _shape = new JPH::CapsuleShape(_size.y, _size.x);
        break;
    }

    // 초기 위치 (Transform 기반)
    auto trans = game_object()->transform();
    JPH::RVec3 pos = PIP::Utils::ToJolt(trans->position());
    JPH::Quat rot = PIP::Utils::ToJolt(trans->rotation());

    JPH::BodyCreationSettings settings(_shape, pos, rot,
        JPH::EMotionType::Kinematic, // 트랜스폼을 수동으로 따라가야 함
        _isSensor ? PIP::Layers::SENSOR : PIP::Layers::MOVING);

    settings.mIsSensor = _isSensor;
    settings.mUserData = reinterpret_cast<uint64_t>(game_object().get()); // GameObject 연결

    auto& bodyInterface = PhysicsManager::instance()->get_body_interface();
    _bodyID = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);
}
void PhysicsColliderComponent::set_active(bool active)
{
    _isActive = active;
    if (_bodyID.IsInvalid()) return;

    auto& body_interface = PhysicsManager::instance()->get_body_interface();
    if (active) body_interface.ActivateBody(_bodyID);
    else body_interface.DeactivateBody(_bodyID);
}
void PhysicsColliderComponent::fixed_update(float deltaTime)
{
    if (_bodyID.IsInvalid() || !_isActive) return;

    auto trans = game_object()->transform();
    XMMATRIX worldMat = XMLoadFloat4x4(&trans->world_matrix());

    // 1. 로컬 오프셋을 월드 좌표로 변환
    f3 worldPosVec;
	XMStoreFloat3(&worldPosVec,XMVector3Transform(XMLoadFloat3(&_center), worldMat));

    // 2. 로컬 회전 오프셋을 월드 회전에 적용
    XMVECTOR localRotQuat = XMQuaternionRotationRollPitchYaw(
        XMConvertToRadians(_rotationOffset.x),
        XMConvertToRadians(_rotationOffset.y),
        XMConvertToRadians(_rotationOffset.z)
    );
    common::Quat worldRotQuat;
    f4 transform_rot = trans->rotation();
	XMStoreFloat4(&worldRotQuat, XMQuaternionMultiply(localRotQuat, XMLoadFloat4(&transform_rot)));

    JPH::RVec3 pos = PIP::Utils::ToJolt(worldPosVec);
    JPH::Quat rot = PIP::Utils::ToJolt(worldRotQuat);

    // 물리 바디를 계산된 위치로 이동
    PhysicsManager::instance()->get_body_interface().MoveKinematic(_bodyID, pos, rot, deltaTime);
}
void PhysicsColliderComponent::OnContact(std::shared_ptr<GameObject> other)
{
    if (!_isActive) return;

    // 1. 등록된 개별 콜백 호출 (기존 방식)
    if (_onCollision) _onCollision(other);

    // 2. Unity 스타일: GameObject를 통해 모든 스크립트에 전파
    game_object()->on_collision_enter(other);
}
