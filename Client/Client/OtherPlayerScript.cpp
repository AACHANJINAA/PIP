#include "stdafx.h"
#include "OtherPlayerScript.h"

#include "AnimationComponent.h"
#include "BehaviorTree.h"
#include "gameobject.h"
#include "ObjectManager.h"
#include "ReadGLTFMesh.h"
#include "RenderComponent.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "SocketComponenet.h"
#include "ParticleSystemComponent.h"
#include "ParticleRenderComponent.h"

void OtherPlayerScript::on_sync_position(const XMFLOAT3& newPosition)
{
    // 1. 패킷이 오기 전까지 화면에 그려지고 있던 '최종 시각적 위치' 계산
    common::Vec3 currentVisualPos = _logicalPosition + _visualOffset;

    // 2. 논리 위치는 서버가 보내준 좌표로 즉시 업데이트 (순간이동)
    _logicalPosition = newPosition;

    // 3. 화면이 툭 튀는 것을 막기 위해 오프셋 재계산
    // (이전 시각적 위치 - 새로운 서버 위치)를 오프셋으로 설정하여 현재 렌더링 위치를 유지함
    _visualOffset = currentVisualPos - _logicalPosition;

    // 4. 만약 오차가 너무 크면(예: 5m 이상) 보간하지 않고 즉시 스냅 (텔레포트 대응)
    if (common::LengthSq(_visualOffset) > 5.0f * 5.0f) {
        _visualOffset = { 0, 0, 0 };
    }
}

void OtherPlayerScript::on_sync_rotation(const XMFLOAT4& newRotation)
{
    // 서버가 알려준 회전으로 내 GameObject의 회전을 설정
    if (transform())
    {
        transform()->set_local_rotation(newRotation);
		transform()->rotate(0, 180, 0); // 서버와 클라이언트 간 모델 회전 차이 보정
	}
}

void OtherPlayerScript::on_sync_state(common::packet::EntityState state)
{
	_state = state;
}
void OtherPlayerScript::on_sync_action_id(int32_t action_id)
{
	_action_id = action_id;
}

void OtherPlayerScript::on_sync_grab(int64_t grabbed_by_id, int8_t grab_slot)
{
	_grabbedById = grabbed_by_id;
	_grabSlot = grab_slot;
}

void OtherPlayerScript::reset_state()
{
	_state = common::packet::EntityState::IDLE;
	_action_id = 0;
	_grabbedById = -1;
	_grabSlot = -1;
	_velocity = { 0, 0, 0 };
	_visualOffset = { 0, 0, 0 };

	// 애니메이션 강제 초기화
	auto anim = game_object()->get_component<AnimationComponent>();
	if (anim) {
		anim->play("idle", true);
	}
}

void OtherPlayerScript::init_skill_variables()
{
    _isSkilling = false;
    _nowSkillTime = 0.0f;
    _SkillObject->get_component<RenderComponent>()->set_enabled(false);
    game_object()->get_component<SocketComponenet>()->set_isFollowAnimation(true);
    _particleEffectObject->set_enabled(false);
}


