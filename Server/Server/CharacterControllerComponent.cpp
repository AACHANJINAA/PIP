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
		float impactSpeed = common::Length(_impactVelocity);
		if (impactSpeed > 50.0f) _impactVelocity = common::Normalize(_impactVelocity) * 50.0f; // 최대 넉백 속도 제한

		if (impactSpeed > 0.1f) {
			_impactVelocity = common::Normalize(_impactVelocity) * std::max(0.0f, impactSpeed - ImpactFriction * deltaTime);
		}
		else {
			_impactVelocity = common::Vec3Zero;
		}

		// 2. 최종 수평 속도 합성 (AI + 넉백)
		common::Vec3 horizontalVel;
		if (common::Length(_impactVelocity) > 10.0f) { // 강하게 맞았을 때
			horizontalVel = _impactVelocity; // 유저 조작 무시 (스턴 느낌)
		}
		else {
			horizontalVel = _aiVelocity + _impactVelocity; // 평상시 합성
		}
		JPH::Vec3 finalJoltVel = Utils::ToJolt(horizontalVel);

		// 3. 수직 속도(중력) 처리 - [핵심] 기존 Y 속도를 가져와서 중력 누적
		float currentYVel = _character->GetLinearVelocity().GetY();
		if (_character->GetGroundState() != JPH::CharacterVirtual::EGroundState::OnGround) {
			// 공중에 있다면 중력 가속도 적용
			currentYVel += _physicsSystem->GetGravity().GetY() * deltaTime;
		}
		else {
			// 땅에 있다면 아주 살짝만 아래로 눌러줌
			currentYVel = -0.5f;
		}
		finalJoltVel.SetY(currentYVel);

		_character->SetLinearVelocity(finalJoltVel);

		// 3. Jolt CharacterVirtual 업데이트 (시뮬레이션이 아닌 '충돌 해결'만 수행)
		_character->Update(deltaTime, _physicsSystem->GetGravity(),
			_physicsSystem->GetDefaultBroadPhaseLayerFilter(_physicsLayer),
			_physicsSystem->GetDefaultLayerFilter(_physicsLayer),
			{}, {}, *allocator);

		// 4. [핵심] NaN 체크 및 강제 복구
		JPH::RVec3 newJoltPos = _character->GetPosition();
		if (std::isnan(newJoltPos.GetX()) || std::isinf(newJoltPos.GetX())) {
			MYERROR("Physics Explosion Detected! Resetting to safe position.");
			_character->SetPosition(JPH::RVec3(0, 50, 0)); // 안전한 위치로 강제 견인
			_impactVelocity = { 0,0,0 };
			_aiVelocity = { 0,0,0 };
			return;
		}

		// 5. GameObject의 Transform과 동기화
		common::Vec3 footPos = this->GetPosition(); // 보정된 발바닥 좌표
		auto tc = GetOwner()->GetComponent<TransformComponent>();
		if (tc) tc->SetPosition(footPos);
	}

	void CharacterControllerComponent::LightPhysicsUpdate(float deltaTime) {
		if (!_character) return;

		common::Vec3 currentPos = GetPosition();
		common::Vec3 vel = _aiVelocity;
		vel.y = 0;

		_verticalVelocity += _physicsSystem->GetGravity().GetY() * deltaTime;

		// [1단계] 먼저 수평 이동만 적용한 위치를 봅니다.
		common::Vec3 nextPos = currentPos + (vel + common::Vec3(0, _verticalVelocity, 0)) * deltaTime;

		// [2단계] ShapeCast로 바닥 체크
		float castDistance = std::max(3.0f, std::abs(_verticalVelocity * deltaTime) + 1.0f);
		float startOffset = 1.0f; // 머리 위 1m부터 체크

		JPH::RShapeCast shapeCast{
			_settings->mShape,
			JPH::Vec3::sReplicate(1.0f),
			JPH::RMat44::sTranslation(Utils::ToJolt(nextPos) + JPH::Vec3(0, startOffset + _halfHeight, 0)),
			JPH::Vec3(0, -(castDistance + startOffset), 0)
		};

		JPH::ShapeCastSettings castSettings;
		JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;

		_physicsSystem->GetNarrowPhaseQuery().CastShape(shapeCast, castSettings, JPH::RVec3::sZero(), collector,
			_physicsSystem->GetDefaultBroadPhaseLayerFilter(_physicsLayer),
			_physicsSystem->GetDefaultLayerFilter(_physicsLayer));

		if (collector.HadHit()) {
			float hitCenterY = (nextPos.y + startOffset + _halfHeight) - ((castDistance + startOffset) *
				collector.mHit.mFraction);
			float groundY = hitCenterY - _halfHeight;

			// [핵심] 턱 높이 체크 (Step Height)
			// 현재 위치보다 0.6m 이상 높은 곳은 "벽"으로 간주하고 이동을 차단합니다.
			float stepHeight = groundY - currentPos.y;

			if (stepHeight > 0.6f) {
				// 너무 높음! 건물 벽이거나 높은 장애물임.
				// 수평 이동을 취소하고 이전 위치로 되돌림 (벽에 막힌 효과)
				nextPos.x = currentPos.x;
				nextPos.z = currentPos.z;

				// Y값은 현재 위치의 바닥을 다시 찾거나 유지
				nextPos.y = currentPos.y;
			}
			else {
				// 올라갈 수 있는 낮은 턱이나 완만한 경사임.
				nextPos.y = groundY;
				_verticalVelocity = -0.5f;
			}
		}

		// 3. 최종 좌표 적용
		JPH::RVec3 joltPos = Utils::ToJolt(nextPos);
		joltPos.SetY(joltPos.GetY() + _halfHeight);
		_character->SetPosition(joltPos);

		auto tc = GetOwner()->GetComponent<TransformComponent>();
		if (tc) tc->SetPosition(nextPos);
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
		if (!_character) return common::Vec3Zero;
		JPH::RVec3 centerPos = _character->GetPosition();
		common::Vec3 footPos = Utils::FromJolt(centerPos);
		footPos.y -= _halfHeight; // 중심에서 발바닥으로
		return footPos;
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

	const JPH::Shape* CharacterControllerComponent::GetShape() const {
		if (_character) return _character->GetShape();
		return nullptr;
	}
}
