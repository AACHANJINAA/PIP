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
    // 서버가 알려준 위치로 내 GameObject의 위치를 설정
    if (transform())
    {
        transform()->set_local_position(newPosition);
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
            anim_comp->play("attack");
        }
        break;
    case common::packet::EntityState::MOVE:
        anim_comp->play("walk");
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

    auto idleMesh = ResourceManager::instance()->load_mesh("Resource/Character/Brute_idle/Brute_idle.gltf", true, "idle");
    auto walkMesh = ResourceManager::instance()->load_mesh("Resource/Character/Brute_Walk/Brute_Walk.gltf", true, "walk");
    dynamic_pointer_cast<ReadGLTFMesh>(idleMesh)->load_animation_only("Resource/Character/Brute_die/Brute_die.gltf", "die");
    render_comp->set_mesh(idleMesh);

    animation_comp->add_animation("idle", idleMesh, "idle");
    animation_comp->add_animation("walk", walkMesh, "walk");
    animation_comp->add_animation("attack", idleMesh, "attack"); // [추가] 공격 애니메이션 매핑 안되어 있어서 오류난거였음
    animation_comp->add_animation("die", idleMesh, "die"); // [추가] 죽음 애니메이션 추가
    

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
}
