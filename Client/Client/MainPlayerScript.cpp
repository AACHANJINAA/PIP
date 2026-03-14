#include "stdafx.h"
#include "MainPlayerScript.h"
#include "GameFramework.h"
#include "InputManager.h"
#include "NetworkManager.h"
#include "ObjectManager.h"
#include "RenderComponent.h"
#include "ReadGLTFMesh.h"
#include "Renderer.h"

#include "ResourceManager.h"
#include "SceneManager.h"
#include "TerrainLoader.h"
#include "TransformComponent.h"
#include "TimerManager.h"
#include "AnimationComponent.h"

#include "PhysicsColliderComponent.h"
#include "WeaponScript.h"
#include "MonsterHPComponent.h"
#include "PhysicsCharacterControllerComponent.h"

void MainPlayerScript::set_hp(int hp)
{
	_hp = std::clamp(hp, 0, _maxHp);
	_displayHp = static_cast<float>(_hp);

	// _hpBar_ui가 null이면 매번 재탐색
	if (!_hpBar_ui)
	{
		auto hp_bar_obj = ObjectManager::instance()->find_by_name("HP_Bar");
		if (hp_bar_obj)
			set_hp_bar_ui(hp_bar_obj->get_component<UIRenderComponent>());
	}
	// UI 직접 갱신은 하지 않음 — update()의 lerp가 담당
}

