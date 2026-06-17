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

#include "PhysicsColliderComponent.h"
#include "WeaponScript.h"
#include "MonsterHPComponent.h"
#include "PhysicsCharacterControllerComponent.h"
#include "SocketComponenet.h"
#include "ParticleSystemComponent.h"
#include "ParticleRenderComponent.h"
#include "GameFramework.h"
#include "FreeCameraScript.h"

void MainPlayerScript::set_hp(int hp)
{
	_hp = std::clamp(hp, 0, _maxHp);
	
	//_displayHp = static_cast<float>(_hp);

	// _hpBar_ui가 null이면 매번 재탐색
	if (!_hpBar_ui)
	{
		auto hp_bar_obj = ObjectManager::instance()->find_by_name("HP_Bar");
		if (hp_bar_obj)
			set_hp_bar_ui(hp_bar_obj->get_component<UIRenderComponent>());
	}
	// UI 직접 갱신은 하지 않음 — update()의 lerp가 담당
}

void MainPlayerScript::set_mp(int mp)
{
	_mp = std::clamp(mp, 0, _maxMp);

	if (!_mpBar_ui)
	{
		auto mp_bar_obj = ObjectManager::instance()->find_by_name("MP_Bar");
		if (mp_bar_obj)
		{
			_mpBar_ui = mp_bar_obj->get_component<UIRenderComponent>();
			if (_mpBar_ui) {
				_mpBar_maxWidth = _mpBar_ui->get_size_x(); 
			}
		}
	}
}

void MainPlayerScript::update(float deltaTime)
{
	die_ui_update(deltaTime);
	update_hp_bar(deltaTime);
	update_mp_bar(deltaTime);
	
	// 스킬 이펙트가 활성화되어 있다면, 해당 이펙트의 지속 시간 체크
	if (_particleEffectObject && _particleEffectObject->is_enable())
	{
		auto psComp = _particleEffectObject->get_component<ParticleSystemComponent>();
		if (psComp && psComp->is_death_timer_end())
		{
			_particleEffectObject->set_enabled(false);
		}
	}

	handle_input(deltaTime);
	handle_state(deltaTime);
	update_physics_and_visuals(deltaTime);
	update_quest_ui(); // 퀘스트 UI 실시간 반영
	send_network_sync(deltaTime);
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
	std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Run_Alert_Fwd.gltf", "run");
	std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Attack_01.gltf", "attack01");
	std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Skill_01.gltf", "skill01");
	std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Skill_01_end.gltf", "skill01_end");
	std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Death.gltf", "death");


	renderer->set_mesh(idleMesh);

	animation_component->add_animation("idle", idleMesh, "idle");
	animation_component->add_animation("walk", idleMesh, "walk");
	animation_component->add_animation("run", idleMesh, "run");
	animation_component->add_animation("attack", idleMesh, "attack01");
	animation_component->add_animation("skill", idleMesh, "skill01");
	animation_component->add_animation("skill_end", idleMesh, "skill01_end");
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

	_logicalPosition = owner->transform()->local_position();
	_visualOffset = { 0, 0, 0 };

	_camera = ObjectManager::instance()->find_by_name("Camera").get();
	if (_camera) {
		XMFLOAT3 camF = _camera->transform()->forward();
		float yawDegrees = XMConvertToDegrees(atan2f(camF.x, camF.z));
		transform()->set_local_rotation(0.0f, yawDegrees + 180.0f, 0.0f);
	}

	// --- 공격 범위 콜라이더 오브젝트 생성 ---
	auto socket = owner->get_component<SocketComponenet>();

	// 다크나이트의 hand_l 오프셋을 참고하여 hand_r용으로 미러링한 값입니다.
	// 좌표와 회전은 모델을 보면서 미세 조정이 필요할 수 있습니다.
	_currentWeaponObject = socket->add_connecting(
		"MainWeapon",
		"hand_r", // 반대쪽 손
		"Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf",
		{ 0.f,0.f,0.f },   // hand_l 기준 X값 반전 시도
		{ -10.f, -80.f, -9.0f },       // 오른손 파지 각도에 맞게 회전 조정
		{ 10.f, 10.f, 10.f }
	);
	//
	//// 무기 렌더링 끄기
	_currentWeaponObject->get_component<RenderComponent>()->set_enabled(false);

	_SkillObject = _currentWeaponObject;

	// --- 3. 무기 오브젝트에 기능(스크립트 + 콜라이더) 추가 ---
	if (_currentWeaponObject) {
		// 물리 바디를 위한 콜라이더 추가
		_currentWeaponObject->add_component<PhysicsColliderComponent>();

		// 롱소드 로직 추가 (내부에서 콜라이더 initialize 호출됨)
		_currentWeapon = _currentWeaponObject->add_component<LongswordScript>();
		_currentWeapon->set_attack_active(true);
	}

	auto skillRender = _SkillObject->get_component<RenderComponent>();
	auto gltfMesh = dynamic_pointer_cast<ReadGLTFMesh>(skillRender->mesh());

	if (gltfMesh)
	{
		// 5만 개의 빽빽한 점 데이터를 추출합니다.
		auto targets = gltfMesh->extract_particle_targets(50000);

		// 2. 파티클 시스템 전용 오브젝트 생성 (ObjectManager 팩토리 사용)
		_particleEffectObject = ObjectManager::instance()->create_game_object("CarianParticleEffect");

		// 3. 연산 담당 컴포넌트 추가 및 데이터 전송
		auto psComp = _particleEffectObject->add_component<ParticleSystemComponent>();
		static const DirectX::XMFLOAT3 PlayerColors[4] =
		{
			DirectX::XMFLOAT3(0.863f, 0.078f, 0.235f), // crimson red
			DirectX::XMFLOAT3(0.0f, 1.0f, 0.498f), // spring green
			DirectX::XMFLOAT3(1.0f, 0.843f, 0.0f), // gold
			DirectX::XMFLOAT3(0.541f, 0.169f, 0.886f), // violet
		};

		DirectX::XMFLOAT4 color = { PlayerColors[_playerId % 4].x, PlayerColors[_playerId % 4].y, PlayerColors[_playerId % 4].z, 0.5f };
		psComp->init_particles(targets, color);

		// 4. 렌더 컴포넌트 추가
		auto prComp = _particleEffectObject->add_component<ParticleRenderComponent>();
		prComp->set_pso_name("particle_draw");

		prComp->set_particle_system(psComp);

		// 5. 위치 동기화 (대검 오브젝트의 자식으로 설정)
		_particleEffectObject->transform()->set_local_position({ 0, 0, 0 });
		_particleEffectObject->transform()->set_parent(_SkillObject->transform());

		// 초기에는 꺼둠
		_particleEffectObject->set_enabled(false);
	}
}

