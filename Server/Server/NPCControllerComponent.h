#pragma once
#include <optional>
#include "CharacterControllerComponent.h"
#include "MotionCurve.h"

namespace PIP::GAME
{
	class TransformComponent;
    class NPCControllerComponent : public CharacterControllerComponent {
    public:
        using CharacterControllerComponent::CharacterControllerComponent;

        // 초기화 시 컴포넌트 캐싱 (OnAdd 또는 별도 Init에서 호출)
        void Initialize(JPH::PhysicsSystem* system, float height, float radius) override;
        void Initialize() override {}
        void PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator) override;
        void LightPhysicsUpdate(float deltaTime); // LOD용 경량 물리
        void SetVelocity(const common::Vec3& velocity) { _aiVelocity = velocity; }
        void ResetVerticalVelocity() { _verticalVelocity = -1.0f; } // [추가] 리스폰 시 중력 로직 강제 진입용

        // --- 모션 이벤트 (NetMotionSync_Design_KR.md 5.3) ---
        struct MotionInfo
        {
            uint32_t id = 0;
            common::motion::MotionType type = common::motion::MotionType::Knockback;
            common::motion::MotionCurve curve = common::motion::MotionCurve::EaseOutQuad;
            double startTime = 0.0;     // 서버 NetNowMsPrecise (ms)
            common::Vec3 startPos = { 0, 0, 0 };
            common::Vec3 endPos = { 0, 0, 0 };
            float hold = 0.0f;          // 초
            float duration = 0.0f;      // 초
        };
        struct MotionEndInfo
        {
            uint32_t id = 0;
            double endTime = 0.0;
            common::Vec3 endPos = { 0, 0, 0 };
        };

        // 피격 넉백을 모션으로 시작. 넉백 경로를 구체 스윕으로 미리 검사해서 벽 앞에서 멈추는 끝 위치를 확정하고,
        // 동작 중에는 AI 이동·충격량 대신 곡선(버팀 -> 일정 감속)대로 옮긴다. 밀릴 거리가 없으면 시작하지 않음
        // (LightPhysicsUpdate에는 수평 충돌 검사가 없어서 끝 위치를 미리 확정해야 벽을 뚫지 않음)
        void StartKnockback(const common::Vec3& impulse);
        void CancelMotion() { _motionActive = false; _startedMotion.reset(); _interruptedMotion.reset(); }
        bool IsInMotion() const { return _motionActive; }
        // 브로드캐스트용: 새로 시작한 모션 / 예정보다 일찍 끝난 모션 (꺼내면 비움)
        std::optional<MotionInfo> TakeStartedMotion() { auto m = _startedMotion; _startedMotion.reset(); return m; }
        std::optional<MotionEndInfo> TakeInterruptedMotion() { auto m = _interruptedMotion; _interruptedMotion.reset(); return m; }

        static constexpr float kKnockbackHold = 0.06f;      // 맞는 순간 버팀 (초). S6에서 조정
        static constexpr float kMotionMaxDeviation = 0.3f;  // 곡선 위치에서 이만큼 벗어나면(막힘) 모션 중단

	private:
        // 모션 중이면 이번 갱신의 목표 위치(곡선)와 그쪽으로 가는 수평 속도를 계산
        bool StepMotion(const common::Vec3& currentPos, float deltaTime, common::Vec3& horizontalVel, common::Vec3& target) const;
        // 갱신 후 실제 위치를 확인해 막혔으면 중단 기록, 시간이 다 됐으면 종료
        void FinishMotionStep(const common::Vec3& actualPos, const common::Vec3& target);

        MotionInfo _motion;
        bool _motionActive = false;
        uint32_t _nextMotionId = 1;
        std::optional<MotionInfo> _startedMotion;
        std::optional<MotionEndInfo> _interruptedMotion;

        common::Vec3 _aiVelocity = { 0,0,0 };
        float _verticalVelocity = 0.0f;
        float _radius = 0.0f;

        // 캐싱된 컴포넌트 포인터 (GetComponent 오버헤드 8% 제거)
        TransformComponent* _cachedTransform = nullptr;
    };
}