void MainPlayerScript::update(float deltaTime)
{
	update_hp_bar(deltaTime);

	handle_input(deltaTime);

	handle_state(deltaTime);
	update_physics_and_visuals(deltaTime);
	send_network_sync(deltaTime);

	//// HP Bar lerp 갱신
	//if (_hpBar_ui)
	//{
	//	float lerp = std::min(1.0f, deltaTime * 10.0f);
	//	_displayHp += (static_cast<float>(_hp) - _displayHp) * lerp;
	//	float ratio = _displayHp / static_cast<float>(_maxHp);
	//	_hpBar_ui->set_size_x(_hpBar_maxWidth * ratio);
	//	_hpBar_ui->set_uv_scale(ratio, 1.0f);
	//}
	//
	//auto current_transform = this->transform();
	//if (!current_transform) return;

	//auto animation_comp = game_object()->get_component<AnimationComponent>();
	//if (!animation_comp) return;

	//common::Vec3 move_direction{};
	//bool is_moving = false;

	//XMFLOAT3 camForward = _camera->transform()->forward();
	//XMFLOAT3 camFwdV = Vector3::Normalize({ camForward.x, 0.0f, camForward.z });
	//XMFLOAT3 camRightV = Vector3::Normalize(Vector3::CrossProduct({ 0, 1.f, 0 }, camFwdV));

	//if (InputManager::instance()->IsKeyPress('W')) {
	//	move_direction = Vector3::Add(move_direction, camFwdV);
	//	is_moving = true;
	//}
	//if (InputManager::instance()->IsKeyPress('A')) {
	//	XMFLOAT3 playerLeft = Vector3::ScalarProduct(camRightV, -1.0f, false);
	//	move_direction = Vector3::Add(move_direction, playerLeft);
	//	is_moving = true;
	//}
	//if (InputManager::instance()->IsKeyPress('S')) {
	//	XMFLOAT3 modelBackward = Vector3::ScalarProduct(camFwdV, -1.0f, false);
	//	move_direction = Vector3::Add(move_direction, modelBackward);
	//	is_moving = true;
	//}
	//if (InputManager::instance()->IsKeyPress('D')) {
	//	move_direction = Vector3::Add(move_direction, camRightV);
	//	is_moving = true;
	//}
	//if (InputManager::instance()->IsKeyPress('Q')) {
	//	move_direction = Vector3::Add(move_direction ,common::Vec3Up);
	//	is_moving = true;
	//}
	//if (InputManager::instance()->IsKeyPress('E')) {
	//	move_direction = Vector3::Add(move_direction ,common::Vec3Down);
	//	is_moving = true;
	//}

	//if (InputManager::instance()->IsKeyPress(VK_SPACE))
	//{
	//	NetworkManager::instance()->SendActionPacket(
	//		common::packet::ActionType::NORMAL_ATTACK,
	//		0, -1,
	//		current_transform->local_position(),
	//		current_transform->local_rotation()
	//	);
	//}

	//if (InputManager::instance()->IsKeyDown('P')) _speed += 1.f;
	//if (InputManager::instance()->IsKeyDown('M')) if(_speed > 5.f) _speed -= 1.f;

	//if (InputManager::instance()->IsKeyPress(VK_SPACE)) {
	//	if (_attackRangeObject) _attackRangeObject->get_component<PhysicsColliderComponent>()->set_active(true);
	//} else {
	//	if (_attackRangeObject) _attackRangeObject->get_component<PhysicsColliderComponent>()->set_active(false);
	//}
	//
	//// ---------------------------------------------------------
	//// [넉백 물리 연산] 서버와 동일한 공식 (Friction 35.0f)
	//// ---------------------------------------------------------
	//float impactSpeed = common::Length(_impactVelocity);
	//if (impactSpeed > 0.1f) {
	//	_impactVelocity = common::Normalize(_impactVelocity) * std::max(0.0f, impactSpeed - 35.0f * deltaTime);
	//} else {
	//	_impactVelocity = {0,0,0};
	//}

	//auto anim_comp = game_object()->get_component<AnimationComponent>();
	//common::Vec3 currentPos = current_transform->local_position();
	//common::Vec3 new_pos = currentPos;

	//auto cc = game_object()->get_component<PhysicsCharacterControllerComponent>();
	//if (cc) {
	//	// 조작 속도(XZ) 설정
	//	common::Vec3 moveVel = is_moving ? common::Normalize(move_direction) * _speed : common::Vec3{ 0, 0, 0 };

	//	// [중요] 현재 Y속도를 가져와서 XZ만 교체
	//	common::Vec3 currentVel = cc->get_velocity();
	//	cc->set_velocity({ moveVel.x + _impactVelocity.x, currentVel.y, moveVel.z + _impactVelocity.z });

	//	// [삭제] cc->step_physics(deltaTime); 호출할 필요 없음!
	//	// -> GameFramework가 1/60초마다 자동으로 fixed_update를 불러줌.

	//	// 4. [핵심] 보정 오차(Visual Offset)를 매 프레임 줄여나감 (보간)
	//	// 0.1초(deltaTime * 10) 정도의 속도로 부드럽게 원래 위치로 돌아가게 합니다.
	//	float lerpFactor = std::min(1.0f, deltaTime * 10.0f);
	//	_visualOffset = _visualOffset * (1.0f - lerpFactor);

	//	// 5. 최종 렌더링 위치 = 물리 위치 + 보정 오프셋
	//	transform()->set_local_position(cc->get_position() + _visualOffset);
	//}


	//if (is_moving) {
	//	if (anim_comp) anim_comp->set_state(common::packet::OBJECT_STATE::WALK);
	//	move_direction = common::Normalize(move_direction);

	//	if (_camera && _camera->transform()) {
	//		XMFLOAT3 camF = _camera->transform()->forward();
	//		float yawDegrees = XMConvertToDegrees(atan2f(camF.x, camF.z)) - 180.f;
	//		current_transform->set_local_rotation(0.0f, yawDegrees, 0.0f);
	//	}

	//}
	//else if(InputManager::instance()->IsKeyPress(VK_SPACE))
	//{
	//	if (anim_comp) anim_comp->set_state(common::packet::OBJECT_STATE::ATTACK);
	//}
	//else 
	//{
	//	if (anim_comp) anim_comp->set_state(common::packet::OBJECT_STATE::IDLE);
	//}

	//// --- 20ms 주기 위치 전송 ---
	//_sendTimer += deltaTime;
	//if (_sendTimer >= SENDINTERVAL) {
	//	_sendTimer = 0.f;
	//	common::packet::OBJECT_STATE current_state = anim_comp ? anim_comp->get_state() : common::packet::OBJECT_STATE::IDLE;
	//	uint32_t currentTick = static_cast<uint32_t>(GetTickCount64());
	//	NetworkManager::instance()->SendMovePacket(current_transform->local_position(), current_transform->local_rotation(), current_state, currentTick);
	//}
}

