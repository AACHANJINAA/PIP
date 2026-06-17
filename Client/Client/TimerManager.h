#pragma once
const ULONG MAX_SAMPLE_COUNT = 60; // 60회의 프레임 처리시간을 누적하여 평균한다.


class TimerManager : public Singleton<TimerManager>
{
	friend class Singleton<TimerManager>;
public:
	TimerManager();
	virtual ~TimerManager();


	void Start() {}
	void Stop() {}
	void Reset();
	void Tick(float fLockFPS = 0.0f); // 타이머의 시간을 갱신함
	unsigned long GetFrameRate(LPTSTR lpszString = NULL, int nCharacters = 0); // 프레임 레이트를 반환한다.
	float GetTimeElapsed(); // 프레임의 평균 경과 시간을 반환한다.

	// [추가] 역경직(Hit Stop) 및 게임 시간 배속 제어
	void SetHitStop(float duration, float timeScale);
	float GetGameTimeScale() const { return _gameTimeScale; }

private:
	bool _hasHardwareHasPerformanceCounter;	// 컴퓨터가 Performance Counter를 갖고 있는지?
	float _timeScale;						// Scale Counter의 양
	
	float _gameTimeScale = 1.0f;			// 게임 내 전역 시간 배속 (기본 1.0)
	float _hitStopTimer = 0.0f;				// 역경직 지속 시간 타이머

	float m_fTimeElapsed;					// 마지막 프레임 이후 지나간 시간
	__int64 m_nCurrentTime;					// 현재의 시간
	__int64 m_nLastTime;					// 마지막 프레임의 시간
	__int64 m_nPerformanceFrequency;		// 컴퓨터의 performance Frequency

	float m_fFrameTime[MAX_SAMPLE_COUNT];	// 프레임 시간을 누적하기 위한 배열
	ULONG m_nSampleCount;					// 누적된 프레임 횟수

	unsigned long m_nCurrentFrameRate;		// 현재의 프레임 레이트
	unsigned long m_nFramesPerSecond;		// 초당 프레임 수
	float m_fFPSTimeElapsed;				// 프레임 레이트 계산 소요 시간

	bool m_bStopped;

};

