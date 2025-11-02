#include "stdafx.h"
#include "TransformComponent.h"
#include "GameObject.h"
#include "RenderComponent.h"

TransformComponent::TransformComponent() : _isDirty(true)
{
    _localPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
    _localRotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    _localScale = XMFLOAT3(1.0f, 1.0f, 1.0f);
    XMStoreFloat4x4(&_worldMatrix, XMMatrixIdentity());
    set_name("TransformComponent");
}

// [추가] 자신과 모든 자식에게 재귀적으로 더럽다고 알리는 함수
void TransformComponent::set_hierarchy_dirty()
{
    if (_isDirty) return; // 이미 더러우면 중복 작업을 방지
    _isDirty = true;

    for (const auto& child : _children)
    {
        if (child)
        {
            child->set_hierarchy_dirty();
        }
    }
}

// [변경] 자신의 월드 행렬만 계산하는 함수
void TransformComponent::calculate_world_matrix()
{
    XMMATRIX localTransform = XMMatrixScalingFromVector(XMLoadFloat3(&_localScale)) *
        XMMatrixRotationQuaternion(XMLoadFloat4(&_localRotation)) *
        XMMatrixTranslationFromVector(XMLoadFloat3(&_localPosition));

    if (auto parent_ptr = _parent.lock())
    {
        // 부모의 world_matrix()를 호출하여, 부모가 먼저 계산되도록 보장
        XMStoreFloat4x4(&_worldMatrix, localTransform * XMLoadFloat4x4(&parent_ptr->world_matrix()));
    }
    else
    {
        XMStoreFloat4x4(&_worldMatrix, localTransform);
    }

    _isDirty = false; // 계산 완료 후 플래그 내리기
}

// --- Getter 함수들 ---

const XMFLOAT4X4& TransformComponent::world_matrix()
{
    if (_isDirty)
    {
        calculate_world_matrix();
    }
    return _worldMatrix;
}

XMFLOAT3 TransformComponent::position()
{
    const XMFLOAT4X4& worldMat = world_matrix(); // 이 호출로 최신 행렬임이 보장됨
    return XMFLOAT3(worldMat._41, worldMat._42, worldMat._43);
}

XMFLOAT4 TransformComponent::rotation()
{
    XMVECTOR s, r, t;
    XMMatrixDecompose(&s, &r, &t, XMLoadFloat4x4(&world_matrix()));
    XMFLOAT4 result;
    XMStoreFloat4(&result, r);
    return result;
}

XMFLOAT3 TransformComponent::right()
{
    XMFLOAT3 result;
    XMMATRIX worldMat = XMLoadFloat4x4(&world_matrix());
    XMStoreFloat3(&result, XMVector3Normalize(worldMat.r[0]));
    return result;
}

XMFLOAT3 TransformComponent::up()
{
    XMFLOAT3 result;
    XMMATRIX worldMat = XMLoadFloat4x4(&world_matrix());
    XMStoreFloat3(&result, XMVector3Normalize(worldMat.r[1]));
    return result;
}

XMFLOAT3 TransformComponent::forward()
{
    XMFLOAT3 result;
    XMMATRIX worldMat = XMLoadFloat4x4(&world_matrix());
    XMStoreFloat3(&result, XMVector3Normalize(worldMat.r[2]));
    return result;
}

// --- Setter 함수들 ---

void TransformComponent::set_local_position(const XMFLOAT3& position)
{
    _localPosition = position;
    set_hierarchy_dirty(); // 자신과 자식들에게 변경 전파
}

void TransformComponent::set_local_rotation(const XMFLOAT4& rotation)
{
    _localRotation = rotation;
    set_hierarchy_dirty(); // 자신과 자식들에게 변경 전파
}

void TransformComponent::set_local_rotation(float pitch, float yaw, float roll)
{
    XMVECTOR rotationQuat = XMQuaternionRotationRollPitchYaw(
        XMConvertToRadians(pitch),
        XMConvertToRadians(yaw),
        XMConvertToRadians(roll)
    );

    XMFLOAT4 localRotation;
    XMStoreFloat4(&localRotation, rotationQuat);

    set_local_rotation(localRotation);
}

void TransformComponent::set_local_scale(const XMFLOAT3& scale)
{
    _localScale = scale;
    set_hierarchy_dirty(); // 자신과 자식들에게 변경 전파
}

void TransformComponent::move_forward(float distance)
{
	world_matrix(); // 최신 행렬 보장

	_localPosition.x += forward().x * distance;
	_localPosition.y += forward().y * distance;
	_localPosition.z += forward().z * distance;

    set_hierarchy_dirty(); // 자신과 자식들에게 변경 전파
}

void TransformComponent::move_right(float distance)
{
    world_matrix(); // 최신 행렬 보장

	_localPosition.x += right().x * distance;
	_localPosition.y += right().y * distance;
	_localPosition.z += right().z * distance;

    set_hierarchy_dirty(); // 자신과 자식들에게 변경 전파
}

void TransformComponent::move_up(float distance)
{
    world_matrix(); // 최신 행렬 보장

	_localPosition.x += up().x * distance;
	_localPosition.y += up().y * distance;
	_localPosition.z += up().z * distance;

    set_hierarchy_dirty(); // 자신과 자식들에게 변경 전파
}

