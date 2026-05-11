#include "stdafx.h"
#include "NPCScript.h"
#include "ReplicationSystem.h"
#include "AnimationComponent.h"
#include "GameFramework.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "MonsterHPComponent.h"
#include "ObjectManager.h"
#include "ResourceManager.h"

void NPCScript::set_position(const XMFLOAT3& position)
{
	if (transform()) {
		transform()->set_local_position(position);
	}
}

void NPCScript::handle_animation_branching()
{
	auto anim = game_object()->get_component<AnimationComponent>();
	if (!anim) return;

	using namespace common::packet;

	// 1. 사망/피격 최우선 처리
	/*if (_state == EntityState::DEAD) {
		anim->play("Die", false);
		return;
	}
	if (_state == EntityState::HITTED) {
		anim->play("Hit", false);
		return;
	}*/

	// 1. 사망 처리
	if (_state == EntityState::DEAD) {
		anim->play("Death", false);
		return;
	}

	// 2. 액션(공격/스킬) 상태 분기
	if (_state == EntityState::ACTION) 
	{
		// [고도화] 보스 액션 ID에 따른 세부 애니메이션 분기
		if (_npcType == NPCType::Tainer) {
			using namespace common::packet;
			switch (_actionId) {
			case ActionID::Tainer::GrabCharge: anim->play("walk", true, 2.0f); break; // 돌진 (빠른 이동)
			case ActionID::Tainer::GrabCarry:  anim->play("attack", true, 1.5f); break; // 난타 (연타 모션)
			case ActionID::Tainer::GrabSlam:   anim->play("attack", false, 0.8f); break; // 슬램 (강한 공격)
			case ActionID::Tainer::Roar:       anim->play("Idle", false); break;
			default: anim->play("Attack", false); break;
			}
		}
		else {
			anim->play("Attack", false);
		}
		return;
	}

	// 3. 이동 상태 분기 (속도에 따라 Walk/Run 결정)
	if (_state == EntityState::MOVE) {
		float speed = common::Length(_serverVel);
		/*if (speed > 5.0f) anim->play("Run");
		else anim->play("Walk");*/
		anim->play("Walk");
	}
	else {
		// 4. 대기 상태
		anim->play("Idle");
	}
}

const XMFLOAT3& NPCScript::position() const
{
	static XMFLOAT3 dummy = { 0, 0, 0 };
	return transform() ? transform()->local_position() : dummy;
}

NPCScript::NPCScript() : ScriptComponent("NPCScript")
{

}


NPCScript::~NPCScript()
{
	
}

void NPCScript::init_visual()
{
	auto NPC = game_object();
	auto animation_component = NPC->get_component<AnimationComponent>();
	auto render_comp = NPC->get_component<RenderComponent>();

	if (_npcType == common::packet::NPCType::Elevator) {
		// 엘리베이터 모델 설정 (실제 경로 적용)
		auto baseMesh = ResourceManager::instance()->load_mesh("Resource/Elevator/Elevator.gltf");
		render_comp->set_mesh(baseMesh);

		std::string material_name = "elevator_material_" + std::to_string(id());
		ResourceManager::instance()->create_material(material_name);
		ResourceManager::instance()->set_shader_for_material(material_name, "default");
		render_comp->set_pso_name("default");
		return;
	}

	// 기본 Brute 모델 설정
	auto baseMesh = ResourceManager::instance()->load_mesh("Resource/Character/DragonBrute/SK_DragonBrute.gltf", true);

	dynamic_pointer_cast<ReadGLTFMesh>(baseMesh)->load_animation_only("Resource/Character/DragonBrute/animation/A_DragonBrute_Idle.gltf", "idle");
	dynamic_pointer_cast<ReadGLTFMesh>(baseMesh)->load_animation_only("Resource/Character/DragonBrute/animation/A_DragonBrute_Walk.gltf", "walk");
	dynamic_pointer_cast<ReadGLTFMesh>(baseMesh)->load_animation_only("Resource/Character/DragonBrute/animation/A_DragonBrute_Attack.gltf", "attack");
	dynamic_pointer_cast<ReadGLTFMesh>(baseMesh)->load_animation_only("Resource/Character/DragonBrute/animation/A_DragonBrute_Death.gltf", "die");

	render_comp->set_mesh(baseMesh);
	animation_component->add_animation("Idle", baseMesh, "idle");
	animation_component->add_animation("Walk", baseMesh, "walk");
	animation_component->add_animation("Attack", baseMesh, "attack");
	animation_component->add_animation("Death", baseMesh, "die");

	std::string material_name = "npc_material_" + std::to_string(id());
	ResourceManager::instance()->create_material(material_name);
	ResourceManager::instance()->set_shader_for_material(material_name, "skinned");
	render_comp->set_pso_name("skinned");
}

