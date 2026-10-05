#include "stdafx.h"
#include "OtherPlayerScript.h"

#include "AnimationComponent.h"
#include "BehaviorTree.h"
#include "gameobject.h"
#include "ObjectManager.h"
#include "ReadGLTFMesh.h"
#include "NetworkManager.h"
#include "SoundManager.h" // [사운드]
#include "ResourceManager.h"
#include "Renderer.h"
#include "SocketComponenet.h"
#include "GatherParticleComponent.h"
#include "ParticleRenderComponent.h"
#include "UIManager.h"
#include "ServerClock.h"
#include "DebugDrawManager.h"

void OtherPlayerScript::set_hp(int hp)
{
	int prevHp = _hp;
	_hp = hp;
	if (_hp < prevHp) {
		if (transform()) {
			SoundManager::instance()->play_3d("Other_PlayerDamage", transform()->get_world_position(), SoundType::SFX, 1.0f, false);
		}
	}
}

void OtherPlayerScript::on_server_move(const common::packet::SC_PACKET_MOVE& packet)
{
	// 서버 시각 기준 보간 (NetMotionSync_Design_KR.md 5.2): 표본을 덮어쓰지 않고 모두 쌓아 렌더 시각에 꺼내 씀
	SnapshotBuffer::Sample sample;
	sample.time = packet._server_time;
	sample.pos = packet._position;
	sample.vel = packet._velocity;
	sample.rot = to_visual_rotation(packet._rotation);
	_positionBuffer.push(sample);

	_pendingStates.push_back({ packet._server_time, packet._state, packet._action_id,
		packet._grabbed_by_id, packet._grab_slot, packet._hp, packet._mp });
}

void OtherPlayerScript::reset_transform(const XMFLOAT3& position, const XMFLOAT4& server_rotation)
{
	_positionBuffer.clear();
	_pendingStates.clear();

	// 지금 렌더 시각에 표본을 둔다. 이후 도착하는 이동 패킷(서버 시각이 이보다 늦음)이 버려지지 않는다
	SnapshotBuffer::Sample sample;
	sample.time = ServerClock::instance()->is_synced() ? render_time() : 0.0;
	sample.pos = position;
	sample.rot = to_visual_rotation(server_rotation);
	_positionBuffer.push(sample);

	if (transform())
	{
		transform()->set_local_position(sample.pos);
		transform()->set_local_rotation(sample.rot);
	}
}

void OtherPlayerScript::apply_state(const PendingState& pending)
{
	const common::packet::EntityState prevState = _state;
	_state = pending.state;
	_action_id = pending.action_id;

    // 상태 변화(IDLE 등 -> ACTION) 감지 후 공격 사운드 재생
    if (prevState != common::packet::EntityState::ACTION && _state == common::packet::EntityState::ACTION) {
        if (_action_id == common::packet::ActionID::Common::Attack) {
            if (transform()) {
                // 3D 사운드 재생 (다른 플레이어 위치 기반)
                SoundManager::instance()->play_3d("Other_SwordSwing", transform()->get_world_position());
            }
        }
    }

	_grabbedById = pending.grabbed_by_id;
	_grabSlot = pending.grab_slot;
	_hp = pending.hp;
	_mp = pending.mp;
}

double OtherPlayerScript::render_time() const
{
	auto clock = ServerClock::instance();
	// 동기화 전에는 서버 시각을 모르므로 최신 표본을 그대로 보여줌
	if (!clock->is_synced()) return _positionBuffer.empty() ? 0.0 : _positionBuffer.latest().time;
	return clock->arrival_now_ms() - ServerClock::kPlayerInterpDelayMs;
}

XMFLOAT4 OtherPlayerScript::to_visual_rotation(const XMFLOAT4& server_rotation)
{
	// 서버와 클라이언트 간 모델 회전 차이 보정 (TransformComponent::rotate(0, 180, 0)과 같은 순서)
	XMFLOAT4 rot;
	const XMVECTOR q180 = XMQuaternionRotationRollPitchYaw(0, XM_PI, 0);
	XMStoreFloat4(&rot, XMQuaternionNormalize(XMQuaternionMultiply(q180, XMLoadFloat4(&server_rotation))));
	return rot;
}

void OtherPlayerScript::reset_state()
{
	_state = common::packet::EntityState::IDLE;
	_action_id = 0;
	_grabbedById = -1;
	_grabSlot = -1;

	// 애니메이션 강제 초기화
	auto anim = game_object()->get_component<AnimationComponent>();
	if (anim) {
		anim->play("idle", true);
	}
}

