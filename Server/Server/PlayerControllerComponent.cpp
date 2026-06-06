#include "pch.h"
#include "PlayerControllerComponent.h"

#include "Actor.h"
#include "GameObject.h"
#include "TransformComponent.h"

namespace PIP::GAME
{
    void PlayerControllerComponent::PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator) {
        if (!_character) return;

        // [추가] 잡힌 상태일 때는 물리 엔진에 의한 자체 이동 및 조작을 완전히 차단
        if (auto actor = dynamic_cast<Actor*>(GetOwner())) {
            if (actor->GetState() == common::packet::EntityState::GRABBED) {
                _character->SetLinearVelocity(JPH::Vec3::sZero());
                _currentMoveVelocity = { 0, 0, 0 };
                _targetMoveVelocity = { 0, 0, 0 };
                _impactVelocity = { 0, 0, 0 };

                // [중요] CharacterVirtual의 위치는 Transform의 위치와 동기화되어야 함 (보스가 이동시키므로)
                auto tc = GetOwner()->GetComponent<TransformComponent>();
                if (tc) {
                    common::Vec3 pos = tc->GetPosition();
                    _character->SetPosition(Utils::ToJolt(pos + common::Vec3(0, _halfHeight, 0)));
                }
                return;
            }
        }

        float impactSpeed = common::Length(_impactVelocity);
        if (impactSpeed > 0.1f) {
            float reduction = 40.0f * deltaTime;
            _impactVelocity = common::Normalize(_impactVelocity) * std::max(0.0f, impactSpeed - reduction);
        }
        else {
            _impactVelocity = { 0, 0, 0 };
        }

        // 부드러운 가감속 (Lerp) 적용
        float moveDeceleration = 15.0f; // 가감속 계수 (클라이언트와 동일해야 함!)
        float t = std::min(deltaTime * moveDeceleration, 1.0f);
        _currentMoveVelocity = _currentMoveVelocity + (_targetMoveVelocity - _currentMoveVelocity) * t;

        // 2. 최종 수평 속도 합성
        common::Vec3 horizontalInput;
        // [핵심] 넉백 속도가 일정 이상이면 플레이어 조작(_moveVelocity)을 완전히 무시
        if (common::LengthSq(_impactVelocity) > 5.0f * 5.0f) {
            horizontalInput = _impactVelocity;
        }
        else {
            // 혹시몰라서 기존 코드 남겨놓음
            // horizontalInput = _moveVelocity + _impactVelocity;

            // [수정] _moveVelocity 대신 _currentMoveVelocity 사용
            horizontalInput = _currentMoveVelocity + _impactVelocity;
        }

        JPH::Vec3 groundVelocity = JPH::Vec3::sZero();
        if (_character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) {
            groundVelocity = _character->GetGroundVelocity();
        }

        // 1. 중력 및 속도 계산
        JPH::Vec3 gravity = _physicsSystem->GetGravity();
        JPH::Vec3 currentVel = _character->GetLinearVelocity();
        // 수직 속도 (중력 누적)
        float newYVel = currentVel.GetY() + gravity.GetY() * deltaTime;

        // 땅에 있을 때 중력 캡핑 (파고듦 방지 핵심)
        if (_character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) {
            // [핵심 수정] 0.0f로 강제 고정하면 엘리베이터가 내려갈 때 붕 뜹니다.
            // 지면의 Y 속도 이하로 떨어지지 않게 제한해야 플랫폼과 함께 자연스럽게 내려갑니다.
            // (살짝 더 아래로 눌러주는 -0.1f 오프셋을 주면 StickToFloor와 시너지가 좋습니다)
            // 단, 가만히 있거나 공격 중일 때(수평 이동이 없을 때) -0.1f를 계속 주면 바닥으로 미세하게 가라앉으므로 이동 중에만 적용합니다.
            if (common::LengthSq(horizontalInput) > 0.001f) {
                newYVel = std::max(newYVel, groundVelocity.GetY() - 0.1f);
            } else {
                newYVel = std::max(newYVel, groundVelocity.GetY());
            }
        }

        // 최종 속도 설정
        _character->SetLinearVelocity(JPH::Vec3(
            horizontalInput.x + groundVelocity.GetX(),
            newYVel,
            horizontalInput.z + groundVelocity.GetZ()));

        // 2. ExtendedUpdate 설정 (여기서 StepUp 높이를 조절합니다)
        JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;

        // 요철을 잘 넘게 하려면 mWalkStairsStepUp을 조절 (기본값 0.4f)
        updateSettings.mWalkStairsStepUp = JPH::Vec3(0, 1.f, 0);
        // 경사로에서 뜨지 않게 하려면 mStickToFloorStepDown 조절
        updateSettings.mStickToFloorStepDown = JPH::Vec3(0, 1.5f, 0);

        // 3. 물리 시뮬레이션 실행 (레이어 필터 확인 필수!)
        // 지형(NON_MOVING)이 포함된 레이어를 사용해야 합니다.
        _character->ExtendedUpdate(deltaTime,
            gravity,
            updateSettings,
            _physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
            _physicsSystem->GetDefaultLayerFilter(Layers::MOVING),
            {}, {}, *allocator);

        // 4. 위치 동기화 (Jolt 결과 -> Transform)
        JPH::RVec3 newJoltPos = _character->GetPosition();
        common::Vec3 footPos = Utils::FromJolt(newJoltPos);
        footPos.y -= _halfHeight;

        

        auto tc = GetOwner()->GetComponent<TransformComponent>();
        if (tc) tc->SetPosition(footPos);

        //_timer -= deltaTime;
        //if (_timer < 0.0f)
        //{
        //    // 1. 서버 로직에서 관리하는 발바닥 위치 (get_position() 등)
        //    auto logicFootPos = GetPosition();

        //    // 2. Jolt 물리 바디의 실제 중심 위치 (Jolt 내부의 진짜 좌표)
        //    JPH::RVec3 joltBodyPos = _character->GetPosition();

        //    // 3. Jolt가 판단하는 현재 캐릭터의 접지 상태
        //    auto groundState = _character->GetGroundState();
        //    const char* groundStateStr = "Unknown";
        //    switch (groundState) {
        //    case JPH::CharacterVirtual::EGroundState::OnGround:     groundStateStr = "OnGround"; break;
        //    case JPH::CharacterVirtual::EGroundState::OnSteepGround:groundStateStr = "OnSteepGround"; break;
        //    case JPH::CharacterVirtual::EGroundState::NotSupported: groundStateStr = "NotSupported"; break;
        //    case JPH::CharacterVirtual::EGroundState::InAir:        groundStateStr = "InAir"; break;
        //    }

        //    // 4. 로그 출력
        //    MYLOG("[Jolt Debug] Actor: " << GetOwner()->GetName()
        //        << " | Logic Foot: (" << logicFootPos.x << ", " << logicFootPos.y << ", " << logicFootPos.z << ")"
        //        << " | Jolt Center: (" << joltBodyPos.GetX() << ", " << joltBodyPos.GetY() << ", " << joltBodyPos.GetZ()
        //        << ")"
        //        << " | GroundState: " << groundStateStr);

        //    auto pos = GetPosition();
        //    MYLOG("player pos (" << pos.x << "," << pos.y << "," << pos.z << ")");
        //    _timer = 2.0f;
        //}


		// 이것도 수정 -> 혹시 몰라서 남겨놓음
        // _moveVelocity = { 0, 0, 0 };
    }
}