void NPCScript::awake()
{
	
	init_visual();

	_serverPos = transform()->local_position();
	_serverRot = transform()->local_rotation();
	_serverVel = { 0, 0, 0 };
	_isFirstUpdate = true;

	// [수정] awake에서 등록하지 않고 set_id 호출 시점에 명시적으로 등록하도록 변경
}

void NPCScript::on_destroy()
{
	auto rs = GameFramework::instance()->get_replication_system();
	if (rs) rs->unregister_entity(this->id());
}

void NPCScript::on_server_update(const common::packet::SC_PACKET_NPC_MOVE& npc_move_packet)
{
	_serverPos = npc_move_packet._position;
	_serverVel = npc_move_packet._velocity;
	_accumulatedTime = 0.0f; // 패킷 수신 후 시간 리셋
	_state = npc_move_packet._state;
	_actionId = npc_move_packet._action_id;
	_hp = npc_move_packet._hp; // [추가] HP 동기화
	_grabbedById = -1; // 단일 이동 패킷엔 아직 그랩 정보가 없음 (일관성을 위해 리셋)
	_grabSlot = -1;

	// --- 1. 서버에서 받은 회전값(rot)에 Y축 180도 추가 회전 적용 ---
	XMVECTOR qServer = XMLoadFloat4((XMFLOAT4*)&npc_move_packet._rotation);
	XMVECTOR qRotate180 = XMQuaternionRotationRollPitchYaw(0, XM_PI, 0); // Y축 180도(PI) 회전
	XMVECTOR qFinal = XMQuaternionMultiply(qServer, qRotate180);         // 회전 결합

	// 보정된 회전값을 _serverRot에 저장
	XMStoreFloat4(&_serverRot, qFinal);

	// 첫 패킷 수신 시 즉시 동기화
	if (_isFirstUpdate) {
		if (transform()) {
			transform()->set_local_position(_serverPos);
			transform()->set_local_rotation(_serverRot);
		}
		_isFirstUpdate = false;
	}
}

