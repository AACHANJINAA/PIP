#pragma once
#include <chrono>
#include <cstdint>

namespace common
{
	// 서버·클라이언트 공용 네트워크 시계 (ms)
	// steady_clock(고해상도 단조 시계) 기준이라 PC마다 기준점이 다르다. 서버 시각 추정은 클라이언트 ServerClock이 맡는다.
	// GetTickCount64는 해상도가 약 16ms라 보간 기준으로 거칠어서 쓰지 않는다.
	inline uint64_t NetNowMs()
	{
		using namespace std::chrono;
		return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
	}

	// 소수점 이하까지 필요한 경우 (RTT 측정 등)
	inline double NetNowMsPrecise()
	{
		using namespace std::chrono;
		return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
	}
}
