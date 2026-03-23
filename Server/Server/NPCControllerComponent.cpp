#include "pch.h"
#include "NPCControllerComponent.h"

#include "GameObject.h"
#include "TransformComponent.h"

namespace PIP::GAME
{
	void NPCControllerComponent::PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator)
	{
		if (!_character || !_isPhysicsActive) return;

		using namespace common::VectorHelper;

		// 1. 외부 임팩트(넉백) 감쇄 처리
		float impactSpeed = common::Length(_impactVelocity);
		if (impactSpeed > 50.0f) _impactVelocity = common::Normalize(_impactVelocity) * 50.0f; // 최대 넉백 속도 제한

		if (impactSpeed > 0.1f) {
			_impactVelocity = common::Normalize(_impactVelocity) * std::max(0.0f, impactSpeed - ImpactFriction *
				deltaTime);
		}
		else {
			_impactVelocity = common::Vec3Zero;
		}

		// 2. 최종 수평 속도 합성 (AI 이동 + 넉백)
		common::Vec3 horizontalVel;
		// 강한 넉백 상태일 때는 AI 이동을 무시하고 밀려나게 함
		if (common::Length(_impactVelocity) > 10.0f) {
			horizontalVel = _impactVelocity;
		}
		else {
			horizontalVel = _aiVelocity + _impactVelocity;
		}
		JPH::Vec3 finalJoltVel = Utils::ToJolt(horizontalVel);

		// 3. 수직 속도(중력) 처리
		float currentYVel = _character->GetLinearVelocity().GetY();
		if (_character->GetGroundState() != JPH::CharacterVirtual::EGroundState::OnGround) {
			// 공중 상태면 중력 적용
			currentYVel += _physicsSystem->GetGravity().GetY() * deltaTime;
		}
		else {
			// 지면 상태면 살짝 눌러줌
			currentYVel = -0.5f;
		}
		finalJoltVel.SetY(currentYVel);

		_character->SetLinearVelocity(finalJoltVel);

		// 4. Jolt CharacterVirtual 정밀 업데이트 (충돌 해결 포함)
		_character->Update(deltaTime, _physicsSystem->GetGravity(),
			_physicsSystem->GetDefaultBroadPhaseLayerFilter(_physicsLayer),
			_physicsSystem->GetDefaultLayerFilter(_physicsLayer),
			{}, {}, *allocator);

		// 5. NaN/Infinity 체크 및 복구 로직
		JPH::RVec3 newJoltPos = _character->GetPosition();
		if (std::isnan(newJoltPos.GetX()) || std::isinf(newJoltPos.GetX())) {
			MYERROR("NPC Physics Explosion! Resetting to safe pos.");
			_character->SetPosition(JPH::RVec3(0, 50, 0));
			_impactVelocity = { 0,0,0 };
			_aiVelocity = { 0,0,0 };
			return;
		}

		// 6. TransformComponent와 동기화 (발바닥 위치 기준)
		common::Vec3 footPos = GetPosition();
		if (auto tc = GetOwner()->GetComponent<TransformComponent>()) {
			tc->SetPosition(footPos);
		}
	}
	void NPCControllerComponent::LightPhysicsUpdate(float deltaTime)
	{
		// [NPC 전용 최적화] 시뮬레이션 없이 ShapeCast로 바닥만 체크하는 경량 모드
		if (!_character || !_isPhysicsActive) return;

		common::Vec3 currentPos = GetPosition();
		common::Vec3 vel = _aiVelocity;
		vel.y = 0;

		// 수직 속도 누적
		_verticalVelocity += _physicsSystem->GetGravity().GetY() * deltaTime;

		// 1. 예상 위치 계산
		common::Vec3 nextPos = currentPos + (vel + common::Vec3(0, _verticalVelocity, 0)) * deltaTime;

		// 2. ShapeCast로 지형 체크
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

		_physicsSystem->GetNarrowPhaseQuery().CastShape(shapeCast, 
			castSettings, 
			JPH::RVec3::sZero(), 
			collector,
			_physicsSystem->GetDefaultBroadPhaseLayerFilter(_physicsLayer),
			_physicsSystem->GetDefaultLayerFilter(_physicsLayer));

		if (collector.HadHit()) {
			float hitCenterY = (nextPos.y + startOffset + _halfHeight) - ((castDistance + startOffset) *
				collector.mHit.mFraction);
			float groundY = hitCenterY - _halfHeight;

			// 턱 높이 체크 (0.6m 이상은 벽으로 간주)
			float stepHeight = groundY - currentPos.y;
			if (stepHeight > 0.4f) {
				nextPos.x = currentPos.x;
				nextPos.z = currentPos.z;
				nextPos.y = currentPos.y;
			}
			else {
				nextPos.y = groundY;
				_verticalVelocity = -0.5f;
			}
		}

		// 3. 최종 좌표 강제 적용 및 동기화
		JPH::RVec3 joltPos = Utils::ToJolt(nextPos);
		joltPos.SetY(joltPos.GetY() + _halfHeight);
		_character->SetPosition(joltPos);

		if (auto tc = GetOwner()->GetComponent<TransformComponent>()) {
			tc->SetPosition(nextPos);
		}
	}
}
