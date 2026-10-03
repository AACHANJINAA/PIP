#include "stdafx.h"
#include "PhysicsColliderComponent.h"

#include "GameObject.h"
#include "JoltHelper.h"
#include "TransformComponent.h"
#include "PhysicsManager.h"
#include "AnimationComponent.h"

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
                                          bool isSensor, BodyMode bodyMode)
{
    _shapeType = type;
    _size = size;
    _center = center;
    _rotationOffset = rotation_offset;
    _isSensor = isSensor;
    _bodyMode = bodyMode;
    create_body();
}

void PhysicsColliderComponent::attach_to_bone(const std::string& bone_name)
{
    _attachedBone = bone_name;
    _boneErrorLogged = false;
    _hasWorldTransform = false; // 기준이 바뀌었으므로 직전 프레임 값을 이어 쓰지 않음
}

void PhysicsColliderComponent::detach_from_bone()
{
    _attachedBone.clear();
    _hasWorldTransform = false;
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

    // 검사 전용 모드는 모양만 들고 있고 Jolt 바디는 만들지 않음
    if (_bodyMode == BodyMode::QueryOnly)
    {
        return;
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

    // 뼈 부착 바디는 late_update에서 계산한 월드 변환을 사용 (물리 스텝이 late_update보다 먼저라 한 프레임 늦음)
    if (is_attached_to_bone())
    {
        if (!_hasWorldTransform) return;
        PhysicsManager::instance()->get_body_interface().MoveKinematic(
            _bodyID, _worldTransform.GetTranslation(), _worldTransform.GetQuaternion(), deltaTime);
        return;
    }

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
void PhysicsColliderComponent::late_update(float deltaTime)
{
    if (!_shape) return; // initialize 전

    JPH::RMat44 world;
    if (!compute_world_transform(world))
    {
        _hasWorldTransform = false;
        return;
    }

    // 첫 프레임은 직전 값이 없으므로 현재 값으로 채움
    _prevWorldTransform = _hasWorldTransform ? _worldTransform : world;
    _worldTransform = world;
    _hasWorldTransform = true;
}

bool PhysicsColliderComponent::compute_world_transform(JPH::RMat44& out)
{
    auto owner = game_object();
    if (!owner || !owner->transform()) return false;

    // 기준 행렬: 뼈 부착이면 (뼈 모델 행렬 × 오브젝트 월드), 아니면 오브젝트 월드
    XMMATRIX basis = XMLoadFloat4x4(&owner->transform()->world_matrix());
    if (is_attached_to_bone())
    {
        auto anim = owner->get_component<AnimationComponent>();
        XMFLOAT4X4 bone_model;
        if (!anim || !anim->try_get_bone_model_matrix(_attachedBone, bone_model))
        {
            // 첫 애니메이션 계산 전에는 자세가 없으므로 그때는 로그를 남기지 않음
            if (anim && anim->has_pose() && !_boneErrorLogged)
            {
                CLOG("[PhysicsCollider] 뼈를 찾을 수 없음: " << _attachedBone << " (" << owner->name() << ")");
                _boneErrorLogged = true;
            }
            return false;
        }
        basis = XMMatrixMultiply(XMLoadFloat4x4(&bone_model), basis);
    }

    // 로컬 오프셋: 회전 오프셋(도) 후 중심 이동 (DirectX 행 벡터 규약: local × basis)
    XMMATRIX local = XMMatrixMultiply(
        XMMatrixRotationQuaternion(XMQuaternionRotationRollPitchYaw(
            XMConvertToRadians(_rotationOffset.x),
            XMConvertToRadians(_rotationOffset.y),
            XMConvertToRadians(_rotationOffset.z))),
        XMMatrixTranslation(_center.x, _center.y, _center.z));
    XMMATRIX world = XMMatrixMultiply(local, basis);

    // Jolt 모양은 행렬 스케일을 반영하지 않으므로 회전과 위치만 사용
    XMVECTOR scale, rotation, translation;
    if (!XMMatrixDecompose(&scale, &rotation, &translation, world)) return false;

    XMFLOAT3 s;
    XMStoreFloat3(&s, scale);
    if (!_scaleWarningLogged && (fabsf(s.x - 1.0f) > 0.01f || fabsf(s.y - 1.0f) > 0.01f || fabsf(s.z - 1.0f) > 0.01f))
    {
        CLOG("[PhysicsCollider] 스케일이 1이 아니어서 무시됨: (" << s.x << ", " << s.y << ", " << s.z << ") " << owner->name());
        _scaleWarningLogged = true;
    }

    XMFLOAT4 q;
    XMFLOAT3 t;
    XMStoreFloat4(&q, XMQuaternionNormalize(rotation));
    XMStoreFloat3(&t, translation);
    out = JPH::RMat44::sRotationTranslation(JPH::Quat(q.x, q.y, q.z, q.w), JPH::RVec3(t.x, t.y, t.z));
    return true;
}

void PhysicsColliderComponent::OnContact(std::shared_ptr<GameObject> other)
{
    if (!_isActive) return;

    // 1. 등록된 개별 콜백 호출 (기존 방식)
    if (_onCollision) _onCollision(other);

    // 2. Unity 스타일: GameObject를 통해 모든 스크립트에 전파
    game_object()->on_collision_enter(other);
}