void MainPlayerScript::sync_with_server(const common::packet::SC_PACKET_MOVE& movePacket)
{
	/// 1. 현재 화면에 보이고 있던 최종 위치 계산
	common::Vec3 currentVisualPos = _logicalPosition + _visualOffset;

	// 2. 논리 위치는 서버 좌표로 즉시 동기화 (순간이동)
	_logicalPosition = movePacket._position;
	_currentVelocity = movePacket._velocity; // [추가] 서버의 물리 속도 동기화
	_state = movePacket._state; // [추가] 서버 상태 동기화
	_grabbedById = movePacket._grabbed_by_id; // [추가]
	_grabSlot = movePacket._grab_slot;         // [추가]
	set_hp(movePacket._hp);                   // [추가] 실시간 HP 동기화
	set_mp(movePacket._mp);                   // [추가] 실시간 MP 동기화

	// 3. [핵심] 화면이 튀지 않게 오프셋 재계산
	// (이전 시각적 위치 - 새로운 논리 위치)를 오프셋으로 설정하여 화면상 위치를 유지함
	_visualOffset = currentVisualPos - _logicalPosition;

	// 만약 오차가 너무 크면(예: 5m) 보간하지 않고 즉시 스냅 (텔레포트 대응)
	if (common::LengthSq(_visualOffset) > 5.0f * 5.0f) {
		_visualOffset = { 0, 0, 0 };
	}
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
		_hpBar_ui->set_uv_scale(ratio, 1.0f);
	}
}

void MainPlayerScript::update_mp_bar(float deltaTime)
{
	if (_mpBar_ui)
	{
		// 10.0f는 속도
		float lerp = std::min(1.0f, deltaTime * 10.0f);

		// 현재 마나 수치로 부드럽게 이동
		_displayMp += (static_cast<float>(_mp) - _displayMp) * lerp;

		// 비율 계산 (0.0 ~ 1.0)
		float ratio = _displayMp / static_cast<float>(_maxMp);

		// UI 크기 및 텍스처 좌표(UV) 조절
		_mpBar_ui->set_size_x(_mpBar_maxWidth * ratio);
		_mpBar_ui->set_uv_scale(ratio, 1.0f);
	}
}

