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

const XMFLOAT3 TransformComponent::local_rotation_euler()
{
    // 1. 월드 쿼터니언을 가져옵니다. (자신의 회전 + 부모의 회전이 모두 적용된 최종값)
    XMFLOAT4 worldQuat = rotation();

    // 2. 이 쿼터니언을 회전 행렬(Matrix)로 바꿉니다.
    XMMATRIX mat = XMMatrixRotationQuaternion(XMLoadFloat4(&worldQuat));

    // 3. 행렬의 성분에 접근하기 위해 4x4 형태로 저장합니다.
    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, mat);

    XMFLOAT3 euler;

    // 4. 행렬에서 직접 각도 추출 (DirectX Left-Handed 기준)
    // Pitch (X축 회전): 전방 벡터의 Y성분(-m._32)
    euler.x = std::asin(-m._32);

    // 짐벌락(Gimbal Lock) 방어: Pitch가 90도나 -90도(위/아래를 완벽히 쳐다봄)에 가까운지 확인
    if (std::cos(euler.x) > 0.0001f)
    {
        // 정상 상태: Yaw와 Roll 정상 계산
        euler.y = std::atan2(m._31, m._33); // Yaw (Y축 회전)
        euler.z = std::atan2(m._12, m._22); // Roll (Z축 회전)
    }
    else
    {
        // 짐벌락 상태: 카메라가 수직으로 서 있을 때는 Roll을 포기하고 Yaw만 계산
        euler.y = std::atan2(-m._13, m._11);
        euler.z = 0.0f;
    }

    // 5. 라디안(Radian)을 눈으로 읽기 편한 도(Degree) 단위로 변환
    euler.x = DirectX::XMConvertToDegrees(euler.x);
    euler.y = DirectX::XMConvertToDegrees(euler.y);
    euler.z = DirectX::XMConvertToDegrees(euler.z);

    return euler; // 세상 기준의 진짜 오일러 각도 반환!
}

// --- Setter 함수들 ---

void TransformComponent::set_world_matrix(const XMFLOAT4X4& matrix)
{
    XMMATRIX worldMat = XMLoadFloat4x4(&matrix);
    // 월드 행렬에서 위치, 회전, 스케일 분해
    XMVECTOR scaleVec, rotationQuat, translationVec;
    XMMatrixDecompose(&scaleVec, &rotationQuat, &translationVec, worldMat);
    // 로컬 공간으로 변환
    if (auto parent_ptr = _parent.lock())
    {
        XMMATRIX parentWorldMat = XMLoadFloat4x4(&parent_ptr->world_matrix());
        XMMATRIX parentWorldMatInv = XMMatrixInverse(nullptr, parentWorldMat);
        XMMATRIX localMat = worldMat * parentWorldMatInv;
        // 다시 분해
        XMMatrixDecompose(&scaleVec, &rotationQuat, &translationVec, localMat);
    }
    // 값 저장
    XMStoreFloat3(&_localPosition, translationVec);
    XMStoreFloat4(&_localRotation, rotationQuat);
    XMStoreFloat3(&_localScale, scaleVec);
	set_hierarchy_dirty(); // 자신과 자식들에게 변경 전파
}

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

void TransformComponent::set_local_rotation(float x, float y, float z, float w)
{
	common::Quat q{ x, y, z, w };
    set_local_rotation(q);
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
    auto render_component = gameObject->get_component<RenderComponent>();

    if (render_component)
    {
        BoundingOrientedBox local_bounding_box = render_component->mesh()->bounding_box();
        XMMATRIX worldMat = XMLoadFloat4x4(&world_matrix());

        // 1. 로컬 바운딩 박스를 통째로 월드 공간으로 변환 (회전, 스케일, 이동 모두 자동 적용)
        BoundingOrientedBox world_bounding_box;
        local_bounding_box.Transform(world_bounding_box, worldMat);

        // 2. Extents는 '절반 크기(Half-size)'이므로 2.0을 곱해야 전체 길이나 나옴
        XMFLOAT3 world_size;
        world_size.x = world_bounding_box.Extents.x * 2.0f;
        world_size.y = world_bounding_box.Extents.y * 2.0f;
        world_size.z = world_bounding_box.Extents.z * 2.0f;

        return world_size;
    }

    return XMFLOAT3(0, 0, 0);
}

