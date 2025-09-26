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
    if (!_isDirty) return;

    XMMATRIX localTransform = XMMatrixScalingFromVector(XMLoadFloat3(&_localScale)) *
        XMMatrixRotationQuaternion(XMLoadFloat4(&_localRotation)) *
        XMMatrixTranslationFromVector(XMLoadFloat3(&_localPosition));

    XMMATRIX finalWorldMatrix;
    if (auto parent_ptr = _parent.lock())
    {
        finalWorldMatrix = localTransform * XMLoadFloat4x4(&parent_ptr->_worldMatrix);
    }
    else
    {
        finalWorldMatrix = localTransform;
    }

    XMStoreFloat4x4(&_worldMatrix, finalWorldMatrix);

    XMVECTOR worldScale, worldRotation, worldPosition;
    XMMatrixDecompose(&worldScale, &worldRotation, &worldPosition, finalWorldMatrix);

    XMStoreFloat3(&_position, worldPosition);
    XMStoreFloat4(&_rotation, worldRotation);
    XMStoreFloat3(&_right, XMVector3Rotate(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), worldRotation));
    XMStoreFloat3(&_up, XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), worldRotation));
    XMStoreFloat3(&_forward, XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), worldRotation));

    _isDirty = false;

    // 부모가 update 되었는데 만약 오브젝트배열의 마지막에 있다면, 지금 프레임에 업데이트가 안될수 있다.
    

    // 자식들도 연쇄적으로 업데이트하도록 dirty 플래그 설정
    for (const auto& child : _children)
    {
        child->_isDirty = true;
    }
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
    if (child) { _children.push_back(child); }
}

void TransformComponent::remove_child(std::shared_ptr<TransformComponent> child)
{
    _children.erase(std::ranges::remove(_children, child).begin(), _children.end());
}

std::shared_ptr<TransformComponent> TransformComponent::get_child(int index) const
{
    if (index < 0 || index >= _children.size()) return nullptr;
    return _children[index];
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