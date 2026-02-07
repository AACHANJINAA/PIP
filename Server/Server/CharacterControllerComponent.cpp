#include "pch.h"
#include "CharacterControllerComponent.h"
#include "GameObject.h"
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

	void CharacterControllerComponent::PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator) {
		if (!_character) return;

		using namespace common::VectorHelper;
		// 1. 외부 임팩트(넉백) 감쇄 처리
		float speed = Length(_impactVelocity);
		if (speed > 0.1f) {
			_impactVelocity =  Normalize(_impactVelocity) * std::max(0.0f, speed - ImpactFriction * deltaTime);
		}
		else {
			_impactVelocity = common::Vec3Zero;
		}

		// 2. 최종 수평 속도 합성 (AI + 넉백)
		common::Vec3 horizontalVel = _aiVelocity + _impactVelocity;
		JPH::Vec3 finalJoltVel = Utils::ToJolt(horizontalVel);

		// 3. 수직 속도(중력) 처리 - [핵심] 기존 Y 속도를 가져와서 중력 누적
		float currentYVel = _character->GetLinearVelocity().GetY();
		if (_character->GetGroundState() != JPH::CharacterVirtual::EGroundState::OnGround) {
			// 공중에 있다면 중력 가속도 적용
			currentYVel += _physicsSystem->GetGravity().GetY() * deltaTime;
		}
		else {
			// 땅에 있다면 아주 살짝만 아래로 눌러줌
			currentYVel = -1.0f;
		}
		finalJoltVel.SetY(currentYVel);

		_character->SetLinearVelocity(finalJoltVel);

		// 3. Jolt CharacterVirtual 업데이트 (시뮬레이션이 아닌 '충돌 해결'만 수행)
		_character->Update(deltaTime, _physicsSystem->GetGravity(),
			_physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::NPC),
			_physicsSystem->GetDefaultLayerFilter(Layers::NPC), 
			{}, {}, *allocator);

		// 4. GameObject의 Transform과 동기화
		JPH::RVec3 centerPos = _character->GetPosition();
		// [중요] Jolt 중심 좌표를 다시 발바닥 좌표로 변환하여 Transform에 저장
		common::Vec3 footPos = Utils::FromJolt(centerPos);
		footPos.y -= _halfHeight; // 반 높이만큼 내려줘야 발바닥 기준이 됩니다.

		auto tc = GetOwner()->GetComponent<TransformComponent>();
		if (tc) tc->SetPosition(footPos);
	}

	void CharacterControllerComponent::SetVelocity(const common::Vec3& velocity)
	{
		_aiVelocity = velocity;
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

	void CharacterControllerComponent::AddImpulse(const common::Vec3& impulse)
	{
		if (_character)
		{
			JPH::Vec3 currentVel = _character->GetLinearVelocity();
			_character->SetLinearVelocity(currentVel + Utils::ToJolt(impulse));
		}
	}

	bool CharacterControllerComponent::IsGrounded() const
	{
		return _character ? _character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround : false;
	}
}