XMFLOAT3 TransformComponent::get_world_scale()
{
    auto gameObject = _gameObject.lock();
    auto render_component = gameObject.get()->get_component<RenderComponent>();

    if (render_component)
    {
        BoundingOrientedBox local_bounding_box = render_component->mesh()->bounding_box();
        XMFLOAT3 local_min = local_bounding_box.Center;
        XMFLOAT3 local_max = local_bounding_box.Extents;
        XMFLOAT3 world_min;
        XMFLOAT3 world_max;
        XMMATRIX worldMat = XMLoadFloat4x4(&world_matrix());
        // 로컬 최소점과 최대점을 월드 공간으로 변환
        XMVECTOR localMinVec = XMLoadFloat3(&local_min);
        XMVECTOR localMaxVec = XMLoadFloat3(&local_max);
        XMVECTOR worldMinVec = XMVector3Transform(localMinVec, worldMat);
        XMVECTOR worldMaxVec = XMVector3Transform(localMaxVec, worldMat);
        XMStoreFloat3(&world_min, worldMinVec);
        XMStoreFloat3(&world_max, worldMaxVec);
        // 월드 공간에서의 크기 계산
        XMFLOAT3 world_scale;
        world_scale.x = fabs(world_max.x - world_min.x);
        world_scale.y = fabs(world_max.y - world_min.y);
        world_scale.z = fabs(world_max.z - world_min.z);
        return world_scale;
	}

    return XMFLOAT3();
}

void TransformComponent::rotate(float pitch, float yaw, float roll)
{
        XMVECTOR delta_rotation_quat = XMQuaternionRotationRollPitchYaw(
            XMConvertToRadians(pitch),
            XMConvertToRadians(yaw),
            XMConvertToRadians(roll)
        );

        // 기존 로컬 회전 쿼터니언을 가져옵니다.
        XMVECTOR current_local_quat = XMLoadFloat4(&_localRotation);

        // 두 쿼터니언을 곱하여 회전을 누적합니다. (순서 중요: new * old)
        XMVECTOR new_local_quat = XMQuaternionMultiply(delta_rotation_quat, current_local_quat);

        // 결과를 정규화하고 다시 저장합니다.
        XMStoreFloat4(&_localRotation, XMQuaternionNormalize(new_local_quat));

        // 행렬이 더럽혀졌음을 표시합니다.
        set_hierarchy_dirty();
}

void TransformComponent::camera_rotate(float pitch, float yaw, float roll)
{
    static float total_yaw_rad = 0.f; 
    static float total_pitch_rad = 0.f; 

    total_yaw_rad += XMConvertToRadians(yaw);
    total_pitch_rad += XMConvertToRadians(pitch);

    if (XMConvertToDegrees(total_pitch_rad) > 89.f)
    {
        total_pitch_rad = XMConvertToRadians(89.f);
    }
    else if (XMConvertToDegrees(total_pitch_rad) < -89.f)
    {
        total_pitch_rad = XMConvertToRadians(-89.f);
    }

    // Yaw는 항상 월드 Y축(0,1,0)을 기준으로 합니다.
    XMVECTOR yaw_quat = XMQuaternionRotationAxis(
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
        total_yaw_rad
    );

    // Pitch는 로컬 X축(1,0,0)을 기준으로 합니다.
    XMVECTOR pitch_quat = XMQuaternionRotationAxis(
        XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
        total_pitch_rad
    );

    // 최종 회전을 계산하여 _localRotation에 '덮어씁니다'. (곱하는 게 아님!)
    XMVECTOR final_quat = XMQuaternionMultiply(pitch_quat, yaw_quat);
    XMStoreFloat4(&_localRotation, XMQuaternionNormalize(final_quat));

    // 행렬이 더럽혀졌음을 표시합니다.
    set_hierarchy_dirty();
}

// --- Hierarchy Management ---

void TransformComponent::set_parent(std::shared_ptr<TransformComponent> newParent)
{
    // 기존 부모가 있다면, 기존 부모의 자식 목록에서 나를 제거
    if (auto oldParent_ptr = _parent.lock())
    {
        // oldParent_ptr의 private 멤버인 remove_child를 스스로 호출
        oldParent_ptr->remove_child(std::static_pointer_cast<TransformComponent>(shared_from_this()));
    }

    // 새로운 부모가 있다면, 새 부모의 자식 목록에 나를 추가
    if (newParent)
    {
        newParent->add_child(std::static_pointer_cast<TransformComponent>(shared_from_this()));
    }

    _parent = newParent;
    set_hierarchy_dirty();
}



void TransformComponent::add_child(std::shared_ptr<TransformComponent> child)
{
    if (child) { _children.push_back(child); }
}

void TransformComponent::remove_child(std::shared_ptr<TransformComponent> child)
{
    std::erase(_children, child);
}

std::shared_ptr<TransformComponent> TransformComponent::child(int index) const
{
    if (index < 0 || index >= _children.size()) return nullptr;
    return _children[index];
}