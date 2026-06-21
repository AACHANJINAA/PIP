#pragma once
#include "CombatDef.h"
#include "NPC.h"
namespace PIP::GAME
{

    class Tainer : public NPC
    {
    public:
        Tainer(int64_t npc_id, int room_id, common::Vec3 position, int32_t maxHp = 500);
        virtual ~Tainer() override = default;

        void SetupBT() override;
        // 페이즈 관리
        TainerPhase GetCurrentPhase() const { return _currentPhase; }
        void CheckPhaseTransition();

        // 피격 판정 오버라이드 (보스 전용 리액션 등)
        bool ValidateHit(JPH::PhysicsSystem* physics,
            const JPH::Shape* attackShape,
            const JPH::RMat44& attackTransform,
            uint32_t timestamp,
            GameObject* attacker,
            int32_t damage) override;
        void SetPhase(const TainerPhase& tainer_phase);
        void Update(float deltaTime, JPH::TempAllocator* allocator) override;

    private:
        TainerPhase _currentPhase = TainerPhase::PHASE_1;

        // 공격 설정 (기획서 기반)
        AttackConfig _slamAtk;    // 내려찍기
        AttackConfig _chargeAtk;  // 돌진
        AttackConfig _clawAtk;    // 클로 난타 (Phase 2)
        AttackConfig _grabAtk;    // 잡기 (Phase 2)
        AttackConfig _grabChargeAtk; // [추가] 잡기 돌진 (Phase 2)
    };
}