void OtherPlayerScript::update(float deltaTime)
{
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

                    _logicalPosition = transform()->local_position();
                    _visualOffset = { 0, 0, 0 };
                    return;
                }
            }
        }
    }

    // [추가] 잡기 해제 시 상태 보정
    if (_grabbedById == -1 && _state == common::packet::EntityState::GRABBED) {
        _state = common::packet::EntityState::IDLE;
    }

    // [추측 항법] 서버의 속도값을 활용해 매 프레임 위치 예측
	_logicalPosition += _velocity * deltaTime;

	// 1. 시각적 오프셋을 매 프레임 조금씩 줄여나감 (0으로 수렴)
	// deltaTime * 15.0f 정도면 약 0.1초 내외로 보정이 완료되어 매우 부드럽게 보입니다.
    float lerpFactor = std::min(1.0f, deltaTime * _lerpFactor);
    _visualOffset = _visualOffset * (1.0f - lerpFactor);

    // [중요 - 이 부분이 빠졌습니다!]
	// 논리 위치와 시각적 오프셋을 더해 실제 Transform에 적용
    if (transform())
    {
        common::Vec3 visualPosition = _logicalPosition + _visualOffset + common::Vec3{0, -0.1, 0};
        transform()->set_local_position(visualPosition);
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
            anim_comp->play("attack", false);
        }
        else if (_action_id == common::packet::ActionID::Common::SKILL1)
        {
            anim_comp->play("skill", false, skillAnimationspeed);
            _nowSkillTime += deltaTime;
            float current_anim_time = anim_comp->get_anim_time();

            if (current_anim_time >= _skillDontFollowAnimationTime)
            {
                game_object()->get_component<SocketComponenet>()->set_isFollowAnimation(false);
            }

            float progress = std::clamp(current_anim_time / _skillDontFollowAnimationTime, 0.0f, 1.0f);

            auto psComp = _particleEffectObject->get_component<ParticleSystemComponent>();
            if (psComp)
            {
                // 파티클 오브젝트를 활성화
                _particleEffectObject->set_enabled(true);

                // 연산을 쏘지 않고, 데이터만 저장만 하기
                psComp->set_compute_data(
                    _SkillObject->transform()->world_matrix(),
                    transform()->local_position(),
                    progress
                );
            }
        }
        else
        {
            init_skill_variables();
        }
        break;
    case common::packet::EntityState::MOVE:
        anim_comp->play("walk", true, (common::move_speed::player_walk_speed / common::anim_speed::player_walk_animation));
        init_skill_variables();
        break;
    case common::packet::EntityState::RUN:
        anim_comp->play("run", true, (common::move_speed::player_run_speed / common::anim_speed::player_run_animation));
        init_skill_variables();
        break;
	case common::packet::EntityState::IDLE:
		anim_comp->play("idle");
        init_skill_variables();
        break;
    case common::packet::EntityState::GRABBED: // [추가]
        if (_currentWeaponObject)
        {
            _currentWeaponObject->set_enabled(false);
        }
        init_skill_variables();
        anim_comp->play("die", false); // 잡힌 동안 고통받는 모습
        break;
    case common::packet::EntityState::DEAD:
        // 피격 애니메이션 재생 (예시)
        if (_currentWeaponObject)
        {
            _currentWeaponObject->set_enabled(false);
        }
        init_skill_variables();
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
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Attack_02.gltf", "attack02");
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Skill_01.gltf", "skill01");
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Death.gltf", "death");
    
	render_comp->set_mesh(idleMesh);

    animation_comp->add_animation("idle", idleMesh, "idle");
    animation_comp->add_animation("walk", idleMesh, "walk");
    animation_comp->add_animation("run", idleMesh, "run");
    animation_comp->add_animation("attack", idleMesh, "attack02");
    animation_comp->add_animation("skill", idleMesh, "skill01");
    animation_comp->add_animation("die", idleMesh, "death");
    

    animation_comp->play("idle");

    // 재질 및 쉐이더 설정
	// ResourceManager을 통해 재질 생성 및 셰이더 할당
    std::string material_name = "player_material"; // player는 고정된 재질
    ResourceManager::instance()->create_material(material_name);
    ResourceManager::instance()->set_shader_for_material(material_name, "skinned");

    // gltf
    render_comp->set_pso_name("skinned");

    // 위치, 회전 정보
    transform()->set_local_scale({ 1.0f, 1.0f, 1.0f });
    // 초기화 시 현재 위치를 논리 위치로 설정
    _logicalPosition = transform()->local_position();
    _visualOffset = { 0, 0, 0 };


    // 무기 오브젝트 생성
    auto owner = game_object();
    auto socket = owner->get_component<SocketComponenet>();

    // 다크나이트의 hand_l 오프셋을 참고하여 hand_r용으로 미러링한 값입니다.
    // 좌표와 회전은 모델을 보면서 미세 조정이 필요할 수 있습니다.
    _currentWeaponObject = socket->add_connecting(
        "MainWeapon",
        "hand_r", // 반대쪽 손
        "Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf",
        { 0.f,0.f,0.f },   // hand_l 기준 X값 반전 시도
        { 10.f, -90.f, 0.f },       // 오른손 파지 각도에 맞게 회전 조정
        { 15.f, 15.f, 15.f }
    );
    //// 무기 렌더링 끄기
    _currentWeaponObject->get_component<RenderComponent>()->set_enabled(false);

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
