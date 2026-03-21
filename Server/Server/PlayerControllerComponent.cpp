#include "pch.h"
#include "PlayerControllerComponent.h"

#include "GameObject.h"
#include "TransformComponent.h"

namespace PIP::GAME
{
    void PlayerControllerComponent::PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator) {
        if (!_character) return;

        // 1. 현재 상태 및 입력 기반 속도 계산
        common::Vec3 currentPos = GetPosition(); // 현재 '발바닥' 위치
        common::Vec3 vel = _moveVelocity + _impactVelocity;

        // 현재 캐릭터의 수직(Y) 속도 유지 (중력 적용 전)
        float currentYVel = _character->GetLinearVelocity().GetY();

        // 수평 이동 예측 (XZ)
        common::Vec3 nextPos = currentPos + (common::Vec3(vel.x, 0, vel.z) * deltaTime);

        // 중력 적용 (공중에 있을 때 아래로 가속)
        currentYVel += _physicsSystem->GetGravity().GetY() * deltaTime;
        nextPos.y += currentYVel * deltaTime;

        // 2. [핵심] ShapeCast로 지형 높이 직접 스캔
        // 머리 위 1.5m에서 아래로 5m까지 캡슐 모양으로 훑음 (끼임/통과 방지)
        float startOffset = 1.5f;
        float castDistance = 20.0f;

        JPH::RShapeCast shapeCast{
            _character->GetShape(),
            JPH::Vec3::sReplicate(1.0f),
            JPH::RMat44::sTranslation(Utils::ToJolt(nextPos) + JPH::Vec3(0, startOffset + _halfHeight, 0)),
            JPH::Vec3(0, -(castDistance + startOffset), 0)
        };

        JPH::ShapeCastSettings castSettings;
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;

        // 지형(NON_MOVING) 레이어에 대해서만 바닥 체크 수행
        _physicsSystem->GetNarrowPhaseQuery().CastShape(shapeCast, castSettings, JPH::RVec3::sZero(), collector,
            _physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
            _physicsSystem->GetDefaultLayerFilter(Layers::MOVING));

        if (collector.HadHit()) {
            // 충돌 지점에서 캡슐의 '중심' 높이 계산
            float hitCenterY = (nextPos.y + startOffset + _halfHeight) - ((castDistance + startOffset) *
                collector.mHit.mFraction);
            // '발바닥' 높이로 변환
            float groundY = hitCenterY - _halfHeight;

            // [턱 높이/경사 보정]
            // 현재 높이보다 너무 높지 않은 곳(0.8m 이내)이라면 즉시 지면으로 안착
            float stepHeight = groundY - currentPos.y;
            if (stepHeight < 0.8f) {
                nextPos.y = groundY;
                currentYVel = -0.5f; // 바닥에 붙었으므로 수직 속도 초기화 (접지 안정성)
            }
        }

        // 3. 물리 엔진 강제 동기화 (SetPosition으로 결과 고정)
        // Jolt 내부의 CharacterVirtual 위치를 서버가 계산한 nextPos로 강제 견인
        JPH::RVec3 finalJoltPos = Utils::ToJolt(nextPos);
        finalJoltPos.SetY(finalJoltPos.GetY() + _halfHeight); // 중심점으로 변환하여 입력
        _character->SetPosition(finalJoltPos);

        // 속도 정보도 동기화 (XZ 입력 + Y 중력)
        _character->SetLinearVelocity(JPH::Vec3(vel.x, currentYVel, vel.z));

        // 4. Transform 컴포넌트 업데이트 (시각적 위치와 서버 로직 좌표 일치)
        auto tc = GetOwner()->GetComponent<TransformComponent>();
        if (tc) tc->SetPosition(nextPos);

        // 5. 외부 임팩트(넉백 등) 감쇄 처리
        float impactSpeed = common::Length(_impactVelocity);
        if (impactSpeed > 0.1f) {
            _impactVelocity = common::Normalize(_impactVelocity) * std::max(0.0f, impactSpeed - ImpactFriction *
                deltaTime);
        }
        else {
            _impactVelocity = common::Vec3Zero;
        }
		_timer -= deltaTime;
        if (_timer < 0.0f)
        {
            auto pos = GetPosition();
            MYLOG("player pos (" << pos.x << "," << pos.y << "," << pos.z << ")");
			_timer = 2.0f; // 5초마다 위치 로그 출력
        }
		
    }
}
