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

    // --- Getters ---
    const XMFLOAT3& local_position() const { return _localPosition; }
    const XMFLOAT4& local_rotation() const { return _localRotation; }
    const XMFLOAT3& local_scale() const { return _localScale; }

    // [수정] 계산된 값을 반환하므로, const&가 아닌 값으로 반환합니다.
    const XMFLOAT4X4& world_matrix();
    XMFLOAT3 position();
    XMFLOAT4 rotation();
    XMFLOAT3 right();
    XMFLOAT3 up();
    XMFLOAT3 forward();

    // --- Setters ---
    void set_local_position(const XMFLOAT3& position);
    void set_local_rotation(const XMFLOAT4& rotation);
    void set_local_rotation(float pitch, float yaw, float roll);
    void set_local_scale(const XMFLOAT3& scale);

	// DW설명 : 현재 위치에서 forward, right, up 방향으로 오프셋만큼 이동
	void move_forward(float distance);
	void move_right(float distance);
	void move_up(float distance);


	// DW설명 : 월드 크기 함수 mehs의 바운딩 박스를 가져와 월드공간으로 이전 후 max-min하여 실제 가로 세로 높이 값을 반환
    XMFLOAT3 get_world_scale();

    // [추가] 현재 회전에 pitch, yaw, roll을 추가로 적용합니다. (각도는 degree 단위)
	// DW설명 : 마지막 인자는 카메라 회전용인지 구분하는 플래그
    void rotate(float pitch, float yaw, float roll);

	// DW설명 : 카메라 전용 회전 함수
    void camera_rotate(float pitch, float yaw, float roll);

    // --- Hierarchy Management ---
    void set_parent(std::shared_ptr<TransformComponent> parent);
    std::weak_ptr<TransformComponent> parent() const { return _parent; }
    std::shared_ptr<TransformComponent> child(int index) const;
    const std::vector<std::shared_ptr<TransformComponent>>& children() const { return _children; }
    int child_count() const { return static_cast<int>(_children.size()); }

protected:
    // [추가] 자신과 모든 자식의 isDirty 플래그를 true로 설정하는 재귀 함수
    void set_hierarchy_dirty();

    // [변경] 이제 이 함수는 부모를 거슬러 올라가지 않고, 자신의 행렬만 계산
    void calculate_world_matrix();

    void add_child(std::shared_ptr<TransformComponent> child);
    void remove_child(std::shared_ptr<TransformComponent> child);

    // Local space data
    XMFLOAT3 _localPosition;
    XMFLOAT4 _localRotation;
    XMFLOAT3 _localScale;
    bool _isDirty;

    // World space data (캐시된 값)
    XMFLOAT4X4 _worldMatrix;

    // Hierarchy Data
    std::weak_ptr<TransformComponent> _parent;
    std::vector<std::shared_ptr<TransformComponent>> _children;
};