void OtherPlayerScript::init_skill_variables()
{
    _isSkilling = false;
    _isSkillAnimationStarted = false;
    _isSkillEndAnimationStart = false;
    _skillGatherTimer = 0.0f;
    _isSwordGathered = false;

    if (game_object()) {
        if (auto socket = game_object()->get_component<SocketComponent>())
            socket->set_isFollowAnimation(false); // 검 따라가기 해제
    }

    if (_particleEffectObject) {
        if (auto psComp = _particleEffectObject->get_component<GatherParticleComponent>())
            psComp->set_particle_dying(true); // 흩어짐 연출 시작
    }
}


void OtherPlayerScript::update(float deltaTime)
{
	if (_partySlotIndex != -1) {
		auto slot = UIManager::instance()->get_party_slot(_partySlotIndex);
		if (slot && slot->hp_bar) {
			// HP 업데이트 로직
			float ratio = (float)_hp / (float)_maxHp;
			if (ratio < 0.0f) ratio = 0.0f;
			if (ratio > 1.0f) ratio = 1.0f;
			slot->hp_bar->set_size_x(slot->max_width * ratio);
			slot->hp_bar->set_uv_scale(ratio, 1.0f);

			// MP 업데이트 로직
			float mp_ratio = (float)_mp / 100.0f;
			slot->mp_bar->set_size_x(slot->max_width * mp_ratio);
			slot->mp_bar->set_uv_scale(mp_ratio, 1.0f);
		}
	}

    // [추가] _playerId가 -1 이면 Scene Editor에서 배치한 더미 비주얼이므로 삭제 (혹은 비활성화)
    if (_playerId == -1) {
        ObjectManager::instance()->remove_game_object(game_object());
        return;
    }

    // 상태·액션·잡기·HP·MP는 그 스냅샷의 서버 시각이 렌더 시각에 닿았을 때 적용 (모션 전환과 위치를 같은 시점에 맞춤)
    const double renderTime = render_time();
    while (!_pendingStates.empty() && _pendingStates.front().time <= renderTime)
    {
        apply_state(_pendingStates.front());
        _pendingStates.pop_front();
    }

    // 0. 잡기 상태일 때 본 부착 처리 (다른 플레이어)
    if (_grabbedById != -1) {
        auto bossObj = ObjectManager::instance()->find_npc(_grabbedById);
        if (bossObj) {
            auto bossAnim = bossObj->get_component<AnimationComponent>();
            auto bossRender = bossObj->get_component<RenderComponent>();
            if (bossAnim && bossRender) {
                auto bossMesh = std::dynamic_pointer_cast<ReadGLTFMesh>(bossRender->mesh());
                if (bossMesh) {
                    // [수정] 대소문자 구분: hand_L, hand_R
                    std::string boneName = (_grabSlot == 0) ? "hand_L" : "hand_R";
                    XMFLOAT4X4 boneSocketTransform = bossMesh->get_socket_transform(boneName);
                    XMFLOAT4X4 bossWorldMatrix = bossObj->transform()->world_matrix();

                    XMMATRIX matBone = XMLoadFloat4x4(&boneSocketTransform);
                    XMMATRIX matBoss = XMLoadFloat4x4(&bossWorldMatrix);
                    XMMATRIX matFinal = matBone * matBoss;

                    // [핵심] 보스의 스케일 제거 및 위치/회전만 추출
                    XMVECTOR scale, rot, pos;
                    XMMatrixDecompose(&scale, &rot, &pos, matFinal);

                    // 플레이어 스케일 1.0 유지
                    XMMATRIX matPlayer = XMMatrixRotationQuaternion(rot) * XMMatrixTranslationFromVector(pos);

                    XMFLOAT4X4 finalWorld;
                    XMStoreFloat4x4(&finalWorld, matPlayer);
                    transform()->set_world_matrix(finalWorld);
                    return;
                }
            }
        }
    }

    // [추가] 잡기 해제 시 상태 보정
    if (_grabbedById == -1 && _state == common::packet::EntityState::GRABBED) {
        _state = common::packet::EntityState::IDLE;
    }

    // 위치·회전: 렌더 시각(도착 기준 서버 시각 - 30ms)으로 보간
    if (transform() && !_positionBuffer.empty())
    {
        const SnapshotBuffer::Result interp = _positionBuffer.sample(renderTime);
        transform()->set_local_position(interp.pos);
        transform()->set_local_rotation(interp.rot);
        ServerClock::instance()->count_interp_mode(ServerClock::InterpTarget::Player, static_cast<int>(interp.mode));

        // [디버그] 서버가 마지막으로 보낸 위치 (F6 창에서 켬)
        if (ServerClock::instance()->show_server_ghost())
        {
            const auto& latest = _positionBuffer.latest();
            DebugDrawManager::instance()->AddDebugShape(common::packet::DebugShapeType::BOX,
                { latest.pos.x, latest.pos.y + 0.9f, latest.pos.z }, latest.rot, { 0.3f, 0.9f, 0.3f }, deltaTime * 1.5f);
        }
    }

	// 파티클 효과가 활성화된 상태에서 애니메이션이 끝났는지 체크하여, 끝났다면 파티클 효과도 비활성화
    if (_particleEffectObject && _particleEffectObject->is_enable())
    {
        auto psComp = _particleEffectObject->get_component<GatherParticleComponent>();
        if (psComp && psComp->is_death_timer_end())
        {
            _particleEffectObject->set_enabled(false);
        }
    }


	auto anim_comp = game_object()->get_component<AnimationComponent>();
	if (!anim_comp)
	{
        return;
	}
    switch (_state)
    {
	case common::packet::EntityState::ACTION:
        if (_action_id == common::packet::ActionID::Common::Attack)
        {
            if (_isSkillAnimationStarted) init_skill_variables(); // 혹시 스킬 쓰다 평타로 넘어왔을 때 초기화
            anim_comp->play("attack", false, _attackAnimationSpeed);
        }
        else if (_action_id == common::packet::ActionID::Common::SKILL1)
        {
            // 1. [스킬 최초 시작 시 1회 셋업] 
            if (!_isSkillAnimationStarted)
            {
                _isSkillAnimationStarted = true;
                _isSkilling = true; // [추가]
                _isSkillEndAnimationStart = false;
                _isSwordGathered = false;
                _skillGatherTimer = 0.0f;

                if (game_object()) {
                    if (auto socket = game_object()->get_component<SocketComponent>())
                        socket->set_isFollowAnimation(true); // 손에 다시 붙이기
                }
                if (_particleEffectObject) {
                    if (auto psComp = _particleEffectObject->get_component<GatherParticleComponent>())
                        psComp->set_particle_dying(false); // 흩어짐 플래그 끄기
                }

                anim_comp->play("skill", false, _skillAnimationspeed);
            }

            // 2. [스킬 종료 애니메이션 진행 중일 때]
            if (_isSkillEndAnimationStart) {
                // 이미 타격하고 흩어지는 중이므로 연산 패스 (서버가 IDLE로 바꿔줄 때까지 대기)
            }
            // 3. [Phase 1 & 2: 모으고 찍기]
            else
            {
                float current_anim_time = anim_comp->get_anim_time();

                if (current_anim_time >= _skillParticleSpawnTime)
                {
                    _particleEffectObject->set_enabled(true);
                    auto psComp = _particleEffectObject->get_component<GatherParticleComponent>();

                    // 아직 안 모임 -> 애니 정지 및 타이머 누적
                    if (!_isSwordGathered)
                    {
                        anim_comp->set_anim_speed(0.0f);
                        _skillGatherTimer += deltaTime;

                        float progress = std::clamp(_skillGatherTimer / _particleGatherDuration, 0.0f, 1.0f);

                        if (psComp) psComp->set_compute_data(_SkillObject->transform()->world_matrix(), transform()->local_position(), progress);

                        // 다 모임 -> 애니 다시 재생
                        if (progress >= 1.0f) {
                            _isSwordGathered = true;
                            anim_comp->set_anim_speed(_skillSwingAnimationSpeed); // 스킬이 모인후에 내려찍는 애니메이션 속도
                        }
                    }
                    // 다 모인 상태 유지
                    else
                    {
                        if (psComp) psComp->set_compute_data(_SkillObject->transform()->world_matrix(), transform()->local_position(), 1.0f);
                    }
                }

                // 4. [Phase 3: 타격 순간 흩어짐 연출]
                if (anim_comp->is_anim_finished())
                {
                    _isSkillEndAnimationStart = true;

                    if (game_object()) {
                        if (auto socket = game_object()->get_component<SocketComponent>())
                            socket->set_isFollowAnimation(false); // 검 공중에 정지
                    }

                    anim_comp->play("skill_end", false, _skillEndingAnimationSpeed);

                    // [추가] 타격 순간 먼지 사운드 재생 (메인 플레이어와 동일하게 구간 재생)
                    SoundManager::instance()->play_3d_section("Other_DustSound", transform()->get_world_position(), "00:00", "01:500", SoundType::SFX, 0.7f);

                    if (_particleEffectObject) {
                        if (auto psComp = _particleEffectObject->get_component<GatherParticleComponent>())
                            psComp->set_particle_dying(true); // 파티클 흩어짐
                    }
                }
            }
        }
        else if (_action_id >= common::packet::ActionID::Common::DASH_FWD && _action_id <= common::packet::ActionID::Common::DASH_RIGHT)
        {
            if (!_isDashAnimationStarted)
            {
                _isDashAnimationStarted = true;
                SoundManager::instance()->play_3d("Other_PlayerDash", transform()->get_world_position(), SoundType::SFX, 1.0f, false);
                std::string animName = "dash_fwd";
                if (_action_id == common::packet::ActionID::Common::DASH_BWD) animName = "dash_bwd";
                else if (_action_id == common::packet::ActionID::Common::DASH_LEFT) animName = "dash_left";
                else if (_action_id == common::packet::ActionID::Common::DASH_RIGHT) animName = "dash_right";

                anim_comp->play(animName, false, 2.0f);
            }
        }
        else
        {
            if (_isSkillAnimationStarted) 
            {
                init_skill_variables();
            }
        }
        break;
    case common::packet::EntityState::MOVE:
        _isDashAnimationStarted = false; // [추가]
        if (_isSkillAnimationStarted) 
        {
            init_skill_variables();
        }
        anim_comp->play("walk", true, (common::move_speed::player_walk_speed / common::anim_speed::player_walk_animation));
        break;
    case common::packet::EntityState::RUN:
        _isDashAnimationStarted = false; // [추가]
        if (_isSkillAnimationStarted)
        {
            init_skill_variables();
        }
        anim_comp->play("run", true, (common::move_speed::player_run_speed / common::anim_speed::player_run_animation));
       
        break;
	case common::packet::EntityState::IDLE:
        _isDashAnimationStarted = false; // [추가]
        if (_isSkillAnimationStarted)
        {
            init_skill_variables();
        }
		anim_comp->play("idle");
       
        break;
    case common::packet::EntityState::GRABBED: // [추가]
        _isDashAnimationStarted = false; // [추가]
        if (_isSkillAnimationStarted)
        {
            init_skill_variables();
        }
        anim_comp->play("die", false); // 잡힌 동안 고통받는 모습
        break;
    case common::packet::EntityState::DEAD:
        // 피격 애니메이션 재생 (예시)
        if (_isSkillAnimationStarted)
        {
            init_skill_variables();
        }
        anim_comp->play("die", false);
		break;
    }
}

