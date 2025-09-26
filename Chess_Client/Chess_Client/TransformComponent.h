#pragma once
#include "Component.h"
#include <DirectXMath.h>
#include <vector>
#include <memory>

using namespace DirectX;

class TransformComponent : public Component, public std::enable_shared_from_this<TransformComponent>
{
public:
    TransformComponent();
    virtual ~TransformComponent() = default;

    void update();

    // --- Getters ---
	// --- Local Space Getters ---
	// 부모 기준 위치, 회전, 스케일
    const XMFLOAT3& local_position() const { return _localPosition; }
    const XMFLOAT4& local_rotation() const { return _localRotation; }
    const XMFLOAT3& local_scale() const { return _localScale; }

	// 월드 기준 위치, 회전, 방향 벡터
    const XMFLOAT3&     position();
    const XMFLOAT4&     rotation();
    const XMFLOAT3&     right();
    const XMFLOAT3&     up();
    const XMFLOAT3&     forward();
    const XMFLOAT4X4&   world_matrix();

    // --- Setters ---
    void set_local_position(const XMFLOAT3& position);
    void set_local_rotation(const XMFLOAT4& rotation);
    void set_local_scale(const XMFLOAT3& scale);

    // --- Hierarchy Management ---
    void set_parent(std::shared_ptr<TransformComponent> parent);
    std::weak_ptr<TransformComponent> parent() const { return _parent; }
    std::shared_ptr<TransformComponent> child(int index) const;
    std::vector<std::shared_ptr<TransformComponent>> children() const;
    int child_count() const { return static_cast<int>(_children.size()); }

private:
    void add_child(std::shared_ptr<TransformComponent> child);
    void remove_child(std::shared_ptr<TransformComponent> child);
    void force_update_hierarchy();
    // Local space data
    XMFLOAT3 _localPosition;
    XMFLOAT4 _localRotation;
    XMFLOAT3 _localScale;
    bool _isDirty;

    // World space data (pre-calculated)
    XMFLOAT4X4 _worldMatrix;
    XMFLOAT3 _position;
    XMFLOAT4 _rotation;
    XMFLOAT3 _right;
    XMFLOAT3 _up;
    XMFLOAT3 _forward;

    // Hierarchy Data
    std::weak_ptr<TransformComponent> _parent;
    std::vector<std::shared_ptr<TransformComponent>> _children;
};

