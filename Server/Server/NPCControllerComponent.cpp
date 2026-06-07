#include "pch.h"
#include "NPCControllerComponent.h"

#include "GameObject.h"
#include "MapDataManager.h"
#include "TransformComponent.h"

namespace PIP::GAME
{
	void NPCControllerComponent::Initialize(JPH::PhysicsSystem* system, float height, float radius)
	{
		CharacterControllerComponent::Initialize(system, height, radius);
		// 생성 시점에 미리 캐싱 (GetComponent 8% 점유율 제거)
		_cachedTransform = GetOwner()->GetComponent<TransformComponent>();
	}

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
		if (!_cachedTransform)
		{
			_cachedTransform = GetOwner()->GetComponent<TransformComponent>();
			return; // 캐싱이 아직 안 된 상태에서는 업데이트 스킵 (다음 프레임에 반영)
		}

		common::Vec3 currentPos = GetPosition();

		// [최적화 1] XZ 이동이 없고 이미 접지 상태라면 BVH 탐색(CastRay) 스킵
		// _verticalVelocity가 -0.2보다 크다는 것은 이미 지면에 안착하여 리셋된 상태임을 의미
		bool isMovingXZ = (common::LengthSq(_aiVelocity) > 0.0001f || common::LengthSq(_impactVelocity) > 0.0001f);
		if (!isMovingXZ && _verticalVelocity > -0.2f) {
			return;
		}

		// --- [추가] 1. 외부 임팩트(넉백) 감쇄 처리 (PhysicsUpdate와 동일 로직) ---
		float impactSpeed = common::Length(_impactVelocity);
		if (impactSpeed > 50.0f) _impactVelocity = common::Normalize(_impactVelocity) * 50.0f; // 최대 넉백 속도 제한

		if (impactSpeed > 0.1f) {
			_impactVelocity = common::Normalize(_impactVelocity) * std::max(0.0f, impactSpeed - ImpactFriction * deltaTime);
		}
		else {
			_impactVelocity = common::Vec3Zero;
		}

		// --- [추가] 2. 최종 수평 속도 합성 (AI 이동 + 넉백) ---
		common::Vec3 horizontalVel;
		// 강한 넉백 상태일 때는 AI 이동을 무시하고 밀려나게 함
		if (common::Length(_impactVelocity) > 10.0f) {
			horizontalVel = _impactVelocity;
		}
		else {
			horizontalVel = _aiVelocity + _impactVelocity;
		}
		horizontalVel.y = 0; // 수평 속도 고정

		// 수직 속도 누적
		_verticalVelocity += _physicsSystem->GetGravity().GetY() * deltaTime;

		// 3. 예상 위치 계산 (horizontalVel 반영)
		common::Vec3 nextPos = currentPos + (horizontalVel + common::Vec3(0, _verticalVelocity, 0)) * deltaTime;

		// [최적화 2] CastShape(65%) -> CastRay(가벼움)로 교체
		// 지형 체크용으로 Ray만 쏴도 충분함 (NPC가 아주 크지 않은 이상)
		float rayStartOffset = 1.0f;
		float rayDistance = 4.0f;

		JPH::RRayCast ray{
	Utils::ToJolt(nextPos) + JPH::Vec3(0, rayStartOffset, 0),
	JPH::Vec3(0, -(rayDistance + rayStartOffset), 0)
		};

		JPH::RayCastResult rayResult;
		// 지형 레이어만 체크하여 부하 최소화
		if (_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, rayResult,
			_physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::NPC),
			_physicsSystem->GetDefaultLayerFilter(Layers::NPC)))
		{

			JPH::RVec3 hitPos = ray.mOrigin + ray.mDirection * rayResult.mFraction;
			float groundY = hitPos.GetY();

			// [최적화 3] No-Lock 인터페이스 사용 (Mutex 8% 점유율 제거)
			// 싱글스레드 로직이므로 락 없이 직접 바디 포인터 획득
			const JPH::BodyLockInterfaceNoLock& lockInterface = _physicsSystem->GetBodyLockInterfaceNoLock();
			const JPH::Body* body = lockInterface.TryGetBody(rayResult.mBodyID);

			if (body) {
				// [확인된 함수] 바디에서 법선 벡터 직접 추출
				JPH::Vec3 normal = body->GetWorldSpaceSurfaceNormal(rayResult.mSubShapeID2, hitPos);

				// [경사로 로직 수정 핵심]
				bool isSteep = (normal.GetY() < 0.6f); // 가파른가?
				bool isUpward = (groundY > currentPos.y + 0.2f); // 가려는 곳이 현재보다 높은가? (마진 0.2m)

				// 가파른데 + 올라가는 중이라면 -> "벽"으로 간주하고 XZ 이동 차단
				if (isSteep && isUpward)
				{
					nextPos.x = currentPos.x;
					nextPos.z = currentPos.z;
					groundY = currentPos.y; // 높이 변화 없음
				}
				// 가파른데 + 내려가는 중이라면 -> 차단하지 않음 (NPC가 아래로 떨어지거나 미끄러짐)
			}

			// 3. 최종 높이 적용
			nextPos.y = groundY + 0.01f;
			_verticalVelocity = -0.1f;
		}
		else {
			// 레이가 빗나갔을 때 (절벽 등) - 자유 낙하를 허용하거나 안전장치 적용
			nextPos.y = currentPos.y + _verticalVelocity * deltaTime;

			// 맵 밖 안전장치
			float mapY = MapDataManager::Instance()->GetGroundHeight(nextPos.x, nextPos.z) + 0.01f;
			if (nextPos.y < mapY) {
				nextPos.y = mapY + 0.01f;
				_verticalVelocity = 0.0f;
			}
		}

		// [최적화 3] Jolt 바디 위치 강제 동기화 (Update 생략)
		JPH::RVec3 joltPos = Utils::ToJolt(nextPos);
		joltPos.SetY(joltPos.GetY() + _halfHeight);
		_character->SetPosition(joltPos);

		// 캐싱된 포인터로 바로 접근
		_cachedTransform->SetPosition(nextPos);
	}
}