void MainPlayerScript::update_quest_ui()
{
    // 현재 진행 중인 퀘스트를 찾습니다.
    const auto active_quest = NetworkManager::instance()->get_quest(1);

    bool is_visible = false;
    int current = 0;
    int target = 0;
    int quest_id = 0;
    
    if (active_quest) {
        is_visible = true;
        current = active_quest->_current_count;
        target = active_quest->_target_count;
        quest_id = active_quest->_quest_id;
    }
    else {
        // 퀘스트 안할 때
        is_visible = false;
        /*current = 0;
        target = 15;
        quest_id = 1;*/
    }
    
    // 배너 및 타이틀 켜기/끄기
    UIManager::instance()->set_visible(UILayer::BACKGROUND, "QuestBanner_UI", is_visible);
    UIManager::instance()->set_visible(UILayer::MIDDLE, "QuestTitle_UI", is_visible);
    
    // 퀘스트 ID에 맞춰 타이틀 이미지 변경
    if (is_visible) {
        auto title_ui = UIManager::instance()->ui_component(UILayer::MIDDLE, "QuestTitle_UI");
        if (title_ui) {
            if (quest_id == 2) {
                title_ui->set_texture("Resource/UI/Quest_Title_2.png");
            } else {
                title_ui->set_texture("Resource/UI/Quest_Title_1.png"); // 기본: 1번
            }
        }
    }

    for (int i = 0; i < 5; ++i) {
        UIManager::instance()->set_visible(UILayer::MIDDLE, "QuestNumber_" + std::to_string(i), is_visible);
	}

	if (is_visible) {
		float uv_scale_x = 1.0f / 11.0f; // 0~9, /

		int cur_tens = (current / 10) % 10;
		int cur_ones = current % 10;
		int max_tens = (target / 10) % 10;
		int max_ones = target % 10;

		auto n0 = UIManager::instance()->ui_component(UILayer::MIDDLE, "QuestNumber_0");
		auto n1 = UIManager::instance()->ui_component(UILayer::MIDDLE, "QuestNumber_1");
		auto n2 = UIManager::instance()->ui_component(UILayer::MIDDLE, "QuestNumber_2");
		auto n3 = UIManager::instance()->ui_component(UILayer::MIDDLE, "QuestNumber_3");
		auto n4 = UIManager::instance()->ui_component(UILayer::MIDDLE, "QuestNumber_4");
        
        if (n0) n0->set_uv_offset(cur_tens * uv_scale_x, 0.0f);
        if (n1) n1->set_uv_offset(cur_ones * uv_scale_x, 0.0f);
        if (n2) n2->set_uv_offset(10 * uv_scale_x, 0.0f); // '/'
        if (n3) n3->set_uv_offset(max_tens * uv_scale_x, 0.0f);
        if (n4) n4->set_uv_offset(max_ones * uv_scale_x, 0.0f);
    }
}


void MainPlayerScript::handle_state(float deltaTime)
{
	auto anim_comp = game_object()->get_component<AnimationComponent>();
	if (!anim_comp) return;

	// 사망 상태 로직 추가
	if (0 >= hp())
	{
		_state = common::packet::EntityState::DEAD;
		if (_currentWeapon) _currentWeapon->set_attack_active(false);
		init_skill_variables();
		anim_comp->play("die", false);
		return;
	}

	// [추가] 잡힌 상태 애니메이션 처리
	if (_state == common::packet::EntityState::GRABBED) {
		if (_currentWeapon) _currentWeapon->set_attack_active(false);
		init_skill_variables();
		anim_comp->play("die", false); // 잡힌 동안 고통받는 모습 (죽는 모션 재활용 혹은 피격 모션)
		return;
	}

	if (_isAttacking) {
		if (_isSkilling)
		{
			_state = common::packet::EntityState::ACTION;
			_actionId = common::packet::ActionID::Common::SKILL1;
			
			// 스킬 비주얼 업데이트 함수 호출 (파티클 생성 및 애니메이션 제어)
			update_skill_visuals(deltaTime);

		}
		else
		{
			_state = common::packet::EntityState::ACTION;
			anim_comp->play("attack", false, _attackAnimationSpeed);
		}

		// 실제 공격 패킷 전송 함수 부분
		process_attack_and_packet();

		// 공격 종료 체크
		if (_isSkilling) // 스킬할 때는 스킬end 애니메이션이 끝나야 함
		{
			if (_isSkillEndAnimationStart && anim_comp->is_anim_finished())
			{
				_isSkillEnd = true; // 스킬 종료 플래그 설정
				_isAttacking = false;
				init_skill_variables(); // 스킬 관련 변수 초기화
				_packetSent = false;
				_actionId = 0;
				_state = common::packet::EntityState::IDLE;

				if (_currentWeapon) _currentWeapon->set_attack_active(false);

				// 따라가는 것만 멈추도록 수정
				game_object()->get_component<SocketComponenet>()->set_isFollowAnimation(false);

				anim_comp->play("idle");
				send_network_sync(0.0f);
			}
		}
		else if (anim_comp->is_anim_finished()) 
		{
			_isAttacking = false;
			init_skill_variables(); // 스킬 관련 변수 초기화
			_packetSent = false;
			_actionId = 0;
			_state = common::packet::EntityState::IDLE;

			if (_currentWeapon) _currentWeapon->set_attack_active(false);

			// 따라가는 것만 멈추도록 수정
			game_object()->get_component<SocketComponenet>()->set_isFollowAnimation(false);

			anim_comp->play("idle");
			send_network_sync(0.0f);
		}
	}
	else {
		if (_speed >= common::move_speed::player_run_speed && common::Length(_currentMoveDir) > 0.01f) {
			_state = common::packet::EntityState::RUN;
			anim_comp->play("run", true, (_speed / common::anim_speed::player_run_animation));
		}
		else if (common::Length(_currentMoveDir) > 0.01f) {
			_state = common::packet::EntityState::MOVE;
			anim_comp->play("walk", true, (_speed / common::anim_speed::player_walk_animation));
		}
		else {
			_state = common::packet::EntityState::IDLE;
			anim_comp->play("idle");
		}
	}
}