void OtherPlayerScript::awake()
{
    auto render_comp = game_object()->get_component<RenderComponent>().get();
	auto animation_comp = game_object()->get_component<AnimationComponent>().get();

    auto idleMesh =
        ResourceManager::instance()->load_mesh("Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf", true);

    std::string animationpath = "Resource/Character/DarkKnight/DKF_animations/";
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Idle_Alert.gltf", "idle");
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Walk_Alert_Fwd.gltf", "walk");
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Run_Alert_Fwd.gltf", "run");
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Attack_01.gltf", "attack01");
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Skill_01.gltf", "skill01");
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Skill_01_end.gltf", "skill_end");
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Death.gltf", "death");

    // [추가] 4방향 대쉬 애니메이션 로드 (이 부분이 누락되어 마지막 모션에서 멈춰있었음)
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Crouch_Alert_Fwd.gltf", "dash_fwd");
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Crouch_Alert_Bwd.gltf", "dash_bwd");
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Crouch_Alert_Left.gltf", "dash_left");
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Crouch_Alert_Right.gltf", "dash_right");
    
	render_comp->set_mesh(idleMesh);

    animation_comp->add_animation("idle", idleMesh, "idle");
    animation_comp->add_animation("walk", idleMesh, "walk");
    animation_comp->add_animation("run", idleMesh, "run");
    animation_comp->add_animation("attack", idleMesh, "attack01");
    animation_comp->add_animation("skill", idleMesh, "skill01");
    animation_comp->add_animation("skill_end", idleMesh, "skill_end");
    animation_comp->add_animation("die", idleMesh, "death");

    // [추가] 4방향 대쉬 애니메이션 추가
    animation_comp->add_animation("dash_fwd", idleMesh, "dash_fwd");
    animation_comp->add_animation("dash_bwd", idleMesh, "dash_bwd");
    animation_comp->add_animation("dash_left", idleMesh, "dash_left");
    animation_comp->add_animation("dash_right", idleMesh, "dash_right");
    

    animation_comp->play("idle");

    // [사운드] 플레이어 무기 공격음 / 피격음 로드
    SoundManager::instance()->load_sound("Other_SwordSwing", "Resource/Sound/SwordSwing.mp3", true);
    SoundManager::instance()->load_sound("Other_DustSound", "Resource/Sound/Dust.wav", true);
    SoundManager::instance()->load_sound("Other_PlayerDamage", "Resource/Sound/PlayerDamage.wav", true);
    SoundManager::instance()->load_sound("Other_PlayerDash", "Resource/Sound/PlayerDash.ogg", true);

    // 재질 및 쉐이더 설정
	// ResourceManager을 통해 재질 생성 및 셰이더 할당
    std::string material_name = "player_material"; // player는 고정된 재질
    ResourceManager::instance()->create_material(material_name);
    ResourceManager::instance()->set_shader_for_material(material_name, "skinned");

    // gltf
    render_comp->set_pso_name("skinned");

    // 위치, 회전 정보
    transform()->set_local_scale({ 1.0f, 1.0f, 1.0f });


    // 무기 오브젝트 생성
    auto owner = game_object();
    auto socket = owner->get_component<SocketComponent>();

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
    //// 무기 렌더링 끄기
    _currentWeaponObject->get_component<RenderComponent>()->set_enabled(false);
    _currentWeaponObject->set_layer("OtherPlayer");

    _SkillObject = _currentWeaponObject;

    // --- 3. 무기 오브젝트에 기능(스크립트 + 콜라이더) 추가 ---
    //if (_currentWeaponObject) {
    //    // 물리 바디를 위한 콜라이더 추가
    //    _currentWeaponObject->add_component<PhysicsColliderComponent>();

    //    // 롱소드 로직 추가 (내부에서 콜라이더 initialize 호출됨)
    //    _currentWeapon = _currentWeaponObject->add_component<LongswordScript>();
    //    _currentWeapon->set_attack_active(true);
    //}

    auto skillRender = _SkillObject->get_component<RenderComponent>();
    auto gltfMesh = dynamic_pointer_cast<ReadGLTFMesh>(skillRender->mesh());

    if (gltfMesh)
    {
        // 5만 개의 빽빽한 점 데이터를 추출합니다.
        auto targets = gltfMesh->extract_particle_targets(50000);

        // 2. 파티클 시스템 전용 오브젝트 생성 (ObjectManager 팩토리 사용)
        _particleEffectObject = ObjectManager::instance()->create_game_object("CarianParticleEffect");
        _particleEffectObject->set_layer("OtherPlayer");

        // 3. 연산 담당 컴포넌트 추가 및 데이터 전송
        auto psComp = _particleEffectObject->add_component<GatherParticleComponent>();
        // 다크 판타지 소울류 팬텀(Phantom) 느낌의 색상 오라
        static const DirectX::XMFLOAT3 PlayerColors[4] =
        {
            DirectX::XMFLOAT3(0.8f, 0.1f, 0.05f),  // [0] 적령 (Blood Red)
            DirectX::XMFLOAT3(0.2f, 0.5f, 0.9f),   // [1] 청령 (Moonlight Blue)
            DirectX::XMFLOAT3(0.9f, 0.6f, 0.1f),   // [2] 태양령 (Sunlight Gold)
            DirectX::XMFLOAT3(0.5f, 0.1f, 0.8f),   // [3] 암령/광령 (Abyssal Purple)
        };

        DirectX::XMFLOAT4 color = { PlayerColors[_playerId % 4].x, PlayerColors[_playerId % 4].y, PlayerColors[_playerId % 4].z, 0.5f };
        psComp->init_particles(targets, color,0.05f);

        // 4. 렌더 컴포넌트 추가
        auto prComp = _particleEffectObject->add_component<ParticleRenderComponent>();
        prComp->set_pso_name("particle_draw");


        // 5. 위치 동기화 (대검 오브젝트의 자식으로 설정)
        _particleEffectObject->transform()->set_local_position({ 0, 0, 0 });
        _particleEffectObject->transform()->set_parent(_SkillObject->transform());

        // 초기에는 꺼둠
        _particleEffectObject->set_enabled(false);
    }

}
