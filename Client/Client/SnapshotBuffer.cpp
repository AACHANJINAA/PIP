#include "stdafx.h"
#include "SnapshotBuffer.h"

void SnapshotBuffer::push(const Sample& sample)
{
	if (!_samples.empty() && sample.time <= _samples.back().time) return;

	Sample s = sample;
	if (!_samples.empty())
	{
		const XMVECTOR diff = XMVectorSubtract(XMLoadFloat3(&s.pos), XMLoadFloat3(&_samples.back().pos));
		if (XMVectorGetX(XMVector3LengthSq(diff)) > kTeleportDistance * kTeleportDistance) s.teleport = true;
	}
	_samples.push_back(s);

	// 보간에 필요한 앞 표본 하나는 남기고 오래된 표본 정리
	while (_samples.size() > 2 && _samples[1].time < s.time - kKeepMs) _samples.pop_front();
}

SnapshotBuffer::Result SnapshotBuffer::sample(double render_time) const
{
	Result result;
	if (_samples.empty()) return result;

	const Sample& last = _samples.back();

	// 최신 표본 이후: 속도로 짧게 외삽
	if (render_time >= last.time)
	{
		const double ahead = render_time - last.time;
		const float t = static_cast<float>(std::min(ahead, kMaxExtrapolateMs) / 1000.0);
		XMStoreFloat3(&result.pos, XMVectorAdd(XMLoadFloat3(&last.pos), XMVectorScale(XMLoadFloat3(&last.vel), t)));
		result.vel = ahead <= kMaxExtrapolateMs ? last.vel : XMFLOAT3{ 0, 0, 0 };
		result.rot = last.rot;
		result.mode = ahead <= kMaxExtrapolateMs ? Mode::Extrapolate : Mode::ExtrapolateCapped;
		return result;
	}

	// 가장 오래된 표본보다 이전: 그대로 유지
	if (render_time <= _samples.front().time)
	{
		const Sample& first = _samples.front();
		result.pos = first.pos;
		result.rot = first.rot;
		result.mode = Mode::Hold;
		return result;
	}

	// render_time을 감싸는 두 표본 찾기 (s0.time <= render_time < s1.time)
	size_t i = _samples.size() - 1;
	while (i > 0 && _samples[i - 1].time > render_time) --i;
	const Sample& s0 = _samples[i - 1];
	const Sample& s1 = _samples[i];

	if (s1.teleport)
	{
		result.pos = s0.pos;
		result.rot = s0.rot;
		result.vel = s0.vel;
		result.mode = Mode::Hold;
		return result;
	}

	// 간격이 길면 앞 표본에 머물다가 마지막 kMaxSegmentMs만 보간
	const double segment_start = std::max(s0.time, s1.time - kMaxSegmentMs);
	if (render_time <= segment_start)
	{
		result.pos = s0.pos;
		result.rot = s0.rot;
		result.mode = Mode::Hold;
		return result;
	}

	const double duration_ms = s1.time - segment_start;
	const float u = static_cast<float>((render_time - segment_start) / duration_ms);
	const float duration = static_cast<float>(duration_ms / 1000.0);

	// 3차 에르미트 (속도를 구간 길이로 스케일). 원래 간격이 잘린 경우 앞 표본 속도는 의미가 없으므로 0으로
	const bool trimmed = segment_start > s0.time;
	const XMVECTOR p0 = XMLoadFloat3(&s0.pos);
	const XMVECTOR p1 = XMLoadFloat3(&s1.pos);
	const XMVECTOR m0 = trimmed ? XMVectorZero() : XMVectorScale(XMLoadFloat3(&s0.vel), duration);
	const XMVECTOR m1 = XMVectorScale(XMLoadFloat3(&s1.vel), duration);
	const float u2 = u * u, u3 = u2 * u;
	const float h00 = 2 * u3 - 3 * u2 + 1, h10 = u3 - 2 * u2 + u, h01 = -2 * u3 + 3 * u2, h11 = u3 - u2;
	XMVECTOR pos = XMVectorAdd(XMVectorAdd(XMVectorScale(p0, h00), XMVectorScale(m0, h10)),
	                           XMVectorAdd(XMVectorScale(p1, h01), XMVectorScale(m1, h11)));
	XMStoreFloat3(&result.pos, pos);

	XMStoreFloat3(&result.vel, XMVectorLerp(trimmed ? XMVectorZero() : XMLoadFloat3(&s0.vel), XMLoadFloat3(&s1.vel), u));
	XMStoreFloat4(&result.rot, XMQuaternionSlerp(XMLoadFloat4(&s0.rot), XMLoadFloat4(&s1.rot), u));
	result.mode = Mode::Interpolate;
	return result;
}