XMFLOAT3 TransformComponent::get_world_position()
{
    const XMFLOAT4X4& worldMat = world_matrix(); // 1. 여기서 최신 월드 행렬을 계산해서 가져옴
    return XMFLOAT3(worldMat._41, worldMat._42, worldMat._43); // 2. 행렬에서 '위치' 값만 쏙 빼옴
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
// ---------------------------- Helper Functions ----------------------------
void TransformComponent::camera_rotate(float pitch, float yaw, float roll)
{
    total_yaw_rad += XMConvertToRadians(yaw);
    total_pitch_rad += XMConvertToRadians(pitch);

    if(_cameraRotationMode)
    {
        if (XMConvertToDegrees(total_pitch_rad) > 89.f)
        {
            total_pitch_rad = XMConvertToRadians(89.f);
        }
        else if (XMConvertToDegrees(total_pitch_rad) < -89.f)
        {
            total_pitch_rad = XMConvertToRadians(-89.f);
        }
    }
    else // 만약 자유 시점 카메라가 아니라면? -> 제약걸기
    {
        if (XMConvertToDegrees(total_pitch_rad) > 60.f) // 카메라 고개 내리는 각도
        {
            total_pitch_rad = XMConvertToRadians(60.f);
        }
        else if (XMConvertToDegrees(total_pitch_rad) < -60.f) // 윗방향 보는 각도
        {
            total_pitch_rad = XMConvertToRadians(-60.f);
		}
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

void TransformComponent::set_camera_rotate(float pitch, float yaw, float roll)
{
    total_yaw_rad = XMConvertToRadians(yaw);
    total_pitch_rad = XMConvertToRadians(pitch);

    if (_cameraRotationMode)
    {
        if (XMConvertToDegrees(total_pitch_rad) > 89.f)
        {
            total_pitch_rad = XMConvertToRadians(89.f);
        }
        else if (XMConvertToDegrees(total_pitch_rad) < -89.f)
        {
            total_pitch_rad = XMConvertToRadians(-89.f);
        }
    }
    else // 만약 자유 시점 카메라가 아니라면? -> 제약걸기
    {
        if (XMConvertToDegrees(total_pitch_rad) > 60.f) // 카메라 고개 내리는 각도
        {
            total_pitch_rad = XMConvertToRadians(60.f);
        }
        else if (XMConvertToDegrees(total_pitch_rad) < -60.f) // 윗방향 보는 각도
        {
            total_pitch_rad = XMConvertToRadians(-60.f);
        }
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

common::Quat TransformComponent::apply_offset_rotation(const common::Quat& base_quat, float pitch_offset_deg,
	float yaw_offset_deg, float roll_offset_deg)
{
    // 1. 기본 쿼터니언 로드
    XMVECTOR base_q_xm = XMLoadFloat4(&base_quat);

    // 2. 오프셋 오일러 각으로부터 쿼터니언 생성
    XMVECTOR offset_q_xm = XMQuaternionRotationRollPitchYaw(
        XMConvertToRadians(pitch_offset_deg),
        XMConvertToRadians(yaw_offset_deg),
        XMConvertToRadians(roll_offset_deg)
    );

    // 3. 두 쿼터니언 합성 (오프셋 -> 기본 순서로 적용)
    XMVECTOR combined_q_xm = XMQuaternionMultiply(offset_q_xm, base_q_xm);

    // 4. 결과 저장 및 반환
    common::Quat result_quat;
    XMStoreFloat4(&result_quat, combined_q_xm);
    return result_quat;
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