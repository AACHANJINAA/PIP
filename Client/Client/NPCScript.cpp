#include "stdafx.h"
#include "NPCScript.h"

#include "AnimationComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "MonsterHPComponent.h"

void NPCScript::set_position(const XMFLOAT3& position)
{
	if (transform()) {
		transform()->set_local_position(position);
	}
}

void NPCScript::set_state(const common::packet::OBJECT_STATE& object_state)
{
	_state = object_state;
	// 애니메이션 컴포넌트에게 상태 변경 알림
	auto animation_component = game_object()->get_component<AnimationComponent>();
	if (animation_component) {
		animation_component->set_state(object_state);
	}
}

const XMFLOAT3& NPCScript::position() const
{
	static XMFLOAT3 dummy = { 0, 0, 0 };
	return transform() ? transform()->local_position() : dummy;
}

void NPCScript::awake()
{
	_serverPos = transform()->local_position();
	_serverRot = transform()->local_rotation();
	_serverVel = { 0, 0, 0 };
	_isFirstUpdate = true;
}

void NPCScript::on_server_update(const XMFLOAT3& pos, const XMFLOAT3& vel, const XMFLOAT4& rot, uint32_t timestamp)
{
	_serverPos = pos;
	_serverVel = vel;
	_accumulatedTime = 0.0f; // 패킷 수신 후 시간 리셋

	// --- 1. 서버에서 받은 회전값(rot)에 Y축 180도 추가 회전 적용 ---
	XMVECTOR qServer = XMLoadFloat4((XMFLOAT4*)&rot);
	XMVECTOR qRotate180 = XMQuaternionRotationRollPitchYaw(0, XM_PI, 0); // Y축 180도(PI) 회전
	XMVECTOR qFinal = XMQuaternionMultiply(qServer, qRotate180);         // 회전 결합

	// 보정된 회전값을 _serverRot에 저장
	XMStoreFloat4(&_serverRot, qFinal);

	// 첫 패킷 수신 시 즉시 동기화
	if (_isFirstUpdate) {
		if (transform()) {
			transform()->set_local_position(pos);
			transform()->set_local_rotation(_serverRot);
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

	// --- 1. 서버에서 받은 회전값(rot)에 Y축 180도 추가 회전 적용 ---
	XMVECTOR qServer = XMLoadFloat4((XMFLOAT4*)&_serverRot);
	XMVECTOR qRotate180 = XMQuaternionRotationRollPitchYaw(0, XM_PI, 0); // Y축 180도(PI) 회전
	XMVECTOR qFinal = XMQuaternionMultiply(qServer, qRotate180);         // 회전 결합

	// 보정된 회전값을 _serverRot에 저장
	XMStoreFloat4(&_serverRot, qFinal);

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

	//// --- 1. 서버에서 받은 회전값(rot)에 Y축 180도 추가 회전 적용 ---
	//XMVECTOR qServer = XMLoadFloat4((XMFLOAT4*)&_serverRot);
	//XMVECTOR qRotate180 = XMQuaternionRotationRollPitchYaw(0, XM_PI, 0); // Y축 180도(PI) 회전
	//XMVECTOR qFinal = XMQuaternionMultiply(qServer, qRotate180);         // 회전 결합

	//// 보정된 회전값을 _serverRot에 저장
	//XMStoreFloat4(&_serverRot, qFinal);

	transform()->set_local_rotation(_serverRot);
}

void NPCScript::late_update(float deltaTime)
{
	// HP 바 업데이트 등 필요한 로직 수행
}