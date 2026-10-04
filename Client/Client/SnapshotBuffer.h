#pragma once

// 서버 시각이 찍힌 위치·회전 표본을 쌓아 두고, 렌더 시각(서버 시각 - 보간 지연)의 값을 보간한다
// (NetMotionSync_Design_KR.md 5.2). NPC와 다른 플레이어가 같이 쓴다.
// - 두 표본 사이: 위치는 속도를 쓴 3차 에르미트, 회전은 slerp
// - 표본 간격이 길면(바뀔 때만 전송되는 NPC가 멈춰 있다 움직인 경우) 앞 표본에 머물다가 마지막 구간만 보간
// - 최신 표본 이후: 최대 kMaxExtrapolateMs까지만 속도로 외삽하고 멈춤
// - 표본 사이 거리가 크면 순간이동으로 보고 보간하지 않음
class SnapshotBuffer
{
public:
	struct Sample
	{
		double time = 0.0;				// 서버 시각 (ms)
		XMFLOAT3 pos = { 0, 0, 0 };
		XMFLOAT3 vel = { 0, 0, 0 };
		XMFLOAT4 rot = { 0, 0, 0, 1 };
		bool teleport = false;			// 앞 표본에서 보간하지 않고 바로 이 표본으로
	};

	enum class Mode { Empty, Interpolate, Hold, Extrapolate, ExtrapolateCapped };

	struct Result
	{
		XMFLOAT3 pos = { 0, 0, 0 };
		XMFLOAT3 vel = { 0, 0, 0 };
		XMFLOAT4 rot = { 0, 0, 0, 1 };
		Mode mode = Mode::Empty;
	};

	// 시각 순서대로 넣는다. 이전 표본보다 이른 시각은 버린다
	void push(const Sample& sample);
	void clear() { _samples.clear(); }
	bool empty() const { return _samples.empty(); }
	const Sample& latest() const { return _samples.back(); }

	Result sample(double render_time) const;

	static constexpr double kMaxExtrapolateMs = 100.0;
	static constexpr double kMaxSegmentMs = 100.0;	// 이보다 긴 간격은 마지막 구간만 보간
	static constexpr float kTeleportDistance = 5.0f;
	static constexpr double kKeepMs = 1000.0;		// 최신 기준 이만큼 지난 표본은 버림

private:
	std::deque<Sample> _samples;
};