void MainPlayerScript::fixed_update(float deltaTime)
{
	ScriptComponent::fixed_update(deltaTime);
}

void MainPlayerScript::awake()
{
	auto owner = game_object();

	// PhysicsCharacterControllerComponent 초기화
	auto cc = game_object()->get_component<PhysicsCharacterControllerComponent>();
	if (cc) {
		cc->initialize(1.8f, 0.5f);
		cc->set_position(transform()->local_position());
	}
	// Animationcomponent
	auto animation_component = owner->get_component<AnimationComponent>();
	if (!animation_component)
	{
		CERROR("애니메이션 컴포넌트 추가 안됨 튜플 확인!");
	}
	// RenderComponent
	auto renderer = owner->get_component<RenderComponent>();
	if (!renderer)
	{
		CERROR("렌더러 컴포넌트 추가 안됨 튜플 확인!");
	}

	auto idleMesh =
		ResourceManager::instance()->load_mesh("Resource/Character/Brute_idle/Brute_idle.gltf", true, "idle");
	auto walkMesh =
		ResourceManager::instance()->load_mesh("Resource/Character/Brute_Walk/Brute_Walk.gltf", true, "walk");

	// Brute_die -> idle 메쉬를 기준으로 사용하여 애니메이션만 로드 (메쉬는 재사용)
	dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only("Resource/Character/Brute_die/Brute_die.gltf", "die");

	renderer->set_mesh(walkMesh);

	animation_component->add_state_mapping(common::packet::OBJECT_STATE::IDLE, "idle", idleMesh);
	animation_component->add_state_mapping(common::packet::OBJECT_STATE::WALK, "walk", walkMesh);
	animation_component->add_state_mapping(common::packet::OBJECT_STATE::ATTACK1, "attack", idleMesh);
	animation_component->add_state_mapping(common::packet::OBJECT_STATE::DIE, "die", idleMesh);

	// 초기 상태 설정 (강제로 적용하여 메쉬/애니메이션 로드)
	animation_component->set_state(common::packet::OBJECT_STATE::WALK); // 잠시 WALK로 바꿨다가
	animation_component->set_state(common::packet::OBJECT_STATE::IDLE); // IDLE로 설정하면 로직이 돕니다.

	// -------------- 재질 생성부 ----------------------- //
	// ResourceManager을 통해 재질 생성 및 쉐이더 할당
	std::string material_name = "player_material"; // player는 고정된 재질
	ResourceManager::instance()->create_material(material_name);
	ResourceManager::instance()->set_shader_for_material(material_name, "skinned");
	// gltf
	renderer->set_pso_name("skinned");

	// 위치, 회전 정보
	owner->transform()->set_local_scale({ 1.0f, 1.0f, 1.0f });

	_camera = ObjectManager::instance()->find_by_name("FreeCamera").get();
	if (_camera) {
		XMFLOAT3 camF = _camera->transform()->forward();
		float yawDegrees = XMConvertToDegrees(atan2f(camF.x, camF.z)) - 180.f;
		transform()->set_local_rotation(0.0f, yawDegrees, 0.0f);
	}

	// --- 공격 범위 콜라이더 오브젝트 생성 ---
	_attackRangeObject = ObjectManager::instance()->create_game_object("AttackRange");
	_attackRangeObject->transform()->set_parent(game_object()->transform());
	_attackRangeObject->transform()->set_local_position({ 0, 0, 0 });
	auto col = _attackRangeObject->add_component<PhysicsColliderComponent>();
	col->initialize(PhysicsColliderComponent::ShapeType::Sphere, { 3.0f, 0, 0 }, { 0,0,0 }, { 0,0,0 }, true);
	_attackRangeObject->add_component<WeaponScript>();
	col->set_active(false);
}

