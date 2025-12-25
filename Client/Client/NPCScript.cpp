#include "stdafx.h"
#include "NPCScript.h"

#include "GameObject.h"

void NPCScript::set_position(const f3& position)
{
	game_object().get()->get_component<TransformComponent>()->set_local_position(position);
}

const f3& NPCScript::position() const
{
	return game_object().get()->get_component<TransformComponent>()->local_position();
}


void NPCScript::awake()
{
	transform()->set_local_rotation(-90.f, 0.f, 0.f);
	transform()->set_local_scale({ 200.0f, 200.0f, 200.0f });

	// [추가] 보간 상태 초기화
	_currentTargetState.position = position();
	_currentTargetState.velocity = { 0,0,0 };
	_currentTargetState.rotation = { 0,0,0,1 };
	_currentTargetState.timestamp = static_cast<uint32_t>(GetTickCount64()); // 클라이언트 시간으로 초기화

	_prevTargetState = _currentTargetState;
	_interpolateTimer = 0.0f;
	_hasReceivedFirstPacket = false;
}

void NPCScript::update(float deltaTime)
{
	ScriptComponent::update(deltaTime);

	if (!_hasReceivedFirstPacket) {
		// 첫 패킷 수신 전까지는 아무것도 하지 않음 (또는 초기 위치 고정)
		return;
	}

	// 보간 진행 시간 누적
	_interpolateTimer += deltaTime;

	// 보간 비율 (0.0 ~ 1.0 이상)
	float t = _interpolateTimer / _interpolateDuration;

	f3 finalPos;
	common::Quat finalRot_server_only;

	// 보간 (Interpolation)
	if (t <= 1.0f) {
		// 위치 Lerp
		XMVECTOR prevPos_xm = XMLoadFloat3(&_prevTargetState.position);
		XMVECTOR targetPos_xm = XMLoadFloat3(&_currentTargetState.position);
		XMVECTOR interpPos_xm = XMVectorLerp(prevPos_xm, targetPos_xm, t);
		XMStoreFloat3(&finalPos, interpPos_xm);

		// 회전 Slerp (구형 선형 보간)
		XMVECTOR prevRot_xm = XMLoadFloat4(&_prevTargetState.rotation);
		XMVECTOR targetRot_xm = XMLoadFloat4(&_currentTargetState.rotation);
		XMVECTOR interpRot_xm = XMQuaternionSlerp(prevRot_xm, targetRot_xm, t);
		XMStoreFloat4(&finalRot_server_only, interpRot_xm);
	}
	// 외삽 (Extrapolation) - 서버 패킷 주기보다 오래 걸리면 속도를 이용해 예측
	else {
		// 마지막 목표 위치에서 속도 벡터 * 초과 시간으로 외삽
		float extrapolation_dt = _interpolateTimer - _interpolateDuration;
		finalPos.x = _currentTargetState.position.x + _currentTargetState.velocity.x * extrapolation_dt;
		finalPos.y = _currentTargetState.position.y + _currentTargetState.velocity.y * extrapolation_dt;
		finalPos.z = _currentTargetState.position.z + _currentTargetState.velocity.z * extrapolation_dt;

		// 회전은 외삽하지 않고 마지막 목표 회전 사용 (단순화)
		finalRot_server_only = _currentTargetState.rotation;
	}

	// 최종 위치와 회전 적용
	transform()->set_local_position(finalPos);

	common::Quat finalRot_combined = TransformComponent::apply_offset_rotation(finalRot_server_only,-90.f,0,0);
	transform()->set_local_rotation(finalRot_combined);
}

void NPCScript::late_update(float deltaTime)
{
	// hp 상태 셰이더로 전송

}

void NPCScript::on_server_update(const f3& pos, const f3& vel, const common::Quat& rot, uint32_t timestamp)
{
	// 첫 패킷 수신 시 현재 클라이언트 위치를 이전 상태로 설정
	if (!_hasReceivedFirstPacket) {
		_prevTargetState.position = position();
		_prevTargetState.velocity = { 0,0,0 }; // 첫 패킷 전에는 속도 0
		_prevTargetState.rotation = { 0,0,0,1 }; // 첫 패킷 전에는 회전 없음
		_prevTargetState.timestamp = static_cast<uint32_t>(GetTickCount64()); // 현재 클라이언트 시간
		_hasReceivedFirstPacket = true;
	}
	else {
		_prevTargetState = _currentTargetState; // 이전 목표 상태를 과거 상태로
	}

	_currentTargetState.position = pos;
	_currentTargetState.velocity = vel;
	_currentTargetState.rotation = rot;
	_currentTargetState.timestamp = timestamp;
	_currentTargetState.client_receive_time = static_cast<uint32_t>(GetTickCount64()); // 클라이언트가 패킷을 받은 시간

	_interpolateTimer = 0.0f; // 보간 타이머 리셋
}