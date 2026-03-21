#include "stdafx.h"
#include "MainPlayerScript.h"


#include "InputManager.h"
#include "NetworkManager.h"
#include "ObjectManager.h"
#include "RenderComponent.h"
#include "ReadGLTFMesh.h"

#include "UIManager.h"

#include "ResourceManager.h"

#include "TerrainLoader.h"
#include "TransformComponent.h"

#include "AnimationComponent.h"
#include "LongswordScript.h"

#include "PhysicsColliderComponent.h"
#include "WeaponScript.h"
#include "MonsterHPComponent.h"
#include "PhysicsCharacterControllerComponent.h"
#include "SocketComponenet.h"

void MainPlayerScript::set_hp(const int hp)
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
	/*auto cc = game_object()->get_component<PhysicsCharacterControllerComponent>();
	if (cc) {
		cc->initialize(1.8f, 0.5f);
		cc->set_position(transform()->local_position());
	}*/
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
		ResourceManager::instance()->load_mesh("Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf", true);

	std::string animationpath = "Resource/Character/DarkKnight/DKF_animations/";
	std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Idle_Alert.gltf", "idle");
	std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Walk_Alert_Fwd.gltf", "walk");
	std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Attack_02.gltf", "attack02");
	std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Death.gltf", "death");


	renderer->set_mesh(idleMesh);

	animation_component->add_animation("idle", idleMesh, "idle");
	animation_component->add_animation("walk", idleMesh, "walk");
	animation_component->add_animation("attack", idleMesh, "attack02");
	animation_component->add_animation("die", idleMesh, "death");

	// 초기 상태 설정 (강제로 적용하여 메쉬/애니메이션 로드)
	animation_component->play("idle");

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
	auto socket = owner->get_component<SocketComponenet>();

	// 다크나이트의 hand_l 오프셋을 참고하여 hand_r용으로 미러링한 값입니다.
	// 좌표와 회전은 모델을 보면서 미세 조정이 필요할 수 있습니다.
	_currentWeaponObject = socket->add_connecting(
		"MainWeapon",
		"hand_r", // 반대쪽 손
		"Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf",
		{ -0.06f, -0.8f, 0.16f },   // hand_l 기준 X값 반전 시도
		{ 10.f, -90.f, 0.f },       // 오른손 파지 각도에 맞게 회전 조정
		{ 2.f, 2.f, 2.f }
	);

	// 무리 렌더링 끄기
	_currentWeaponObject->get_component<RenderComponent>()->set_enabled(false);

	// --- 3. 무기 오브젝트에 기능(스크립트 + 콜라이더) 추가 ---
	if (_currentWeaponObject) {
		// 물리 바디를 위한 콜라이더 추가
		_currentWeaponObject->add_component<PhysicsColliderComponent>();

		// 롱소드 로직 추가 (내부에서 콜라이더 initialize 호출됨)
		_currentWeapon = _currentWeaponObject->add_component<LongswordScript>();
		_currentWeapon->set_attack_active(true);
	}
}

