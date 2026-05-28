#include "stdafx.h"
#include "gameobject.h"
#include "QuestNPCScript.h"
#include "ObjectManager.h"
#include "ResourceManager.h"
#include "RenderComponent.h"
#include "AnimationComponent.h"
#include "ReadGLTFMesh.h"

void QuestNPCScript::awake()
{
    auto renderer = game_object()->get_component<RenderComponent>();
    auto animation_component = game_object()->get_component<AnimationComponent>();

    if (!animation_component)
    {
        CERROR("애니메이션 컴포넌트 추가 안됨 틀 확인!");
    }

    // 메쉬 및 애니메이션 로드
    auto T1_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/Bandit_Rd_NPC/Bandit_Rd_NPC.gltf",true);
    auto mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh);
    if (mesh)
    {
        mesh->load_animation_only("Resource/Character/Bandit_Rd_NPC/Animations/A_Hu_F_Idle.gltf", "idle");
    }

    renderer->set_mesh(T1_Mesh);

    if (animation_component)
    {
        animation_component->add_animation("idle", T1_Mesh, "idle");
        animation_component->play("idle");
    }

    // 재질 및 쉐이더 설정
    std::string material = "NPC_Material";
    ResourceManager::instance()->create_material(material);
    ResourceManager::instance()->set_shader_for_material(material, "skinned");

    // skinned
    renderer->set_pso_name("skinned");

    // 위치, 회전, 스케일 설정
    auto trans = transform();
    if (trans)
    {
        trans->set_local_rotation(0.f, 0.f, 0.f);
        trans->set_local_scale({ 1.f, 1.f, 1.f });
        trans->set_local_position(DirectX::XMFLOAT3(-215.27, 6.59, -366.41));
    }
}

void QuestNPCScript::update(float deltaTime)
{
    // 퀘스트 NPC의 매 프레임 로직 (예: 플레이어 바라보기 등) 필요 시 구현
}
