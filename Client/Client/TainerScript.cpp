#include "stdafx.h"
#include "TainerScript.h"

#include "AnimationComponent.h"
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
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Hit.gltf", "hit");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Roar.gltf", "roar");

        renderComp->set_mesh(mainMesh);

        // 3. 상태별 애니메이션 매핑
        using namespace common::packet;
        animComp->add_state_mapping(OBJECT_STATE::IDLE, "idle", mainMesh);
        animComp->add_state_mapping(OBJECT_STATE::WALK, "walk", mainMesh);
        animComp->add_state_mapping(OBJECT_STATE::RUN, "run", mainMesh);
        animComp->add_state_mapping(OBJECT_STATE::ATTACK, "attack", mainMesh);
        animComp->add_state_mapping(OBJECT_STATE::HITTED, "hit", mainMesh);
        animComp->add_state_mapping(OBJECT_STATE::ROAR, "roar", mainMesh);
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
    // 1. 부모의 동기화 로직 수행 (위치 보간 등)
    NPCScript::update(deltaTime);
	auto pos = position();
    //CLOG("(" << pos.x << "," << pos.y << "," << pos.z << ")");

    //// 2. [테스트] 1초마다 애니메이션 상태 변경
    //_testTimer += deltaTime;
    //if (_testTimer >= 1.0f)
    //{
    //    _testTimer = 0.0f;

    //    using namespace common::packet;
    //    static OBJECT_STATE testStates[] = {
    //        OBJECT_STATE::IDLE,
    //        OBJECT_STATE::WALK,
    //        OBJECT_STATE::RUN,
    //        OBJECT_STATE::ATTACK,
    //        OBJECT_STATE::HITTED, 
    //        OBJECT_STATE::ROAR    
    //    };

    //    OBJECT_STATE nextState = testStates[_testAnimIdx];
    //    set_state(nextState);

    //    CLOG("[Tainer Test] State: " << (int)nextState << " (Anim Index: " << _testAnimIdx << ")");

    //    _testAnimIdx = (_testAnimIdx + 1) % (sizeof(testStates) / sizeof(testStates[0]));
    //}
}

void TainerScript::on_server_update(const XMFLOAT3& pos, const XMFLOAT3& vel, const XMFLOAT4& rot, uint32_t timestamp)
{
	NPCScript::on_server_update(pos, vel, rot, timestamp);
	CLOG("[TainerScript] Received Server Update - Pos: (" << pos.x << "," << pos.y << "," << pos.z << ") Vel: (" << vel.x << "," << vel.y << "," << vel.z << ") Rot: (" << rot.x << "," << rot.y << "," << rot.z << "," << rot.w << ") Timestamp: " << timestamp);
}
