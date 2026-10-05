#pragma once

// 서버 시각 추정 (NetMotionSync_Design_KR.md 5.1)
// - TIME_SYNC 왕복으로 RTT와 "서버 시각 - 클라 시각" 차이(offset)를 잰다
// - 최근 표본 중 RTT가 가장 작은 표본의 offset을 목표로 삼는다 (지연이 튄 표본은 시계 차이 추정이 부정확)
// - offset은 한 번에 뛰지 않고 천천히 따라간다 (큰 차이는 즉시 맞춤)
class ServerClock : public Singleton<ServerClock>
{
	friend class Singleton<ServerClock>;
public:
	// 매 프레임 호출: 로그인 상태에서 동기화 요청을 보낼 때가 되면 보내고, offset을 목표로 옮긴다
	void update();
	// TIME_SYNC 응답. arrival_ms: 패킷이 클라에 도착한 시각 (네트워크 스레드 수신 시각, 인공 지연 포함)
	void on_time_sync(double client_time, double server_time, double arrival_ms);
	// 재접속 등으로 처음부터 다시 잴 때
	void reset();

	bool is_synced() const { return _synced; }
	// 추정 서버 시각 (ms, common::NetNowMs 기준)
	double server_now_ms() const;
	// 지금 클라에 도착해 있을 수 있는 가장 최근 서버 시각 = 추정 서버 시각 - 편도 지연(RTT/2)
	// 보간 렌더 시각의 기준. 이걸 기준으로 해야 보간 지연이 네트워크 지연과 무관하게 전송 간격·흔들림만 담당한다
	double arrival_now_ms() const { return server_now_ms() - _bestRtt * 0.5; }
	double offset_ms() const { return _offset; }
	double best_rtt_ms() const { return _bestRtt; }
	double last_rtt_ms() const { return _lastRtt; }

	// 서버가 32비트로 잘라 보낸 시각(NetNowMs)을 현재 추정 서버 시각 근처의 전체 값으로 되돌림 (약 49일마다 오는 래핑 처리)
	double unwrap_server_time(uint32_t stamp) const;

	// 보간 지연 (NetMotionSync_Design_KR.md 8장)
	static constexpr double kNpcInterpDelayMs = 70.0;	// 서버 NPC 전송 간격 50ms + 여유
	static constexpr double kPlayerInterpDelayMs = 30.0;

	// [디버그] 이번 프레임 보간 상태 집계 (SnapshotBuffer::Mode 값, NPC와 다른 플레이어 따로), 서버 위치(유령) 표시 여부
	enum class InterpTarget { Npc, Player, Count };
	void count_interp_mode(InterpTarget target, int mode)
	{
		if (mode >= 0 && mode < kModeCount) ++_modeCounts[static_cast<int>(target)][mode];
	}
	bool show_server_ghost() const { return _showDebugWindow && _showServerGhost; }

	// 디버그 패널(DebugPanel, \ 키)의 Network 항목: RTT, offset, 보간 집계, 유령, 인공 지연 설정
	// ImGui 창 안에서 호출 (Begin/End는 패널이 함)
	void draw_debug_contents();
	// 패널이 열려 있는지 (닫혀 있으면 서버 위치 유령을 그리지 않음)
	void set_debug_visible(bool visible) { _showDebugWindow = visible; }

private:
	ServerClock() = default;

	struct Sample
	{
		double rtt;
		double offset;
	};

	static constexpr size_t kMaxSamples = 16;
	static constexpr int kBurstCount = 5;			// 접속 직후 빠르게 보낼 횟수
	static constexpr double kBurstIntervalMs = 100.0;
	static constexpr double kIntervalMs = 2000.0;
	static constexpr double kSlewMsPerSec = 5.0;	// offset이 따라가는 최대 속도
	static constexpr double kSnapMs = 50.0;			// 이보다 크게 어긋나면 즉시 맞춤

	std::deque<Sample> _samples;
	double _offset = 0.0;
	double _targetOffset = 0.0;
	double _bestRtt = 0.0;
	double _lastRtt = 0.0;
	bool _synced = false;

	int _burstRemaining = kBurstCount;
	double _nextSendMs = 0.0;
	double _lastUpdateMs = 0.0;

	bool _showDebugWindow = false;
	bool _showServerGhost = false;

	static constexpr int kModeCount = 5;
	static constexpr int kTargetCount = static_cast<int>(InterpTarget::Count);
	int _modeCounts[kTargetCount][kModeCount] = {};			// 이번 프레임 집계 중
	int _shownModeCounts[kTargetCount][kModeCount] = {};	// 직전 프레임 결과 (창에 표시)
};
