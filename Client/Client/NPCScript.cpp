#include "stdafx.h"
#include "NPCScript.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "MonsterHPComponent.h"

void NPCScript::set_position(const XMFLOAT3& position)
{
	if (transform()) {
		transform()->set_local_position(position);
	}
}

const XMFLOAT3& NPCScript::position() const
{
	static XMFLOAT3 dummy = { 0, 0, 0 };
	return transform() ? transform()->local_position() : dummy;
}

void NPCScript::awake()
{
	// 모델 기본 설정
	transform()->set_local_rotation(-90.f, 0.f, 0.f);
	transform()->set_local_scale({ 200.0f, 200.0f, 200.0f });

	_serverPos = transform()->local_position();
	_serverRot = transform()->local_rotation();
	_serverVel = { 0, 0, 0 };
	_isFirstUpdate = true;
}

void NPCScript::on_server_update(const XMFLOAT3& pos, const XMFLOAT3& vel, const XMFLOAT4& rot, uint32_t timestamp)
{
	_serverPos = pos;
	_serverVel = vel;
	_serverRot = rot;
	_accumulatedTime = 0.0f; // 패킷 수신 후 시간 리셋

	// 첫 패킷 수신 시 즉시 동기화
	if (_isFirstUpdate) {
		if (transform()) {
			transform()->set_local_position(pos);
			// 서버 회전에 -90도 오프셋 적용
			transform()->set_local_rotation(TransformComponent::apply_offset_rotation(rot, -90.f, 0, 0));
		}
		_isFirstUpdate = false;
	}
}

void NPCScript::initialize_from_server(const XMFLOAT3& pos)
{
	_serverPos = pos;
	_serverVel = { 0, 0, 0 };
	_serverRot = { 0, 0, 0, 1 };
	_accumulatedTime = 0.0f;

	if (transform()) {
		transform()->set_local_position(pos);
	}

	_isFirstUpdate = false; // 이제 업데이트 가능 상태로 전환
}

void NPCScript::set_hp(int hp)
{
	_hp = hp;
	auto hp_component = game_object()->get_component<MonsterHPComponent>();
	if (hp_component) {
		hp_component.get()->set_current_hp(hp);
	}
}

void NPCScript::update(float deltaTime)
{
	if (_isFirstUpdate || !transform()) return;

	_accumulatedTime += deltaTime;

	// [최적화] 패킷이 0.5초 이상 안 오면 예측 이동(Dead Reckoning) 중지 (가출 방지)
	XMVECTOR vServerVel = XMLoadFloat3(&_serverVel);
	if (_accumulatedTime > 0.3f) {
		vServerVel = XMVectorZero();
	}

	// ---------------------------------------------------------
	// 1. 추측 항법 (Dead Reckoning)
	// ---------------------------------------------------------
	XMVECTOR vServerPos = XMLoadFloat3(&_serverPos);
	XMVECTOR vPredictedPos = vServerPos + (vServerVel * _accumulatedTime);

	// ---------------------------------------------------------
	// 2. 위치 보간 (Lerp)
	// ---------------------------------------------------------
	XMVECTOR vCurrentPos = XMLoadFloat3(&transform()->local_position());
	float distSq = XMVectorGetX(XMVector3LengthSq(vPredictedPos - vCurrentPos));
	
	float lerpSpeed = 10.0f;
	if (distSq > 10.0f * 10.0f) { // 10m 이상 오차 시 텔레포트
		lerpSpeed = 1000.0f;
	} else if (distSq > 0.5f * 0.5f) {
		lerpSpeed = 15.0f;
	}

	XMVECTOR vNextPos = XMVectorLerp(vCurrentPos, vPredictedPos, deltaTime * lerpSpeed);
	XMFLOAT3 nextPos;
	XMStoreFloat3(&nextPos, vNextPos);
	transform()->set_local_position(nextPos);

	// ---------------------------------------------------------
	// 3. 회전 보간 (Slerp)
	// ---------------------------------------------------------
	XMVECTOR qCurrentRot = XMLoadFloat4(&transform()->local_rotation());
	// 서버 회전에 모델 오프셋(-90도) 적용한 것을 목표로 설정
	auto offset_rotation = TransformComponent::apply_offset_rotation(_serverRot, -90.f, 0, 0);
	XMVECTOR qTargetRot = XMLoadFloat4(&offset_rotation);

	XMVECTOR qNextRot = XMQuaternionSlerp(qCurrentRot, qTargetRot, deltaTime * 8.0f);
	XMFLOAT4 nextRot;
	XMStoreFloat4(&nextRot, qNextRot);
	transform()->set_local_rotation(nextRot);
}

void NPCScript::late_update(float deltaTime)
{
	// HP 바 업데이트 등 필요한 로직 수행
}