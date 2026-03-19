#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "Behavior.h"

class PhysicsColliderComponent : public Behavior
{
public:
	enum class ShapeType { Box, Sphere, Capsule };

	PhysicsColliderComponent();
	~PhysicsColliderComponent() override;

	// 사용하기 쉬운 인터페이스
	// size: Box면 (가로,세로,높이), Sphere면 (x=반지름), Capsule이면 (x=반경, y=절반높이)
	void initialize(ShapeType type, const XMFLOAT3& size,
	                const f3& center = {0,0,0},
	                const f3& rotation_offset = { 0,0,0 },
	                bool isSensor = true);

	// 공격 중에만 켜기 위해 사용
	void set_active(bool active);
	bool is_active() const { return _isActive; }
	ShapeType shape_type() const { return _shapeType; }

	// 특정 콜백 등록 (예: WeaponScript에서 공격 패킷 보내기용)
	void set_on_collision_callback(std::function<void(std::shared_ptr<GameObject>)> callback)
	{
		_onCollision = callback;
	}
	// GameObject 루프에 의해 자동 호출
	void fixed_update(float deltaTime) override;

	// PhysicsManager로부터 전달받는 충돌 알림
	void OnContact(std::shared_ptr<GameObject> other);
	const JPH::Shape* get_shape() {};

private:
	void create_body();

	ShapeType	_shapeType = ShapeType::Box;
	XMFLOAT3	_size = { 1.f, 1.f, 1.f };
	JPH::Ref<JPH::Shape> _shape;
	bool		_isSensor = true;
	bool		_isActive = false;

	JPH::BodyID _bodyID;
	std::function<void(std::shared_ptr<GameObject>)> _onCollision;
	f3 _center;
	f3 _rotationOffset;
};
