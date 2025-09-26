#include "stdafx.h"
#include "TransformComponent.h"
#include "GameObject.h"

TransformComponent::TransformComponent() : _isDirty(true)
{
    _localPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
    _localRotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    _localScale = XMFLOAT3(1.0f, 1.0f, 1.0f);

    XMStoreFloat4x4(&_worldMatrix, XMMatrixIdentity());
    _position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    _rotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    _right = XMFLOAT3(1.0f, 0.0f, 0.0f);
    _up = XMFLOAT3(0.0f, 1.0f, 0.0f);
    _forward = XMFLOAT3(0.0f, 0.0f, 1.0f);

    set_name("TransformComponent");
}

void TransformComponent::update()
{
    // isDirty 플래그 없이 항상 계산합니다.
      // 부모의 update가 항상 먼저 호출되는 것이 보장되기 때문입니다.
    XMMATRIX localTransform = XMMatrixScalingFromVector(XMLoadFloat3(&_localScale)) *
        XMMatrixRotationQuaternion(XMLoadFloat4(&_localRotation)) *
        XMMatrixTranslationFromVector(XMLoadFloat3(&_localPosition));

    XMMATRIX finalWorldMatrix;
    if (auto parent_ptr = _parent.lock())
    {
        // 부모의 월드 행렬은 이미 이번 프레임에 계산이 끝난 최신 상태입니다.
        finalWorldMatrix = localTransform * XMLoadFloat4x4(&parent_ptr->_worldMatrix);
    }
    else
    {
        finalWorldMatrix = localTransform;
    }

    // 월드 행렬 및 파생 데이터 저장
    XMStoreFloat4x4(&_worldMatrix, finalWorldMatrix);

    XMVECTOR worldScale, worldRotation, worldPosition;
    XMMatrixDecompose(&worldScale, &worldRotation, &worldPosition, finalWorldMatrix);

    XMStoreFloat3(&_position, worldPosition);
    XMStoreFloat4(&_rotation, worldRotation);
    XMStoreFloat3(&_right, XMVector3Rotate(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), worldRotation));
    XMStoreFloat3(&_up, XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), worldRotation));
    XMStoreFloat3(&_forward, XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), worldRotation));
}

const XMFLOAT3& TransformComponent::position() 
{
    const XMFLOAT4X4& worldMat = world_matrix();
    return XMFLOAT3(worldMat._41, worldMat._42, worldMat._43);
}

const XMFLOAT4& TransformComponent::rotation() 
{
    XMVECTOR s, r, t;
    XMMatrixDecompose(&s, &r, &t, XMLoadFloat4x4(&world_matrix()));
    XMFLOAT4 result;
    XMStoreFloat4(&result, r);
    return result;
}

const XMFLOAT3& TransformComponent::right() 
{
    XMFLOAT3 rightDir;
    XMMATRIX worldMat = XMLoadFloat4x4(&world_matrix());
    XMStoreFloat3(&rightDir, XMVector3Normalize(worldMat.r[0]));
    return rightDir;
}

const XMFLOAT3& TransformComponent::up() 
{
    XMFLOAT3 upDir;
    XMMATRIX worldMat = XMLoadFloat4x4(&world_matrix());
    XMStoreFloat3(&upDir, XMVector3Normalize(worldMat.r[1]));
    return upDir;
}

const XMFLOAT3& TransformComponent::forward() 
{
    XMFLOAT3 forwardDir;
    XMMATRIX worldMat = XMLoadFloat4x4(&world_matrix());
    XMStoreFloat3(&forwardDir, XMVector3Normalize(worldMat.r[2]));
    return forwardDir;
}

const XMFLOAT4X4& TransformComponent::world_matrix() 
{
    if (_isDirty)
    {
        force_update_hierarchy();
    }
    return _worldMatrix;
}

// --- Hierarchy Management ---
void TransformComponent::set_parent(std::shared_ptr<TransformComponent> newParent)
{
    if (auto oldParent_ptr = _parent.lock())
    {
        oldParent_ptr->remove_child(std::static_pointer_cast<TransformComponent>(shared_from_this()));
    }
    if (newParent)
    {
        newParent->add_child(std::static_pointer_cast<TransformComponent>(shared_from_this()));
    }
    _parent = newParent;
    _isDirty = true;
}

void TransformComponent::add_child(std::shared_ptr<TransformComponent> child)
{
    if (child)
    {
	    _children.push_back(child);
    }
}

void TransformComponent::remove_child(std::shared_ptr<TransformComponent> child)
{
	std::erase(_children, child);
}

void TransformComponent::force_update_hierarchy()
{
    // 1. 부모가 있고, 부모가 더럽다면, 부모부터 강제로 업데이트
    if (auto parent_ptr = _parent.lock())
    {
        // 부모의 world_matrix() Getter를 호출하는 것만으로 연쇄 업데이트가 일어남
        parent_ptr->world_matrix();
    }

    // 2. 자신의 월드 행렬 계산
    XMMATRIX localTransform = XMMatrixScalingFromVector(XMLoadFloat3(&_localScale)) *
			XMMatrixRotationQuaternion(XMLoadFloat4(&_localRotation)) *
			XMMatrixTranslationFromVector(XMLoadFloat3(&_localPosition));

    if (auto parent_ptr = _parent.lock())
    {
        XMStoreFloat4x4(&_worldMatrix, localTransform * XMLoadFloat4x4(&parent_ptr->_worldMatrix));
    }
    else
    {
        XMStoreFloat4x4(&_worldMatrix, localTransform);
    }

    // 3. 계산이 끝났으므로 더티 플래그를 false로 변경
    _isDirty = false;
}

std::shared_ptr<TransformComponent> TransformComponent::child(int index) const
{
	if (index < 0 || index >= _children.size()) 
    {
        return nullptr;
    }
    return _children[index];
}

std::vector<std::shared_ptr<TransformComponent>> TransformComponent::children() const
{
    return _children;
}

// --- Setters ---
void TransformComponent::set_local_position(const XMFLOAT3& position)
{
    _localPosition = position;
    _isDirty = true;
}

void TransformComponent::set_local_rotation(const XMFLOAT4& rotation)
{
    _localRotation = rotation;
    _isDirty = true;
}

void TransformComponent::set_local_scale(const XMFLOAT3& scale)
{
    _localScale = scale;
    _isDirty = true;
}