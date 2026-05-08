#pragma once

namespace PIP::GAME
{
	// NPC 및 보스 공격 설정을 위한 구조체
	struct AttackConfig {
		JPH::Ref<JPH::Shape>            shape;                        // 공격 형태 (Sphere, Box, Capsule 등)
		common::Vec3                    posOffset;                    // NPC 중심으로부터의 오프셋
		common::packet::EntityState     entityState;                  // 공격 상태 (예: ACTION)
		int32_t                         actionId{ 0 };                // 공격 액션 ID (0이면 없음, 보스 스킬 번호 등)
		float                           damage;                       // 공격력
		float                           cooldown;                     // 재사용 대기시간
		float                           animationDuration   { 1.0f }; // 애니메이션 지속 시간 (초, 기본 1초)
		float                           attackTiming        { 1.0f }; // 공격 판정 발생 타이밍 (초, 애니메이션 시작부터의 시간)

		// --- 추가 옵션 ---
		bool                            isContinuous{ false }; // true면 지속형 공격 (돌진 등)
		float                           hitInterval{ 0.1f };  // 지속 공격 시 판정 주기
		float                           knockbackValue{ 0.0f };
		bool                            isGrab{ false };      // [추가] 잡기 판정 여부
	};

	// Tainer (보스) 페이즈 상태
	enum class TainerPhase {
		PHASE_1, // 기본 (내려찍기, 돌진)
		PHASE_2  // 공격성 강화 (클로 연타, 잡기)
	};


}
