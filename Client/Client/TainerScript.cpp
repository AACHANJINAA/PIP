#include "stdafx.h"
#include "TainerScript.h"

#include "AnimationComponent.h"
#include "DebugDrawManager.h"
#include "ReadGLTFMesh.h"
#include "ResourceManager.h"
#include "MonsterHPComponent.h"

void TainerScript::awake()
{
	NPCScript::awake();
    game_object()->get_component<TransformComponent>()->set_local_scale({ 5.f,5.f ,5.f });
    auto hp = get_hp();
    game_object()->get_component<MonsterHPComponent>()->set_max_hp(get_hp());
    CLOG("[TainerScript] Boss Initialization Complete.");
}

void TainerScript::init_visual()
{
    auto obj = game_object();
    // 1. 보스 크기 설정
    

    auto animComp = obj->get_component<AnimationComponent>();
    auto renderComp = obj->get_component<RenderComponent>();

    if (animComp && renderComp)
    {
        const std::string basePath = "Resource/Character/BoneGolem/";

        // 1. 메인 메쉬 로드 (이 파일은 반드시 메쉬 데이터를 포함해야 함)
        auto mainMesh = ResourceManager::instance()->load_mesh(basePath + "BoneGolemRd.gltf", true);
        ReadGLTFMesh* gltfMesh = static_cast<ReadGLTFMesh*>(mainMesh.get());
        //gltfMesh->set_shader_for_all_materials("skinned");

        // 2. 애니메이션만 별도로 로드하여 병합 (load_mesh 대신 load_animation_only 사용)
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Idle.gltf", "idle");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Walk.gltf", "walk");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Run.gltf", "run");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Attack.gltf", "attack");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Attack01.gltf", "attack2");
		gltfMesh->load_animation_only(basePath + "A_BoneGolem_Attack02.gltf", "attack3");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Hit.gltf", "hit");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Roar.gltf", "roar");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Swim.gltf", "swim");


        renderComp->set_mesh(mainMesh);

        // 3. 상태별 애니메이션 매핑
        using namespace common::packet;
        animComp->add_state_mapping(OBJECT_STATE::IDLE, "idle", mainMesh);
        animComp->add_state_mapping(OBJECT_STATE::WALK, "walk", mainMesh);
        animComp->add_state_mapping(OBJECT_STATE::RUN, "run", mainMesh);
        animComp->add_state_mapping(OBJECT_STATE::ATTACK1, "attack", mainMesh);
        animComp->add_state_mapping(OBJECT_STATE::ATTACK2, "attack2", mainMesh);
        animComp->add_state_mapping(OBJECT_STATE::ATTACK3, "attack3", mainMesh);
        animComp->add_state_mapping(OBJECT_STATE::CHARGE, "swim", mainMesh);
        animComp->add_state_mapping(OBJECT_STATE::HITTED, "hit", mainMesh);
        animComp->add_state_mapping(OBJECT_STATE::ROAR, "roar", mainMesh);

        // DW주의 : 지금 테이너 SKILL1을 서버에서 넣어주고 있는데 클라이언트에서는 없어서 에러 발생 임시로 넣음
        animComp->add_state_mapping(OBJECT_STATE::SKILL1, "roar", mainMesh);
        CLOG("[TainerScript] BoneGolem Boss Visuals Settings Completed.");
    }

    // 4. 재질 및 쉐이더 설정 (Skinned Shader)
    std::string matName = "mat_tainer_boss_" + std::to_string(id());
    ResourceManager::instance()->create_material(matName);
    ResourceManager::instance()->set_shader_for_material(matName, "skinned");
    renderComp->set_pso_name("skinned");

    CLOG("[TainerScript] BoneGolem Boss Visuals Initialized.");
}

void TainerScript::update(float deltaTime)
{
    
    NPCScript::update(deltaTime);
    // 디버그 드로우 매니저를 통해 보스 머리 위에 현재 노드 이름 표시
    if (!_currentBTNodeName.empty()) {
        common::Vec3 headPos = position();
        headPos.y += 5.0f; // 보스 키만큼 올림
		CLOG("[TainerScript] Current BT Node: " << _currentBTNodeName);
    }
}

void TainerScript::on_server_update(const XMFLOAT3& pos, const XMFLOAT3& vel, const XMFLOAT4& rot, uint32_t timestamp)
{
	NPCScript::on_server_update(pos, vel, rot, timestamp);
	CLOG("[TainerScript] Received Server Update - Pos: (" << pos.x << "," << pos.y << "," << pos.z << ") Vel: (" << vel.x << "," << vel.y << "," << vel.z << ") Rot: (" << rot.x << "," << rot.y << "," << rot.z << "," << rot.w << ") Timestamp: " << timestamp);
}
