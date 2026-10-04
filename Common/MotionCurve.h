#pragma once
#include <cstdint>
#include <algorithm>

// 모션 이벤트 (NetMotionSync_Design_KR.md 5.3)
// 넉백처럼 정해진 궤적으로 움직이는 동작은 위치 표본 대신 정의(시작·끝 위치, 시간, 곡선)를 보내고,
// 서버와 클라가 같은 곡선으로 위치를 계산한다. 이 파일의 진행률 함수를 서버·클라가 같이 쓴다.
namespace common::motion
{
	enum class MotionType : uint8_t
	{
		Knockback = 0,
	};

	enum class MotionCurve : uint8_t
	{
		EaseOutQuad = 0,	// 일정한 감속 (초기 속도 v, 감속 a로 밀리다 멈추는 것과 같은 모양)
	};

	// 시작 후 경과 시간(초) → 시작 위치에서 끝 위치까지의 진행률 0~1
	// hold 동안은 0 (맞는 순간 버팀), 그 뒤 duration 동안 곡선대로 1까지
	inline float Progress(MotionCurve curve, float hold, float duration, float elapsed)
	{
		if (elapsed <= hold) return 0.0f;
		if (duration <= 0.0f) return 1.0f;
		const float u = std::clamp((elapsed - hold) / duration, 0.0f, 1.0f);
		switch (curve)
		{
		case MotionCurve::EaseOutQuad:
		default:
			return 1.0f - (1.0f - u) * (1.0f - u);
		}
	}
}
