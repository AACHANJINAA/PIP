#include "stdafx.h"
#include "ServerClock.h"
#include "NetworkManager.h"
#include "imgui/imgui.h"

void ServerClock::update()
{
	const double now = common::NetNowMsPrecise();
	const double elapsed = _lastUpdateMs > 0.0 ? now - _lastUpdateMs : 0.0;
	_lastUpdateMs = now;

	// 직전 프레임 보간 상태 집계를 창 표시용으로 넘기고 새로 셈
	std::copy(std::begin(_modeCounts), std::end(_modeCounts), std::begin(_shownModeCounts));
	std::fill(std::begin(_modeCounts), std::end(_modeCounts), 0);

	// offset을 목표로 천천히 옮김
	const double diff = _targetOffset - _offset;
	if (std::abs(diff) > kSnapMs) _offset = _targetOffset;
	else
	{
		const double max_step = kSlewMsPerSec * elapsed / 1000.0;
		_offset += std::clamp(diff, -max_step, max_step);
	}

	// 동기화 요청 (로그인 후에만)
	auto network = NetworkManager::instance();
	if (!network->is_login() || now < _nextSendMs) return;

	network->SendTimeSyncPacket(now);
	if (_burstRemaining > 0)
	{
		--_burstRemaining;
		_nextSendMs = now + kBurstIntervalMs;
	}
	else
	{
		_nextSendMs = now + kIntervalMs;
	}
}

void ServerClock::on_time_sync(double client_time, double server_time, double arrival_ms)
{
	const double rtt = std::max(0.0, arrival_ms - client_time);
	// 서버가 요청을 받은 순간 = 왕복의 중간이라고 가정
	const double offset = server_time + rtt * 0.5 - arrival_ms;

	_samples.push_back({ rtt, offset });
	if (_samples.size() > kMaxSamples) _samples.pop_front();
	_lastRtt = rtt;

	// RTT가 가장 작은 표본을 기준으로 삼음
	const Sample* best = &_samples.front();
	for (const auto& sample : _samples)
		if (sample.rtt < best->rtt) best = &sample;
	_bestRtt = best->rtt;
	_targetOffset = best->offset;

	if (!_synced)
	{
		_offset = _targetOffset;
		_synced = true;
	}
}

void ServerClock::reset()
{
	_samples.clear();
	_offset = _targetOffset = 0.0;
	_bestRtt = _lastRtt = 0.0;
	_synced = false;
	_burstRemaining = kBurstCount;
	_nextSendMs = 0.0;
}

double ServerClock::server_now_ms() const
{
	return common::NetNowMsPrecise() + _offset;
}

double ServerClock::unwrap_server_time(uint32_t stamp) const
{
	const double now = server_now_ms();
	const uint32_t now32 = static_cast<uint32_t>(static_cast<uint64_t>(now));
	const int32_t diff = static_cast<int32_t>(now32 - stamp); // 양수면 stamp가 과거
	return static_cast<double>(static_cast<uint64_t>(now)) - diff;
}

void ServerClock::draw_debug_window()
{
	if (!_showDebugWindow) return;

	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	ImGui::Begin("Network (F6)", &_showDebugWindow, ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("Synced: %s   Samples: %d", _synced ? "yes" : "no", static_cast<int>(_samples.size()));
	ImGui::Text("RTT  best %.1f ms / last %.1f ms", _bestRtt, _lastRtt);
	ImGui::Text("Offset %.1f ms (target %.1f)", _offset, _targetOffset);
	ImGui::Text("Server now %.0f ms", server_now_ms());

	// 보간 상태 (NPC·다른 플레이어 수): 외삽이 많으면 보간 지연이 전송 간격보다 짧거나 패킷이 늦는 것
	ImGui::Separator();
	ImGui::Text("Interp %d / Hold %d / Extrap %d / Capped %d",
		_shownModeCounts[1], _shownModeCounts[2], _shownModeCounts[3], _shownModeCounts[4]);
	ImGui::Text("Delay NPC %.0f ms / Player %.0f ms", kNpcInterpDelayMs, kPlayerInterpDelayMs);
	ImGui::Checkbox("Show NPC server position (ghost)", &_showServerGhost);

	// 인공 수신 지연·흔들림 (같은 PC에서 나쁜 네트워크 재현용, 수신 쪽만 늦춤)
	ImGui::Separator();
	auto network = NetworkManager::instance();
	float latency = network->sim_latency_ms();
	float jitter = network->sim_jitter_ms();
	if (ImGui::SliderFloat("Recv delay (ms)", &latency, 0.0f, 300.0f, "%.0f")) network->set_sim_latency_ms(latency);
	if (ImGui::SliderFloat("Recv jitter (ms)", &jitter, 0.0f, 100.0f, "%.0f")) network->set_sim_jitter_ms(jitter);

	ImGui::End();
}