void MainPlayerScript::sync_with_server(const common::packet::SC_PACKET_MOVE& movePacket)
{
	// 서버가 준 좌표로 물리/시각적 위치 동기화
	common::Vec3 currentVisualPos = transform()->local_position();

	// 1. 서버 좌표를 내 '진짜' 좌표로 수용
	// 만약 클라이언트 Jolt를 뺐다면 transform에 직접 적용
	// _impactVelocity 등도 서버 값에 맞춰 동기화 필요할 수 있음

	// 2. [중요] 보간 오프셋 계산 (끊김 방지)
	// "현재 내 가짜 위치"와 "서버가 준 진짜 위치"의 차이를 기억해뒀다가 서서히 줄임
	_visualOffset = currentVisualPos - movePacket._position;

	transform()->set_local_position(movePacket._position); // 즉시 물리적 위치 텔레포트
	// transform->set_local_rotation(movePacket._rotation); // 회전도 동기화
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
		_state = common::packet::EntityState::DEAD;
		// set_state에도 애니메이션 루프 설정 추가
		anim_comp->play("die", false);
		// 사망 상태의 ui 업데이트
		//die_ui_update(deltaTime);
		return;
	}

	if (_isAttacking) {
		_state = common::packet::EntityState::ACTION;
		anim_comp->play("attack", false);

		// 실제 타격 패킷 전송 (애니메이션 중간 지점)
		float progress = anim_comp->get_anim_time();
		float duration = anim_comp->get_anim_duration();

		// [추가] 특정 프레임(30% ~ 60%) 동안 히트박스 활성화
		if (_currentWeapon) {
			if (progress >= (duration * 0.3f) && progress <= (duration * 0.6f)) {
				_currentWeapon->set_attack_active(true);
			}
			else {
				_currentWeapon->set_attack_active(false);
			}
		}

		if (!_packetSent && progress >= (duration * 0.3f)) {
			NetworkManager::instance()->SendActionPacket(common::packet::ActionType::NORMAL_ATTACK, 0, -1,
				transform()->local_position(), transform()->local_rotation());
			_packetSent = true;
		}

		// 공격 종료 체크
		if (anim_comp->is_anim_finished()) {
			_isAttacking = false;
			_packetSent = false; // 중요: 다음 공격을 위해 리셋
			_actionId = 0;
			_state = common::packet::EntityState::IDLE;

			if (_currentWeapon) _currentWeapon->set_attack_active(false);

			// 즉시 서버에 IDLE 상태임을 알려야 함
 			anim_comp->play("idle");
			send_network_sync(0.0f);
		}
	}
	else {
		// 공격 중이 아닐 때만 WALK/IDLE 전환
		if (common::Length(_currentMoveDir) > 0.01f) {
			_state = common::packet::EntityState::MOVE;
			anim_comp->play("walk");
		}
		else {
			_state = common::packet::EntityState::IDLE;
			anim_comp->play("idle");
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
		_actionId = common::packet::ActionID::Common::Attack;
		// 공격 시작 시점에 즉시 상태를 ATTACK으로 변경하도록 update_state에서 처리됨
	}

	//// [추가] 스킬 입력 (R 키: 꿰뚫는 일격)
	//if (!_isAttacking && _currentWeapon && _currentWeapon->can_use_skill() && InputManager::instance()->IsKeyDown('R')) {
	//	_isAttacking = true;
	//	_isChargingSkill = true;
	//	_skillChargeTimer = 0.0f;
	//	_packetSent = false;
	//	_actionId = 2; // 꿰뚫는 일격 ID (가칭)
	//	_currentWeapon->start_charge();
	//}

	//if (_isChargingSkill) {
	//	_skillChargeTimer += deltaTime;
	//	if (_skillChargeTimer >= 2.0f) { // 2초 캐스팅 완료
	//		_isChargingSkill = false;
	//		_currentWeapon->use_skill();
	//		// 실제 서버 전송 로직은 handle_state 등에서 애니메이션과 맞춰 처리
	//	}
	//}
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

	// [수정] 물리 엔진(cc) 대신 Transform을 직접 제어
	common::Vec3 currentPos = transform()->local_position();
	common::Vec3 moveVel = _currentMoveDir * _speed;

	// 단순 선형 예측 이동 (지형 무시하고 일단 가봄)
	common::Vec3 predictedPos = currentPos + (moveVel + _impactVelocity) * deltaTime;

	// 시각적 오차 보정 (서버 보정 패킷 수신 시 발생한 오차를 서서히 줄임)
	float lerpFactor = std::min(1.0f, deltaTime * 10.0f);
	_visualOffset = _visualOffset * (1.0f - lerpFactor);

	// 최종 위치 적용
	transform()->set_local_position(predictedPos + _visualOffset);
}
void MainPlayerScript::send_network_sync(float deltaTime)
{
	_timer -= deltaTime;
	if (_timer < 0.0f)
	{
		_timer = 2.0f;
		auto pos = position();
		CLOG("player pos (" << pos.x << "," << pos.y <<"," << pos.z << ")");
		
	}

	_sendTimer += deltaTime;
	if (_sendTimer >= SENDINTERVAL || deltaTime == 0.0f) { // deltaTime 0은 즉시 전송용
		_sendTimer = 0.f;
		uint32_t currentTick = static_cast<uint32_t>(GetTickCount64());

		NetworkManager::instance()->SendMovePacket(transform()->local_position(), transform()->local_rotation(),
			_state, _actionId,currentTick);
	}
}

void MainPlayerScript::die_ui_update(float deltaTime)
{
	static float alpha_background = 0.0f;
	static float alpha_text = 0.0f;
	static float timer = 0.0f;
	if (0 < hp())
	{
		timer = 0.f;
		alpha_background = 0.f;
		alpha_text = 0.f;
		UIManager::instance()->set_visible(UILayer::MIDDLE, "Death_Background_UI", false); // 처음에는 보이지 않도록 설정
		UIManager::instance()->set_visible(UILayer::FRONT, "Death_UI", false); // 처음에는 보이지 않도록 설정
	}

	if (timer <= 2.f)
	{
		timer += deltaTime;
		return;
	}

	// 셋팅 -> 추후에 bt에서는 awake에서
	UIManager::instance()->set_visible(UILayer::MIDDLE, "Death_Background_UI", true);
	UIManager::instance()->set_visible(UILayer::FRONT, "Death_UI", true);

	// 실제 동작 -> update에서
	alpha_background += deltaTime * 0.25f;
	alpha_text += deltaTime * 0.25f * 0.5f;

	alpha_background = std::min(alpha_background, 1.f);
	alpha_text = std::min(alpha_text, 1.f);

	auto background = UIManager::instance()->ui_component(UILayer::MIDDLE, "Death_Background_UI");
	auto deathUI = UIManager::instance()->ui_component(UILayer::FRONT, "Death_UI");
	if (background)
	{
		background->set_color(XMFLOAT4(1, 1, 1, alpha_background)); // 배경은 반투명 검정

	}
	if(deathUI)
	{
		deathUI->set_color(XMFLOAT4(1, 1, 1, alpha_text)); // 배경은 반투명 검정
	}
	
}


