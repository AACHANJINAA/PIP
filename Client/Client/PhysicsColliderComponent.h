#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "Behavior.h"

class PhysicsColliderComponent : public Behavior
{
public:
	enum class ShapeType { Box, Sphere, Capsule };

	// Body: Jolt 바디를 만들어 물리 접촉을 받음 (기존 방식)
	// QueryOnly: 바디 없이 모양과 월드 변환만 보관 (겹침 검사, 디버그 기록용)
	enum class BodyMode { Body, QueryOnly };

	PhysicsColliderComponent();
	~PhysicsColliderComponent() override;

	// 사용하기 쉬운 인터페이스
	// size: Box면 (가로,세로,높이), Sphere면 (x=반지름), Capsule이면 (x=반경, y=절반높이)
	// center, rotation_offset(도): 붙은 기준(오브젝트 또는 뼈) 좌표계에서의 오프셋
	void initialize(ShapeType type, const XMFLOAT3& size,
	                const f3& center = {0,0,0},
	                const f3& rotation_offset = { 0,0,0 },
	                bool isSensor = true,
	                BodyMode bodyMode = BodyMode::Body);

	// 같은 오브젝트의 AnimationComponent 뼈에 부착 (호출하지 않으면 오브젝트 트랜스폼에 부착)
	void attach_to_bone(const std::string& bone_name);
	void detach_from_bone();
	const std::string& attached_bone() const { return _attachedBone; }
	bool is_attached_to_bone() const { return !_attachedBone.empty(); }

	// 공격 중에만 켜기 위해 사용
	void set_active(bool active);
	bool is_active() const { return _isActive; }
	ShapeType shape_type() const { return _shapeType; }
	BodyMode body_mode() const { return _bodyMode; }

	// 특정 콜백 등록 (예: WeaponScript에서 공격 패킷 보내기용)
	void set_on_collision_callback(std::function<void(std::shared_ptr<GameObject>)> callback)
	{
		_onCollision = callback;
	}
	// GameObject 루프에 의해 자동 호출
	void fixed_update(float deltaTime) override;
	// 애니메이션 이후 월드 변환 계산 (현재/직전 프레임 보관)
	void late_update(float deltaTime) override;

	// PhysicsManager로부터 전달받는 충돌 알림
	void OnContact(std::shared_ptr<GameObject> other);
	const JPH::Shape* get_shape() const { return _shape.GetPtr(); }

	// late_update에서 계산된 월드 변환 (스케일 제거됨). 업데이트 단계에서 읽으면 직전 프레임 값
	bool has_world_transform() const { return _hasWorldTransform; }
	const JPH::RMat44& world_transform() const { return _worldTransform; }
	const JPH::RMat44& prev_world_transform() const { return _prevWorldTransform; }

private:
	void create_body();
	bool compute_world_transform(JPH::RMat44& out);

	ShapeType	_shapeType = ShapeType::Box;
	BodyMode	_bodyMode = BodyMode::Body;
	XMFLOAT3	_size = { 1.f, 1.f, 1.f };
	JPH::Ref<JPH::Shape> _shape;
	bool		_isSensor = true;
	bool		_isActive = false;

	JPH::BodyID _bodyID;
	std::function<void(std::shared_ptr<GameObject>)> _onCollision;
	f3 _center;
	f3 _rotationOffset;

	// 뼈 부착
	std::string _attachedBone;
	bool _boneErrorLogged = false;
	bool _scaleWarningLogged = false;

	// 월드 변환 (late_update에서 갱신)
	bool _hasWorldTransform = false;
	JPH::RMat44 _worldTransform = JPH::RMat44::sIdentity();
	JPH::RMat44 _prevWorldTransform = JPH::RMat44::sIdentity();
};
