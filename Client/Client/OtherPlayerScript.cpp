#include "stdafx.h"
#include "OtherPlayerScript.h"

#include "AnimationComponent.h"
#include "BehaviorTree.h"
#include "gameobject.h"
#include "ReadGLTFMesh.h"
#include "RenderComponent.h"
#include "ResourceManager.h"
#include "Renderer.h"

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


void OtherPlayerScript::update(float deltaTime)
{
    // [추측 항법 - 선택 사항]
	// 만약 서버에서 속도(Velocity) 패킷도 보낸다면, 여기에 _logicalPosition += _velocity * deltaTime; 추가 가능

	// 1. 시각적 오프셋을 매 프레임 조금씩 줄여나감 (0으로 수렴)
	// deltaTime * 15.0f 정도면 약 0.1초 내외로 보정이 완료되어 매우 부드럽게 보입니다.
    float lerpFactor = std::min(1.0f, deltaTime * _lerpFactor);
    _visualOffset = _visualOffset * (1.0f - lerpFactor);

    // [중요 - 이 부분이 빠졌습니다!]
	// 논리 위치와 시각적 오프셋을 더해 실제 Transform에 적용
    if (transform())
    {
        transform()->set_local_position(_logicalPosition + _visualOffset);
    }


	auto anim_comp = game_object()->get_component<AnimationComponent>();
	if (!anim_comp)
	{
        return;
	}
    switch (_state)
    {
	case common::packet::EntityState::ACTION:
        if (_action_id == 0)
        {
            anim_comp->play("attack");
        }
        break;
    case common::packet::EntityState::MOVE:
        anim_comp->play("walk", true, (common::move_speed::player_walk_speed / common::anim_speed::player_walk_animation));
		break;
    case common::packet::EntityState::RUN:
        anim_comp->play("run", true, (common::move_speed::player_run_speed / common::anim_speed::player_run_animation));
        break;
	case common::packet::EntityState::IDLE:
		anim_comp->play("idle");
		break;
    case common::packet::EntityState::DEAD:
        // 피격 애니메이션 재생 (예시)
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
    std::dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only(animationpath + "Anim_DKF_Death.gltf", "death");
    
	render_comp->set_mesh(idleMesh);

    animation_comp->add_animation("idle", idleMesh, "idle");
    animation_comp->add_animation("walk", idleMesh, "walk");
    animation_comp->add_animation("run", idleMesh, "run");
    animation_comp->add_animation("attack", idleMesh, "attack02");
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
}