void NPCScript::initialize_from_server(const common::packet::SC_PACKET_NPC_SPAWN& spawnPkt)
{
	_serverPos = spawnPkt._position;
	_serverVel = { 0, 0, 0 };
	_serverRot = { 0, 0, 0, 1 };
	_accumulatedTime = 0.0f;
	_isNewDataArrived = false; // 대기 중인 스냅샷 무시 (생성 시 좌표가 우선)
	_state = spawnPkt._state;
	_actionId = spawnPkt._action_id;
	_hp = spawnPkt._hp;
	set_id(spawnPkt._npc_id);
	_npcType = spawnPkt._npc_type;

	// --- 1. 서버에서 받은 회전값(rot)에 Y축 180도 추가 회전 적용 ---
	XMVECTOR qServer = XMLoadFloat4((XMFLOAT4*)&_serverRot);
	XMVECTOR qRotate180 = XMQuaternionRotationRollPitchYaw(0, XM_PI, 0); // Y축 180도(PI) 회전
	XMVECTOR qFinal = XMQuaternionMultiply(qServer, qRotate180);         // 회전 결합

	// 보정된 회전값을 _serverRot에 저장
	XMStoreFloat4(&_serverRot, qFinal);

	if (transform()) {
		transform()->set_local_position(spawnPkt._position);
		// [추가] 초기 회전값도 안전하게 설정
		XMVECTOR qRotate180 = XMQuaternionRotationRollPitchYaw(0, XM_PI, 0);
		XMStoreFloat4(&_serverRot, qRotate180);
		transform()->set_local_rotation(_serverRot);
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
	// 0. 잡기 상태일 때 본 부착 처리 (NPC)
	if (_grabbedById != -1) {
		auto ownerObj = ObjectManager::instance()->find_npc(_grabbedById);
		if (ownerObj) {
			auto bossAnim = ownerObj->get_component<AnimationComponent>();
			auto bossRender = ownerObj->get_component<RenderComponent>();
			if (bossAnim && bossRender) {
				auto bossMesh = std::dynamic_pointer_cast<ReadGLTFMesh>(bossRender->mesh());
				if (bossMesh) {
					// [수정] 대소문자 구분: hand_L, hand_R
					std::string boneName = (_grabSlot == 0) ? "hand_L" : "hand_R";
					XMFLOAT4X4 boneSocketTransform = bossMesh->get_socket_transform(boneName);
					XMFLOAT4X4 bossWorldMatrix = ownerObj->transform()->world_matrix();

					XMMATRIX matBone = XMLoadFloat4x4(&boneSocketTransform);
					XMMATRIX matBoss = XMLoadFloat4x4(&bossWorldMatrix);
					XMMATRIX matFinal = matBone * matBoss;

					// [핵심] 보스의 스케일 제거 및 위치/회전만 추출
					XMVECTOR scale, rot, pos;
					XMMatrixDecompose(&scale, &rot, &pos, matFinal);

					// NPC 스케일 1.0 유지 (혹은 자신의 원래 스케일 기반 재조합)
					XMMATRIX matNpc = XMMatrixRotationQuaternion(rot) * XMMatrixTranslationFromVector(pos);

					XMFLOAT4X4 finalWorld;
					XMStoreFloat4x4(&finalWorld, matNpc);
					transform()->set_world_matrix(finalWorld);

					_serverPos = transform()->local_position();
					return;
				}
			}
		}
	}

	// 개별 NPC의 업데이트는 매우 짧으므로,
	// 특정 임계치를 넘는 경우만 확인하거나 누적해서 보는 것이 좋습니다.
	// 여기서는 0.1ms(100us)를 넘는 경우만 체크합니다.
	auto start = std::chrono::high_resolution_clock::now();

	if (_isFirstUpdate || !transform()) return;

	_accumulatedTime += deltaTime;

	// [최적화] 패킷이 0.5초 이상 안 오면 예측 이동(Dead Reckoning) 중지 (가출 방지)
	XMVECTOR vServerVel = XMLoadFloat3(&_serverVel);

	// [방어 코드 1] 서버 속도가 너무 빠르면 캡핑 (보스 넉백 등 예외 상황 방지)
	float speedSq = XMVectorGetX(XMVector3LengthSq(vServerVel));
	if (speedSq > 100.0f * 100.0f) { // 초속 100m 이상은 비정상으로 간주
		vServerVel = XMVector3Normalize(vServerVel) * 100.0f;
	}

	if (_accumulatedTime > 0.3f) {
		vServerVel = XMVectorZero();
	}

	// 1. 추측 항법 (Dead Reckoning)
	XMVECTOR vServerPos = XMLoadFloat3(&_serverPos);
	XMVECTOR vPredictedPos = vServerPos + (vServerVel * _accumulatedTime);

	// [방어 코드 2] 예측 위치가 서버 위치로부터 너무 멀어지면 보정 (최대 5m)
	XMVECTOR vDiff = vPredictedPos - vServerPos;
	if (XMVectorGetX(XMVector3LengthSq(vDiff)) > 5.0f * 5.0f) {
		vPredictedPos = vServerPos + XMVector3Normalize(vDiff) * 5.0f;
	}

	// 2. 위치 보간 (Lerp)
	XMVECTOR vCurrentPos = XMLoadFloat3(&transform()->local_position());

	// [NaN 체크 강화]
	if (common::XMVector3AnyNaN(vCurrentPos) || common::XMVector3AnyNaN(vPredictedPos)) {
		vCurrentPos = vServerPos;
		vPredictedPos = vServerPos;
		transform()->set_local_position(_serverPos);
		CERROR("[NPCScript] Critical NaN detected! Resetting to server position.");
	}

	float distSq = XMVectorGetX(XMVector3LengthSq(vPredictedPos - vCurrentPos));
	
	
	XMVECTOR vNextPos;
	float lerpSpeed = 10.0f;
	if (distSq > 10.0f * 10.0f) {
		// 10m 이상이면 보간하지 않고 즉시 목표 위치로 스냅 (가장 안전)
		vNextPos = vPredictedPos;
	}
	else {
		if (distSq > 0.5f * 0.5f) {
			lerpSpeed = 15.0f;
		}

		// [중요] 보간 계수 t를 [0.0, 1.0] 범위로 제한
		float clampedDelta = std::min(deltaTime, 0.1f);
		float t = std::min(1.0f, clampedDelta * lerpSpeed);
		vNextPos = XMVectorLerp(vCurrentPos, vPredictedPos, t);
	}

	// [NaN 방어]
	if (common::XMVector3AnyNaN(vNextPos)) {
		CERROR("[NPCScript] Interpolated position is NaN! Fallback to predicted pos.");
		vNextPos = vPredictedPos;
	}

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

	/*auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

	if (duration > 100) {
		CLOG("[Profiling] NPC Update Overload (ID: " << _id << "): " << duration << "us");
	}*/

	handle_animation_branching();
}

void NPCScript::late_update(float deltaTime)
{
	// HP 바 업데이트 등 필요한 로직 수행
}

void NPCScript::set_id(int64_t npc_id)
{
	_id = npc_id;
}

// --- INetSync 인터페이스 구현 ---
void NPCScript::on_receive_snapshot(const NetSnapshot& snapshot)
{
	// [구조적 강제] 핸들러에선 오직 데이터 복사만 수행! (매우 빠름)
	_pendingSnapshot = snapshot;
	_isNewDataArrived = true;
}
void NPCScript::apply_snapshot()
{
	auto owner = game_object();
	if (!owner || !owner->is_enable() || owner->is_destroyed())
	{
		return;
	}
	if(!_isNewDataArrived) return;

	_state = _pendingSnapshot.state;
	_actionId = _pendingSnapshot.action_id; // NetSnapshot에 action_id가 포함되어 있어야 함
	_grabbedById = _pendingSnapshot.grabbed_by_id; // [추가]
	_grabSlot = _pendingSnapshot.grab_slot;         // [추가]
	set_hp(_pendingSnapshot.hp);                   // [추가] HP 동기화

	_serverPos = _pendingSnapshot.pos;
	_serverVel = _pendingSnapshot.vel;
	_accumulatedTime = 0.0f;

	if (_npcType == common::packet::NPCType::Elevator) {
		_serverRot = _pendingSnapshot.rot;
	}
	else {
		// --- 1. 서버에서 받은 회전값(rot)에 Y축 180도 추가 회전 적용 ---
		XMVECTOR qServer = XMLoadFloat4((XMFLOAT4*)&_pendingSnapshot.rot);
		XMVECTOR qRotate180 = XMQuaternionRotationRollPitchYaw(0, XM_PI, 0); // Y축 180도(PI) 회전
		XMVECTOR qFinal = XMQuaternionMultiply(qServer, qRotate180);         // 회전 결합

		// 보정된 회전값을 _serverRot에 저장
		XMStoreFloat4(&_serverRot, qFinal);
	}

	// [핵심] 첫 번째 데이터를 받으면 이 플래그를 반드시 꺼줘야 합니다!
	// 이게 true로 남아있으면 update() 함수가 맨 위에서 return 됩니다.
	if (_isFirstUpdate) {
		if (transform()) {
			transform()->set_local_position(_serverPos);
			transform()->set_local_rotation(_serverRot);
		}
		_isFirstUpdate = false;
	}
	_isNewDataArrived = false;
}