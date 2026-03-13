#pragma once

namespace PIP::GAME
{
    // NPC 및 보스 공격 설정을 위한 구조체
    struct NPCAttackConfig {
        JPH::Ref<JPH::Shape>            shape;                        // 공격 판정 모양 (Sphere, Box, Capsule 등)
        common::Vec3                    posOffset;                    // NPC 중심으로부터의 오프셋
		common::packet::OBJECT_STATE    animationState;               // 공격 애니메이션 상태값 (예: ATTACK1, SKILL1 등)
        float                           damage;                       // 공격력
        float                           cooldown;                     // 재사용 대기시간
		float                           animationDuration   { 1.0f }; // 애니메이션 지속 시간 (초 단위, 기본 1초)
		float                           attackTiming        { 1.0f }; // 공격 판정 발생 시점 (초 단위, 애니메이션 시작부터의 시간)

        // --- 추가된 필드 ---
        bool                            isContinuous{ false }; // true면 동작 내내 판정 (돌진 등)
        float                           hitInterval{ 0.1f };  // 지속 공격 시 판정 주기
        float                           knockbackValue{ 0.0f };
    };

    // 테이너(보스) 페이즈 정의
    enum class TainerPhase : uint8_t
    {
        PHASE_1, // 거인의 압박 (내려찍기, 돌진)
        PHASE_2  // 날카로운 뼈 (클로 난타, 잡기)
    };
}