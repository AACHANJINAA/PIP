#include "stdafx.h"
#include "MainPlayerScript.h"
#include "GameFramework.h"
#include "InputManager.h"
#include "NetworkManager.h"
#include "ObjectManager.h"
#include "RenderComponent.h"
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

	if (_hpBarUI)
	{
		float ratio = static_cast<float>(_hp) / static_cast<float>(_maxHp);
		_hpBarUI->set_size_x(_hpBarMaxWidth * ratio);
	}
}

void MainPlayerScript::update(float deltaTime)
{
	auto current_transform = this->transform();
	if (!current_transform) return;

	auto animation_comp = game_object()->get_component<AnimationComponent>();

	common::Vec3 move_direction{};
	bool is_moving = false;

	XMFLOAT3 camForward = _camera->transform()->forward();
	XMFLOAT3 camFwdV = Vector3::Normalize({ camForward.x, 0.0f, camForward.z });
	XMFLOAT3 camRightV = Vector3::Normalize(Vector3::CrossProduct({ 0, 1.f, 0 }, camFwdV));

	if (InputManager::instance()->IsKeyPress('W')) {
		move_direction = Vector3::Add(move_direction, camFwdV);
		is_moving = true;
	}
	if (InputManager::instance()->IsKeyPress('A')) {
		XMFLOAT3 playerLeft = Vector3::ScalarProduct(camRightV, -1.0f, false);
		move_direction = Vector3::Add(move_direction, playerLeft);
		is_moving = true;
	}
	if (InputManager::instance()->IsKeyPress('S')) {
		XMFLOAT3 modelBackward = Vector3::ScalarProduct(camFwdV, -1.0f, false);
		move_direction = Vector3::Add(move_direction, modelBackward);
		is_moving = true;
	}
	if (InputManager::instance()->IsKeyPress('D')) {
		move_direction = Vector3::Add(move_direction, camRightV);
		is_moving = true;
	}
	if (InputManager::instance()->IsKeyPress('Q')) {
		move_direction = Vector3::Add(move_direction ,common::Vec3Up);
		is_moving = true;
	}
	if (InputManager::instance()->IsKeyPress('E')) {
		move_direction = Vector3::Add(move_direction ,common::Vec3Down);
		is_moving = true;
	}

	if (InputManager::instance()->IsKeyPress(VK_SPACE))
	{
		NetworkManager::instance()->SendActionPacket(
			common::packet::ActionType::NORMAL_ATTACK,
			0, -1,
			current_transform->local_position(),
			current_transform->local_rotation()
		);
	}

	if (InputManager::instance()->IsKeyDown('P')) _speed += 1.f;
	if (InputManager::instance()->IsKeyDown('M')) if(_speed > 5.f) _speed -= 1.f;

	if (InputManager::instance()->IsKeyPress(VK_SPACE)) {
		if (_attackRangeObject) _attackRangeObject->get_component<PhysicsColliderComponent>()->set_active(true);
	} else {
		if (_attackRangeObject) _attackRangeObject->get_component<PhysicsColliderComponent>()->set_active(false);
	}
	
	// ---------------------------------------------------------
	// [넉백 물리 연산] 서버와 동일한 공식 (Friction 35.0f)
	// ---------------------------------------------------------
	float impactSpeed = common::Length(_impactVelocity);
	if (impactSpeed > 0.1f) {
		_impactVelocity = common::Normalize(_impactVelocity) * std::max(0.0f, impactSpeed - 35.0f * deltaTime);
	} else {
		_impactVelocity = {0,0,0};
	}

	auto anim_comp = game_object()->get_component<AnimationComponent>();
	common::Vec3 currentPos = current_transform->local_position();
	common::Vec3 new_pos = currentPos;

	auto cc = game_object()->get_component<PhysicsCharacterControllerComponent>();
	if (cc) {
		// 조작 속도(XZ) 설정
		common::Vec3 moveVel = is_moving ? common::Normalize(move_direction) * _speed : common::Vec3{ 0, 0, 0 };

		// [중요] 현재 Y속도를 가져와서 XZ만 교체
		common::Vec3 currentVel = cc->get_velocity();
		cc->set_velocity({ moveVel.x + _impactVelocity.x, currentVel.y, moveVel.z + _impactVelocity.z });

		// [삭제] cc->step_physics(deltaTime); 호출할 필요 없음!
		// -> GameFramework가 1/60초마다 자동으로 fixed_update를 불러줌.

		// 4. [핵심] 보정 오차(Visual Offset)를 매 프레임 줄여나감 (보간)
		// 0.1초(deltaTime * 10) 정도의 속도로 부드럽게 원래 위치로 돌아가게 합니다.
		float lerpFactor = std::min(1.0f, deltaTime * 10.0f);
		_visualOffset = _visualOffset * (1.0f - lerpFactor);

		// 5. 최종 렌더링 위치 = 물리 위치 + 보정 오프셋
		transform()->set_local_position(cc->get_position() + _visualOffset);
	}


	if (is_moving) {
		if (anim_comp) anim_comp->set_state(common::packet::OBJECT_STATE::WALK);
		move_direction = common::Normalize(move_direction);

		if (_camera && _camera->transform()) {
			XMFLOAT3 camF = _camera->transform()->forward();
			float yawDegrees = XMConvertToDegrees(atan2f(camF.x, camF.z)) - 180.f;
			current_transform->set_local_rotation(0.0f, yawDegrees, 0.0f);
		}

	}
	else if(InputManager::instance()->IsKeyPress(VK_SPACE))
	{
		if (anim_comp) anim_comp->set_state(common::packet::OBJECT_STATE::ATTACK);
	}
	else 
	{
		if (anim_comp) anim_comp->set_state(common::packet::OBJECT_STATE::IDLE);
	}

	// --- 20ms 주기 위치 전송 ---
	_sendTimer += deltaTime;
	if (_sendTimer >= SENDINTERVAL) {
		_sendTimer = 0.f;
		common::packet::OBJECT_STATE current_state = anim_comp ? anim_comp->get_state() : common::packet::OBJECT_STATE::IDLE;
		uint32_t currentTick = static_cast<uint32_t>(GetTickCount64());
		NetworkManager::instance()->SendMovePacket(current_transform->local_position(), current_transform->local_rotation(), current_state, currentTick);
	}
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

	renderer->set_mesh(walkMesh);

	animation_component->add_state_mapping(common::packet::OBJECT_STATE::IDLE, "idle", idleMesh);
	animation_component->add_state_mapping(common::packet::OBJECT_STATE::WALK, "walk", walkMesh);
	animation_component->add_state_mapping(common::packet::OBJECT_STATE::ATTACK, "attack", idleMesh);

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
	owner->transform()->set_local_scale({ 2.0f, 2.0f, 2.0f });

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

void MainPlayerScript::move_pos(common::packet::MOVE_TYPE cmd)
{
	// ... 기존 레거시 함수 유지 ...
}
