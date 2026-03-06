#pragma once

namespace PIP::GAME
{
    // NPC 및 보스 공격 설정을 위한 구조체
    struct NPCAttackConfig {
        JPH::Ref<JPH::Shape> shape;    // 공격 판정 모양 (Sphere, Box, Capsule 등)
        common::Vec3 posOffset;        // NPC 중심으로부터의 오프셋
        float damage;                  // 공격력
        float cooldown;                // 재사용 대기시간
        std::string animationKey;      // (선택) 클라이언트에 보낼 애니메이션 이름/번호
        // 추가 가능: 상태 이상, 넉백 수치 등
    };

    // 테이너(보스) 페이즈 정의
    enum class TainerPhase : uint8_t
    {
        PHASE_1, // 거인의 압박 (내려찍기, 돌진)
        PHASE_2  // 날카로운 뼈 (클로 난타, 잡기)
    };
}