void MainPlayerScript::handle_input(float deltaTime)
{
	if (InputManager::instance()->IsKeyDown(VK_F8))
	{
		NetworkManager::instance()->SendDebugCommandPacket(common::packet::DebugCommandType::PHYSICS_SNAPSHOT);
	}
	if (InputManager::instance()->IsKeyDown(VK_F10))
	{
		// 임시 컷씬 종료 패킷 전송 (클라이언트 컷씬 연출이 끝나면 보내는 패킷을 디버깅용으로 F9에 연결)
		NetworkManager::instance()->SendCutsceneDonePacket();
	}

	// DW추가 : 사망 상태 로직 추가
	if (0 >= hp() || _state == common::packet::EntityState::GRABBED)
	{
		// 사망 또는 잡힌 상태에서는 입력 무시
		_currentMoveDir = { 0, 0, 0 }; // 이동 입력 초기화
		return;
	}
	auto targeting = game_object()->get_component<TargetingComponent>();
	bool is_lock_on = targeting && targeting->is_locked_on();

	common::Vec3 move_direction{};
	bool is_moving_input = false;

	// [방어 코드] 카메라가 로딩되지 않았거나 nullptr인 경우 입력 처리를 스킵하여 크래시 방지
	if (!_camera)
	{
		CERROR("카메라가 널 포인터임");
		return;
	}
			

	XMFLOAT3 camForward = _camera->transform()->forward();
	XMFLOAT3 camFwdV = Vector3::Normalize({ camForward.x, 0.0f, camForward.z });
	XMFLOAT3 camRightV = Vector3::Normalize(Vector3::CrossProduct({ 0, 1.f, 0 }, camFwdV));

	bool is_foward = false;

	bool is_gathering = _isSkilling && !_isSwordGathered;

	if (false == _isAttacking || is_gathering) // 공격 중이 아닐 때만 이동 입력 처리 (스킬 모으는 중이면 방향 입력만 허용)
	{
		// 이동 입력 처리
		if (InputManager::instance()->IsKeyPress('W')) {
			move_direction = Vector3::Add(move_direction, camFwdV);
			is_moving_input = true;
			is_foward = true;
		}
		if (InputManager::instance()->IsKeyPress('A')) {
			move_direction = Vector3::Add(move_direction,
				Vector3::ScalarProduct(camRightV, -1.0f)); is_moving_input = true;

			is_foward = false;
		}
		if (InputManager::instance()->IsKeyPress('S')) {
			move_direction = Vector3::Add(move_direction,
				Vector3::ScalarProduct(camFwdV, -1.0f)); is_moving_input = true;

			is_foward = false;
		}
		if (InputManager::instance()->IsKeyPress('D')) {
			move_direction = Vector3::Add(move_direction, camRightV);
			is_moving_input = true;

			is_foward = false;
		}

		if (InputManager::instance()->IsKeyPress(VK_LSHIFT)) {
			_speed = common::move_speed::player_run_speed ; // 달리기 속도 -> 서버와 동일하게 해주어야 함
		}
		else
		{
			_speed = common::move_speed::player_walk_speed; // 걷기 속도 -> 서버와 동일하게 해주어야 함
		}

		if (is_gathering) {
			_speed = 0.0f; // 스킬 모으기 중에는 이동 불가, 방향 회전만 가능
		}
	}

	if (is_lock_on) {
		auto target = ObjectManager::instance()->find_npc(targeting->current_target_id());
		if (target) {
			// [락온 모드] 적을 정면으로 고정
			common::Vec3 targetPos = target->transform()->position();
			common::Vec3 playerPos = transform()->position();
			common::Vec3 dirToEnemy = targetPos - playerPos;

			float targetYaw = XMConvertToDegrees(atan2f(dirToEnemy.x, dirToEnemy.z)); //- 180.f;

			XMVECTOR qLogical = XMQuaternionRotationRollPitchYaw(0, XMConvertToRadians(_currentyaw), 0);
			XMStoreFloat4(&_logicalRotation, qLogical);

			float yawDiff = targetYaw - _currentyaw;
			while (yawDiff > 180.f) yawDiff -= 360.f;
			while (yawDiff < -180.f) yawDiff += 360.f;

			float rotSpeed = is_gathering ? 2.0f : 15.0f; // 스킬 모으기 중에는 느리게 회전
			_currentyaw += yawDiff * std::min(1.0f, deltaTime * rotSpeed); // 캐릭터 회전은 카메라보다 빠르게
			transform()->set_local_rotation(0.0f, _currentyaw + 180.0f, 0.0f);

			// 이동 벡터는 카메라 기준 유지 (이동 시 옆걸음/뒷걸음질이 됨)
			_currentMoveDir = is_moving_input ? common::Normalize(move_direction) : common::Vec3{ 0, 0, 0 };
		}
	}
	else if (is_moving_input) {
		// 방향벡터 갱신
		_currentMoveDir = common::Normalize(move_direction);

		float targetYaw = XMConvertToDegrees(atan2f(move_direction.x, move_direction.z)); // -180.f;
		float currentYaw = _currentyaw;
		// Yaw 보간 (360도 회전 고려)
		float yawDiff = targetYaw - currentYaw;

		while (yawDiff > 180.f) yawDiff -= 360.f;
		while (yawDiff < -180.f) yawDiff += 360.f;

		float rotSpeed = is_gathering ? 2.0f : 10.0f; // 회전 속도 조절
		float lerpFactor = std::min(1.0f, deltaTime * rotSpeed); 
		_currentyaw += yawDiff * lerpFactor;

		if (_currentyaw > 360.f) _currentyaw -= 360.f;
		if (_currentyaw < -360.f) _currentyaw += 360.f;

		XMVECTOR qLogical = XMQuaternionRotationRollPitchYaw(0, XMConvertToRadians(_currentyaw), 0);
		XMStoreFloat4(&_logicalRotation, qLogical);

		transform()->set_local_rotation(0.0f, _currentyaw + 180.0f, 0.0f);
	}
	else {
		_currentMoveDir = { 0, 0, 0 };
	}


	// 대쉬 입력 (스페이스바)
	if (!_isAttacking && InputManager::instance()->IsKeyDown(VK_SPACE)) {
		common::Quat dashRotation = _logicalRotation;
		if (common::LengthSq(_currentMoveDir) > 0.001f) {
			// 이동 입력이 있으면 해당 방향으로 대쉬
			float yawRad = atan2f(_currentMoveDir.x, _currentMoveDir.z);
			XMVECTOR qDash = XMQuaternionRotationRollPitchYaw(0, yawRad, 0);
			XMStoreFloat4(&dashRotation, qDash);
		} else {
			// 입력이 없으면 뒤로 대쉬 (소울라이크의 백스텝)
			float yawRad = XMConvertToRadians(_currentyaw + 180.0f);
			XMVECTOR qDash = XMQuaternionRotationRollPitchYaw(0, yawRad, 0);
			XMStoreFloat4(&dashRotation, qDash);
		}
		NetworkManager::instance()->SendActionPacket(common::packet::ActionID::Common::DASH, -1, _logicalPosition, dashRotation);
	}

	// 점프 입력 (F키)
	if (!_isAttacking && InputManager::instance()->IsKeyDown('F')) {
		NetworkManager::instance()->SendActionPacket(common::packet::ActionID::Common::JUMP, -1, _logicalPosition, _logicalRotation);
	}

	// 상호작용 입력 (E키) -> 레버 스크립트에서 하도록 전환
	//if (!_isAttacking && InputManager::instance()->IsKeyDown('E')) {
	//	NetworkManager::instance()->SendActionPacket(common::packet::ActionID::Common::INTERACT, -1, _logicalPosition, _logicalRotation);
	//}

	// 공격 입력 (공격 중이 아닐 때만 새 공격 시작 가능)
	if (!_isAttacking && InputManager::instance()->IsKeyDown(VK_LBUTTON)) {
		_isAttacking = true;
		_packetSent = false;
		_actionId = common::packet::ActionID::Common::Attack;
		// 공격 시작 시점에 즉시 상태를 ATTACK으로 변경하도록 update_state에서 처리됨
	}

	if (!_isAttacking && InputManager::instance()->IsKeyDown(VK_RBUTTON)) {

		if (_mp >= 80) // 스킬 사용 시 마나 80 소모 (서버에서 차감하므로 여기서는 체크만)
		{
			CLOG("[Skill] 스킬 사용! 예상 마나 잔량: " << _mp - 80);
			set_mp(_mp - 80); // [추가] 서버 응답 전 클라이언트 선 차감 (스팸 방지)
			init_skill_variables(); // 스킬 관련 변수 초기화
			_isAttacking = true;
			_isSkilling = true;
			_packetSent = false;
			_actionId = common::packet::ActionID::Common::SKILL1;
			// 공격 시작 시점에 즉시 상태를 ATTACK으로 변경하도록 update_state에서 처리됨

			// 두 번째 스킬 사용 시 연출을 처음부터 다시 하기 위한 초기화
			_isSwordGathered = false;
			_skillGatherTimer = 0.0f;
			game_object()->get_component<SocketComponenet>()->set_isFollowAnimation(true); // 손에 다시 쥐어줌
			if (_particleEffectObject) {
				// 파티클 죽는 연출 끄기
				_particleEffectObject->get_component<ParticleSystemComponent>()->set_particle_dying(false);
			}
		}
	}

	if (InputManager::instance()->IsKeyDown(VK_MBUTTON) || InputManager::instance()->IsKeyDown(VK_TAB)) {
		if (auto targeting_comp = game_object()->get_component<TargetingComponent>()) {
			targeting_comp->toggle_lock_on();
		}
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
	// 0. 잡기 상태일 때 본 부착 처리 (물리 무시)
	if (_grabbedById != -1) {
		auto bossObj = ObjectManager::instance()->find_npc(_grabbedById);
		if (bossObj) {
			auto bossAnim = bossObj->get_component<AnimationComponent>();
			auto bossRender = bossObj->get_component<RenderComponent>();
			if (bossAnim && bossRender) {
				auto bossMesh = std::dynamic_pointer_cast<ReadGLTFMesh>(bossRender->mesh());
				if (bossMesh) {
					// [수정] 모델의 실제 본 이름: hand_L, hand_R
					std::string boneName = (_grabSlot == 0) ? "hand_L" : "hand_R";

					// 1. 보스의 해당 본 월드 행렬 가져오기
					XMFLOAT4X4 boneSocketTransform = bossMesh->get_socket_transform(boneName);
					XMFLOAT4X4 bossWorldMatrix = bossObj->transform()->world_matrix();

					// 2. 최종 월드 행렬 계산
					XMMATRIX matBone = XMLoadFloat4x4(&boneSocketTransform);
					XMMATRIX matBoss = XMLoadFloat4x4(&bossWorldMatrix);
					XMMATRIX matFinal = matBone * matBoss;

					// [핵심] 보스의 스케일(5배) 성분 제거 및 위치/회전만 추출
					XMVECTOR scale, rot, pos;
					XMMatrixDecompose(&scale, &rot, &pos, matFinal);

					// 플레이어 원래 스케일(1.0) 기반으로 행렬 재조합
					XMMATRIX matPlayer = XMMatrixRotationQuaternion(rot) * XMMatrixTranslationFromVector(pos);

					// 3. 플레이어 트랜스폼에 적용
					XMFLOAT4X4 finalWorld;
					XMStoreFloat4x4(&finalWorld, matPlayer);
					transform()->set_world_matrix(finalWorld);
					
					// 논리 위치도 동기화 (팅겨나갈 때 시작점이 됨)
					_logicalPosition = transform()->local_position();
					_visualOffset = { 0, 0, 0 };
					return; // 물리 업데이트 건너뜀
				}
			}
		}
	}

	// 1. TerrainLoader 정적 함수를 통해 현재 위치의 지형 높이 가져오기
	// 지형 타일이 여러 개여도 알아서 내 발밑의 높이를 찾아줍니다.
	float groundHeight = TerrainLoader::get_height_anywhere(_logicalPosition.x, _logicalPosition.z);

	//// 2. 접지 체크 및 수직 속도(중력) 계산
	//if (_logicalPosition.y > groundHeight + 0.05f) {
	//	_isGrounded = false;
	//	_verticalVelocity += -9.81f * deltaTime; // 서버와 동일한 중력 가속도
	//}
	//else {
	//	// 지면에 닿아있는 상태
	//	if (!_isGrounded) {
	//		_isGrounded = true;
	//		_verticalVelocity = 0.0f;
	//		_logicalPosition.y = groundHeight; // 지면에 착지 스냅
	//	}
	//}

	// [핵심 수정] 서버와 동일한 Lerp 가감속 공식 적용
	common::Vec3 targetVelocity = _currentMoveDir * _speed;
	float moveDeceleration = 15.0f; // 서버와 무조건 같은 값이어야 함
	float t = std::min(deltaTime * moveDeceleration, 1.0f);


	// 현재 속도를 목표 속도로 서서히 변화시킴
	_currentVelocity = _currentVelocity + (targetVelocity - _currentVelocity) * t;

	// 보간된 _currentVelocity를 사용해서 이동
	_logicalPosition += _currentVelocity * deltaTime;
	_logicalPosition.y += _verticalVelocity * deltaTime;

	//// 2. 논리적 위치 예측 (Input + Knockback + Gravity)
	//common::Vec3 moveVel = _currentMoveDir * _speed;
	//_logicalPosition += (moveVel) * deltaTime;
	//_logicalPosition.y += _verticalVelocity * deltaTime;

	// 땅 파고듦 방지 (중요!)
	if (_isGrounded && _logicalPosition.y < groundHeight) {
		//_logicalPosition.y = groundHeight;
	}

	// 3. 시각적 오프셋 감쇄 (부드럽게 0으로 수렴)
	// 0.1초(deltaTime * 10) 주기로 오차를 없앰
	float lerpFactor = std::min(1.0f, deltaTime * 15.0f);
	_visualOffset = _visualOffset * (1.0f - lerpFactor);

	// 4. 최종 Transform 적용 (논리 위치 + 보정 오프셋)
	// 이렇게 해야 렌더링은 부드럽고, 서버에 보내는 좌표는 정확해집니다.
	if (transform())
	{
		common::Vec3 visualPosition = _logicalPosition + _visualOffset + common::Vec3{ 0, 0.0f, 0 };
		transform()->set_local_position(visualPosition);
	}

	// [카메라 다이나믹 줌 자동 원상복구]
	// 스킬 연출 도중(기가 모이는 중)이 아니라면, 카메라 줌을 서서히 0.0f로 복구합니다.
	bool is_gathering = _isSkilling && !_isSwordGathered;
	if (!is_gathering) {
		auto mainCam = CameraComponent::get_main();
		if (mainCam && mainCam->game_object()) {
			auto freeCamScript = mainCam->game_object()->get_component<FreeCameraScript>();
			if (freeCamScript) {
				float currentZoom = freeCamScript->get_dynamic_zoom_offset();
				if (currentZoom != 0.0f) {
					// 거리가 멀면 더 빨리, 가까우면 부드럽게 복구
					float speed = 20.0f; // 타격 시 줌인 속도
					float newZoom = currentZoom;
					if (currentZoom > 0.0f) {
						newZoom = currentZoom - (deltaTime * speed);
						if (newZoom < 0.0f) newZoom = 0.0f;
					} else {
						newZoom = currentZoom + (deltaTime * speed);
						if (newZoom > 0.0f) newZoom = 0.0f;
					}
					freeCamScript->set_dynamic_zoom_offset(newZoom);
				}
			}
		}
	}
}
void MainPlayerScript::send_network_sync(float deltaTime)
{

	_sendTimer += deltaTime;
	if (_sendTimer >= SENDINTERVAL) {
		_sendTimer = 0.f;

		NetworkManager::instance()->SendMovePacket(
			_logicalPosition,
			_currentMoveDir,
			_logicalRotation,
			_state,
			_actionId, static_cast<uint32_t>(GetTickCount64())
		);
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

void MainPlayerScript::update_skill_visuals(float deltaTime)
{
	if (_isSkillEndAnimationStart) // 스킬이 끝나는 애니메이션이 시작된 이후에는 더 이상 스킬 연출을 업데이트하지 않음
	{
		return;
	}

	auto anim_comp = game_object()->get_component<AnimationComponent>();
	if (!anim_comp) return;

	if(!_isSkillAnimationStarted)
	{
		anim_comp->play("skill", false, _skillAnimationspeed);
		_isSkillAnimationStarted = true;
	}

	float current_anim_time = anim_comp->get_anim_time(); // 이거 스킬 재생 후 가져와야 함

	// 애니메이션이 0.25초(6프레임)를 넘어가면 파티클 연출 시작
	if (current_anim_time >= _skillParticleSpawnTime)
	{
		_particleEffectObject->set_enabled(true);
		auto psComp = _particleEffectObject->get_component<ParticleSystemComponent>();

		if (!_isSwordGathered)
		{
			anim_comp->set_anim_speed(0.0f);
			_skillGatherTimer += deltaTime;

			float progress = std::clamp(_skillGatherTimer / _particleGatherDuration, 0.0f, 1.0f);

			if (psComp) 
			{
				psComp->set_compute_data(_SkillObject->transform()->world_matrix(), transform()->local_position(), progress);
			}

			// [카메라 다이나믹 줌 아웃]
			auto mainCam = CameraComponent::get_main();
			if (mainCam && mainCam->game_object()) {
				auto freeCamScript = mainCam->game_object()->get_component<FreeCameraScript>();
				if (freeCamScript) {
					// 줌 아웃 (최대 4.0f 까지 멀어짐)
					float targetZoom = progress * 4.0f;
					freeCamScript->set_dynamic_zoom_offset(targetZoom);
				}
			}

			if (progress >= 1.0f)
			{
				_isSwordGathered = true;
				anim_comp->set_anim_speed(_skillSwingAnimationSpeed); // 스킬이 모인후에 내려찍는 애니메이션 속도
			}
		}
		else
		{
			if (psComp) {
				psComp->set_compute_data(_SkillObject->transform()->world_matrix(), transform()->local_position(), 1.0f);
			}
			// 카메라 줌 복구는 이제 late_update에서 자동으로 처리됩니다.
		}
	}
}

void MainPlayerScript::process_attack_and_packet()
{
	auto anim_comp = game_object()->get_component<AnimationComponent>();
	if (!anim_comp) return;
	float anim_progress = anim_comp->get_anim_time();
	float duration = anim_comp->get_anim_duration();

	int64_t targetId = -1;
	if (auto targeting_comp = game_object()->get_component<TargetingComponent>()) {
		targetId = targeting_comp->current_target_id();
	}

	if (_isSkilling)
	{
		if (!_isSkillEndAnimationStart && anim_comp->is_anim_finished()) // 마지막은 아니고 스킬 애니메이션이 끝났을 때
		{
			_isSkillEndAnimationStart = true;

			game_object()->get_component<SocketComponenet>()->set_isFollowAnimation(false);

			anim_comp->play("skill_end", false, _skillEndingAnimationSpeed);

			auto psComp = _particleEffectObject->get_component<ParticleSystemComponent>();
			if (psComp) psComp->set_particle_dying(true);

			if (_currentWeapon) _currentWeapon->set_attack_active(true);

			if (!_packetSent) 
			{
				NetworkManager::instance()->SendActionPacket(_actionId, targetId, _logicalPosition, _logicalRotation);
				_packetSent = true;
			}
		}
	}
	else
	{
		if (_currentWeapon)
		{
			if (anim_progress >= (duration * 0.3f) && anim_progress <= (duration * 0.6f)) 
			{
				_currentWeapon->set_attack_active(true);
			}
			else {
				_currentWeapon->set_attack_active(false);
			}
		}

		if (!_packetSent && anim_progress >= (duration * 0.3f)) 
		{
			NetworkManager::instance()->SendActionPacket(_actionId, targetId, _logicalPosition, _logicalRotation);
			_packetSent = true;
		}
	}
}

void MainPlayerScript::init_skill_variables()
{
	_isSkilling = false;
	_isSkillAnimationStarted = false;
	_isSkillEndAnimationStart = false;
	_isSkillEnd = false;
	_nowSkillTime = 0.0f;
	_skillGatherTimer = 0.0f;
	_isSwordGathered = false;
	if (game_object()) 
	{
		// 검 따라가기 멈추기
		if (auto socket = game_object()->get_component<SocketComponenet>())
			socket->set_isFollowAnimation(false);
	}

	if (_particleEffectObject)
	{
		// 파티클 죽는 연출 시작
		if (auto psComp = _particleEffectObject->get_component<ParticleSystemComponent>())
			psComp->set_particle_dying(true);
	}
	/*_SkillObject->get_component<RenderComponent>()->set_enabled(false);
	game_object()->get_component<SocketComponenet>()->set_isFollowAnimation(true);*/

}

void MainPlayerScript::reset_state()
{
	_state = common::packet::EntityState::IDLE;
	_actionId = 0;
	_grabbedById = -1;
	_grabSlot = -1;
	_isAttacking = false;
	_isSkilling = false;
	_packetSent = false;
	_nowSkillTime = 0.0f;
	_currentVelocity = { 0, 0, 0 };
	_verticalVelocity = 0.0f;
	_visualOffset = { 0, 0, 0 };
	
	// 물리 속성 초기화
	auto cc = game_object()->get_component<PhysicsCharacterControllerComponent>();
	if (cc) {
		cc->set_velocity({ 0, 0, 0 });
	}

	// 애니메이션 강제 초기화
	auto anim = game_object()->get_component<AnimationComponent>();
	if (anim) {
		anim->play("idle", true);
	}

	// 카메라 줌 오프셋 초기화
	auto mainCam = CameraComponent::get_main();
	if (mainCam && mainCam->game_object()) {
		auto freeCamScript = mainCam->game_object()->get_component<FreeCameraScript>();
		if (freeCamScript) freeCamScript->set_dynamic_zoom_offset(0.0f);
	}
}
