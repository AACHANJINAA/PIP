#include "pch.h"
#include "CharacterControllerComponent.h"

#include <algorithm>

#include "TransformComponent.h"

namespace PIP::GAME
{
	CharacterControllerComponent::~CharacterControllerComponent()
	{
		// Jolt Ref 카운팅에 의해 객체 해제 관리
		_character = nullptr;
		_settings = nullptr;
	}

	void CharacterControllerComponent::Initialize(JPH::PhysicsSystem* physicsSystem, float height, float radius)
	{
		_physicsSystem = physicsSystem;
		if (!_physicsSystem) return;

		// 1. 순수한 캡슐 생성 (중심이 0,0,0)
		// 복잡한 RotatedTranslatedShape 제거 -> 물리 연산 안정성 확보
		float halfCylinderHeight = 0.5f * height - radius;
		halfCylinderHeight = std::max(halfCylinderHeight, 0.1f);

		JPH::Ref<JPH::Shape> capsuleShape = new JPH::CapsuleShape(halfCylinderHeight, radius);

		// 2. 초기 위치 설정 (발바닥 -> 허리 변환)
		// Transform은 발바닥 위치지만, 캡슐은 중심이 기준이므로 '반 높이'만큼 올려줍니다.
		_halfHeight = 0.5f * height;

		auto transform = GetComponent<TransformComponent>();
		JPH::RVec3 startPos = JPH::RVec3::sZero();
		if (transform)
		{
			startPos = Utils::ToJolt(transform->GetPosition());
			startPos.SetY(startPos.GetY() + _halfHeight); // [핵심] 땅에 박히지 않게 허리만큼 올림
		}

		// 3. 컨트롤러 설정
		_settings = new JPH::CharacterVirtualSettings();
		_settings->mShape = capsuleShape;
		// SupportingVolume도 캡슐 주변을 감싸도록 설정
		_settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -_halfHeight); // 발바닥 위치 인식
		_settings->mMaxSlopeAngle = JPH::DegreesToRadians(50.0f);
		_settings->mUp = JPH::Vec3::sAxisY();

		// 4. 생성
		_character = new JPH::CharacterVirtual(_settings, startPos, JPH::Quat::sIdentity(), _physicsSystem);
		_character->SetListener(&_contactListener);
	}

	void CharacterControllerComponent::PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator)
	{
		if (!_character || !_physicsSystem) return;

		// 1. 중력 적용
		JPH::Vec3 velocity = _character->GetLinearVelocity();
		JPH::Vec3 gravity = _physicsSystem->GetGravity();
		// [추가] 바닥 상태 체크 및 중력 처리
		if (_character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround)
		{
			// 땅에 있을 때는 Y 속도를 0으로 (또는 아주 작은 값으로 유지해 바닥 밀착)
			// 만약 점프 중이 아니라면:
			if (velocity.GetY() < 0.0f) {
				velocity.SetY(std::max(velocity.GetY(), -1.0f)); // 약간만 눌러줌
			}
		}
		else
		{
			// 공중에 있을 때만 중력 누적
			velocity += gravity * deltaTime;
		}
		_character->SetLinearVelocity(velocity);

		// 2. 필터 설정
		JPH::DefaultBroadPhaseLayerFilter broadPhaseFilter(
			_physicsSystem->GetObjectVsBroadPhaseLayerFilter(), Layers::NPC);
		JPH::DefaultObjectLayerFilter objectFilter(
			_physicsSystem->GetObjectLayerPairFilter(), Layers::NPC);

		JPH::BodyFilter bodyFilter;
		JPH::ShapeFilter shapeFilter;

		// 3. 업데이트
		_character->Update(deltaTime, gravity, broadPhaseFilter, objectFilter, bodyFilter, shapeFilter, *allocator);


		// 4. Transform 동기화 (허리 -> 발바닥 변환)
		auto transform = GetComponent<TransformComponent>();
		if (transform)
		{
			JPH::RVec3 centerPos = _character->GetPosition();

			centerPos.SetY(centerPos.GetY() - _halfHeight); // [핵심] 다시 발바닥으로 내림

			transform->SetPosition(Utils::FromJolt(centerPos));
		}
	}

	void CharacterControllerComponent::SetVelocity(const common::Vec3& velocity)
	{
		if (_character)
		{
			JPH::Vec3 currentVel = _character->GetLinearVelocity();
			JPH::Vec3 inputVel = Utils::ToJolt(velocity);
			// X, Z축 이동만 덮어쓰고 Y축(중력)은 유지하여 자연스러운 낙하 구현
			inputVel.SetY(currentVel.GetY());
			_character->SetLinearVelocity(inputVel);
		}
	}
	common::Vec3 CharacterControllerComponent::GetVelocity() const
	{
		return _character ? Utils::FromJolt(_character->GetLinearVelocity()) : common::Vec3{ 0,0,0 };
	}

	void CharacterControllerComponent::SetPosition(const common::Vec3& position)
	{
		if (_character)
		{
			// 입력받은 position은 발바닥 기준
			JPH::RVec3 joltPos = Utils::ToJolt(position);
			// 허리 위치로 보정해서 넣기
			joltPos.SetY(joltPos.GetY() + _halfHeight);
			_character->SetPosition(joltPos);
		}
	}
	common::Vec3 CharacterControllerComponent::GetPosition() const
	{
		return _character ? Utils::FromJolt(_character->GetPosition()) : common::Vec3{ 0,0,0 };
	}

	bool CharacterControllerComponent::IsGrounded() const
	{
		return _character ? _character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround : false;
	}
}