void MainPlayerScript::sync_with_server(const common::Vec3& pos, const common::Quat& rot)
{
	auto cc = game_object()->get_component<PhysicsCharacterControllerComponent>();
	if (!cc) return;

	// 1. 현재 클라이언트의 물리 위치와 서버 위치의 차이를 계산
	common::Vec3 currentPhysicsPos = cc->get_position();

	// 2. 물리 위치는 서버 위치로 즉시 옮김 (서버 권위 인정)
	cc->set_position(pos);
	cc->set_velocity({ 0, 0, 0 });

	// 3. [핵심] 대신 시각적으로는 튀지 않게 오프셋을 설정
	// "물리는 옮겼지만, 눈에 보이는 모델은 이전 위치에서 서서히 이동해라"는 뜻입니다.
	_visualOffset = currentPhysicsPos - pos;

	transform()->set_local_rotation(rot);
}
//---------------------------------------------------------- private functions ----------------------------------------------------------
//--- update() 내부에서 호출되는 기능 분리용 함수들 ---
void MainPlayerScript::update_hp_bar(float deltaTime)
{
	if (_hpBar_ui)
	{
		float lerp = std::min(1.0f, deltaTime * 10.0f);
		_displayHp += (static_cast<float>(_hp) - _displayHp) * lerp;
		float ratio = _displayHp / static_cast<float>(_maxHp);
		_hpBar_ui->set_size_x(_hpBar_maxWidth * ratio);
		_hpBar_ui->set_uv_scale(1.0f, 1.0f);
	}
}
void MainPlayerScript::handle_state(float deltaTime)
{
	auto anim_comp = game_object()->get_component<AnimationComponent>();
	if (!anim_comp) return;

	// DW추가 : 사망 상태 로직 추가
	if (0 >= hp())
	{
		// set_state에도 애니메이션 루프 설정 추가
		anim_comp->set_state(common::packet::OBJECT_STATE::DIE,false);
		return;
	}

	if (_isAttacking) {
		anim_comp->set_state(common::packet::OBJECT_STATE::ATTACK1);

		// 실제 타격 패킷 전송 (애니메이션 중간 지점)
		float progress = anim_comp->get_anim_time();
		float duration = anim_comp->get_anim_duration();
		if (!_packetSent && progress >= (duration * 0.3f)) {
			NetworkManager::instance()->SendActionPacket(common::packet::ActionType::NORMAL_ATTACK, 0, -1,
				transform()->local_position(), transform()->local_rotation());
			_packetSent = true;
		}

		// 공격 종료 체크
		if (anim_comp->is_anim_finished()) {
			_isAttacking = false;
			_packetSent = false; // 중요: 다음 공격을 위해 리셋

			// 즉시 서버에 IDLE 상태임을 알려야 함
			anim_comp->set_state(common::packet::OBJECT_STATE::IDLE);
			send_network_sync(0.0f);
		}
	}
	else {
		// 공격 중이 아닐 때만 WALK/IDLE 전환
		if (common::Length(_currentMoveDir) > 0.01f) {
			anim_comp->set_state(common::packet::OBJECT_STATE::WALK);
		}
		else {
			anim_comp->set_state(common::packet::OBJECT_STATE::IDLE);
		}
	}
}
void MainPlayerScript::handle_input(float deltaTime)
{
	// DW추가 : 사망 상태 로직 추가
	if (0 >= hp())
	{
		// 사망 상태에서는 입력 무시
		_currentMoveDir = { 0, 0, 0 }; // 이동 입력 초기화
		return;
	}

	common::Vec3 move_direction{};
	bool is_moving_input = false;

	XMFLOAT3 camForward = _camera->transform()->forward();
	XMFLOAT3 camFwdV = Vector3::Normalize({ camForward.x, 0.0f, camForward.z });
	XMFLOAT3 camRightV = Vector3::Normalize(Vector3::CrossProduct({ 0, 1.f, 0 }, camFwdV));

	// 이동 입력 처리
	if (InputManager::instance()->IsKeyPress('W')) {
		move_direction = Vector3::Add(move_direction, camFwdV);
		is_moving_input = true;
	}
	if (InputManager::instance()->IsKeyPress('A')) {
		move_direction = Vector3::Add(move_direction,
			Vector3::ScalarProduct(camRightV, -1.0f)); is_moving_input = true;
	}
	if (InputManager::instance()->IsKeyPress('S')) {
		move_direction = Vector3::Add(move_direction,
			Vector3::ScalarProduct(camFwdV, -1.0f)); is_moving_input = true;
	}
	if (InputManager::instance()->IsKeyPress('D')) {
		move_direction = Vector3::Add(move_direction, camRightV);
		is_moving_input = true;
	}

	if (is_moving_input) {
		_currentMoveDir = common::Normalize(move_direction);
		// 이동 중이면 카메라 방향에 맞춰 회전 (기존 로직)
		float yawDegrees = XMConvertToDegrees(atan2f(camFwdV.x, camFwdV.z)) - 180.f;
		transform()->set_local_rotation(0.0f, yawDegrees, 0.0f);
	}
	else {
		_currentMoveDir = { 0, 0, 0 };
	}

	// 공격 입력 (공격 중이 아닐 때만 새 공격 시작 가능)
	if (!_isAttacking && InputManager::instance()->IsKeyDown(VK_SPACE)) {
		_isAttacking = true;
		_packetSent = false;
		// 공격 시작 시점에 즉시 상태를 ATTACK으로 변경하도록 update_state에서 처리됨
	}
}
void MainPlayerScript::update_physics_and_visuals(float deltaTime)
{
	float impactSpeed = common::Length(_impactVelocity);
	if (impactSpeed > 0.1f) {
		// 매 프레임 35.0f의 마찰력으로 속도를 줄임
		_impactVelocity = common::Normalize(_impactVelocity) * std::max(0.0f, impactSpeed - 35.0f * deltaTime);
	}
	else {
		_impactVelocity = { 0,0,0 };
	}

	auto pcc = game_object()->get_component<PhysicsCharacterControllerComponent>();
	if (pcc) {
		// 공격 중에도 _currentMoveDir에 값이 있다면 이동함 (이동 공격)
		common::Vec3 moveVel = _currentMoveDir * _speed;
		common::Vec3 currentVel = pcc->get_velocity();

		pcc->set_velocity({ moveVel.x + _impactVelocity.x, currentVel.y, moveVel.z + _impactVelocity.z });

		// 시각적 보정 (기존 로직)
		float lerpFactor = std::min(1.0f, deltaTime * 10.0f);
		_visualOffset = _visualOffset * (1.0f - lerpFactor);
		transform()->set_local_position(pcc->get_position() + _visualOffset);
	}
}
void MainPlayerScript::send_network_sync(float deltaTime)
{
	_sendTimer += deltaTime;
	if (_sendTimer >= SENDINTERVAL || deltaTime == 0.0f) { // deltaTime 0은 즉시 전송용
		_sendTimer = 0.f;
		auto anim_comp = game_object()->get_component<AnimationComponent>();
		common::packet::OBJECT_STATE current_state = anim_comp ? anim_comp->get_state() :
			common::packet::OBJECT_STATE::IDLE;
		uint32_t currentTick = static_cast<uint32_t>(GetTickCount64());

		NetworkManager::instance()->SendMovePacket(transform()->local_position(), transform()->local_rotation(),
			current_state, currentTick);
	}
}


