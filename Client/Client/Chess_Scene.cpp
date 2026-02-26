#include "stdafx.h"
#include "Chess_Scene.h"

#include "FreeCameraScript.h"
#include "ObjectManager.h"
#include "GameObject.h"

#include "GltfAnimationScript.h"

#include "TransformComponent.h"
#include "RenderComponent.h"
#include "ResourceManager.h"
#include "InputManager.h"
#include "AnimationComponent.h"
#include "SocketComponenet.h"
#include "ReadGLTFMesh.h"

#include "TerrainLoader.h"
#include "UIRenderComponent.h"
#include "MonsterHPUIRenderComponent.h"

#include "SkyboxRenderComponent.h"
#include "CameraComponent.h"
#include "PhysicsColliderComponent.h"
#include "Renderer.h"
#include "SceneManager.h"
#include "Tool_Scene.h"


void Chess_Scene::build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	// =========================필요한 메시 로드==================================

    ResourceManager::instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");
    ResourceManager::instance()->load_mesh("Resource/Character/Brute_Walk/Brute_Walk.gltf",true,"walk");

    auto idle_brute_mesh = ResourceManager::instance()->load_mesh("Resource/Character/Brute_idle/Brute_idle.gltf",true,"idle");
    dynamic_pointer_cast<ReadGLTFMesh>(idle_brute_mesh)->load_animation_only("Resource/Character/Brute_Attack_animation/Brute_Attack_animation.gltf","attack");
	// =========================================================================



    // 카메라 생성
    auto cameraObject = ObjectManager::instance()->create_game_object("FreeCamera");
    cameraObject->add_component<FreeCameraScript>();
    cameraObject->set_layer("Camera");
    cameraObject->transform()->set_local_position(XMFLOAT3(0.0f, 500.0f, 10.0f)); // 20m 높이
    cameraObject->transform()->set_local_rotation(90.0f, 0.0f, 0.0f); // 약간 아래 보기

    load_scene_from_file("Resource/MD/ExportedClientData.json", device, commandList);

	// DW설명 : 브루트 소년단 생성 함수 호출
	//SpawnBTS(device, commandList);

	// DW설명 : 그래미 워크 생성 함수 호출
	//SpawnGrammy_Walk(device, commandList);

    Spawn_SK_MagicConstruct(device, commandList);

	Spawn_UI(device, commandList);

	Spawn_Monster_HP_UI(device, commandList);

	// DW설명 : 플레이어 오브젝트 생성
    {
        //auto playerObject = ObjectManager::Instance()->create_game_object("MainPlayer");
        //// MainPlayerScript추가
        //playerObject->add_component<MainPlayerScript>();
        ////// RenderComponent
        //auto renderer = playerObject->add_component<RenderComponent>();

        //auto playerMesh = ResourceManager::Instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");

        //// 재질 및 쉐이더 설정
        //auto material = std::make_shared<GltfMaterial>("test_Material");
        //material->set_shader(Renderer::Instance()->get_shader("gltf"));
        //renderer->set_material(material);

        //// gltf
        //renderer->set_pso_name("gltf");

        //// 위치, 회전 정보
        //playerObject->transform()->set_local_rotation(-90.f, 0.f, 0.f);  
        //playerObject->transform()->set_local_scale({ 200.0f, 200.0f, 200.0f }); 

        //

        //playerObject->transform()->set_local_position(XMFLOAT3(0.0f, 70.0f, -150.0f));
        //ResourceManager::Instance()->upload_pending_meshes(device, commandList);
    }

    SpawnDummyNPC(device, commandList);
}

void Chess_Scene::release_upload_buffers()
{
}

void Chess_Scene::scene_process(float deltaTime)
{
    // InputManager의 IsKeyDown을 사용하여 'T' 키가 막 눌린 시점을 감지합니다.
    if (InputManager::instance()->IsKeyDown('T'))
    {
        // 씬 매니저에게 툴 씬으로 넘어가라고 요청합니다.
        SceneManager::instance()->change_scene("ToolScene");
    }
}

void Chess_Scene::SpawnDummyNPC(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    

    auto npc = ObjectManager::instance()->create_game_object("DummyNPC");
    npc->transform()->set_local_position({ 5.0f, 0.0f, 5.0f });

    // 1. 렌더 컴포넌트 추가
    auto render_comp = npc->add_component<RenderComponent>();

    // 2. 메쉬 로드 (플레이어와 동일한 메쉬 사용)
    auto npc_mesh = ResourceManager::instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");
    render_comp->set_mesh(npc_mesh);

    // 3. 재질(Material) 및 PSO 설정
    // 기존에 "gltf" PSO가 설정되어 있어야 합니다.
    render_comp->set_pso_name("gltf");

    // 4. 크기 및 회전 조정 (너무 작거나 누워있을 수 있으므로)
    npc->transform()->set_local_scale({ 200.0f, 200.0f, 200.0f }); // 플레이어와 비슷한 크기
    npc->transform()->set_local_rotation(-90.0f, 0.0f, 0.0f);    // GLTF 특성상 -90도 회전이 필요할 수 있음

    // 5. 물리 컴포넌트 추가 (캡슐)
    auto collider = npc->add_component<PhysicsColliderComponent>();
    // 반지름 0.5, 높이 1.0 (플레이어 크기에 맞춰 조정)
    collider->initialize(PhysicsColliderComponent::ShapeType::Capsule, { 0.5f, 1.0f, 0.0f }, { 0.f, 1.0f, 0.f },
        { 0,0,0 }, false);
}

void Chess_Scene::SpawnBTS(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // DW설명 : 인사 애니메이션 오브젝트 생성
    {
        auto hi_brute = ObjectManager::instance()->create_game_object("Hi_animation_brute");
        // GltfAnimationScript추가

        hi_brute->add_component<GltfAnimationScript>();
        //// RenderComponent
        auto renderer = hi_brute->add_component<RenderComponent>();

        auto hi_brute_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/Animation_BruteHi/bruteHi.gltf", true);
        renderer->set_mesh(hi_brute_Mesh);

        // 재질 및 쉐이더 설정
        std::string material = "skinned_animation_brute";

        ResourceManager::instance()->create_material(material);
        ResourceManager::instance()->set_shader_for_material(material, "skinned");

        // gltf
        renderer->set_pso_name("skinned");

        // 위치, 회전 정보
        hi_brute->transform()->set_local_rotation(0.f, 180.f, 0.f);
        hi_brute->transform()->set_local_scale({ 25.0f, 25.0f, 25.0f });


        hi_brute->transform()->set_local_position(XMFLOAT3(0.0f, 25.0f, -130.0f));
    }
    
	for (int i = 0; i < 5; ++i)
    {
        auto hi_brute = ObjectManager::instance()->create_game_object("Dance_animation_brute");
        // GltfAnimationScript추가

        hi_brute->add_component<GltfAnimationScript>();
        //// RenderComponent
        auto renderer = hi_brute->add_component<RenderComponent>();

        auto hi_brute_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/BruteDance/BruteDance.gltf", true);
        renderer->set_mesh(hi_brute_Mesh);

        // 재질 및 쉐이더 설정
        std::string material = "skinned_Dance_brute";

        ResourceManager::instance()->create_material(material);
        ResourceManager::instance()->set_shader_for_material(material, "skinned");

        // gltf
        renderer->set_pso_name("skinned");

        // 위치, 회전 정보
        hi_brute->transform()->set_local_rotation(90.f,(0.f + 45.f*i), 90.f);
        hi_brute->transform()->set_local_scale({ 25.0f, 25.0f, 25.0f });



        hi_brute->transform()->set_local_position(XMFLOAT3((-100.f + i * 50.f), 50.0f, -80.0f));
    }
}

void Chess_Scene::SpawnGrammy_Walk(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // DW설명 : 인사 애니메이션 오브젝트 생성
    {
        auto hi_brute = ObjectManager::instance()->create_game_object("Grammy_Walk");
        // GltfAnimationScript추가

        //hi_brute->add_component<GltfAnimationScript>();
     
        //// RenderComponent
        auto renderer = hi_brute->add_component<RenderComponent>();

        auto hi_brute_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/Gramma_Walk/Gramma_Walk.gltf", true,"walk");
        renderer->set_mesh(hi_brute_Mesh);

        auto animaiton_component = hi_brute->add_component<AnimationComponent>();
        animaiton_component->add_state_mapping(common::packet::OBJECT_STATE::WALK,"walk", hi_brute_Mesh);
		animaiton_component->set_state(common::packet::OBJECT_STATE::WALK);

        // 재질 및 쉐이더 설정
        std::string material = "skinned_animation_Gramma_Walk";

        ResourceManager::instance()->create_material(material);
        ResourceManager::instance()->set_shader_for_material(material, "skinned");

        // gltf
        renderer->set_pso_name("skinned");

        // 위치, 회전 정보
        hi_brute->transform()->set_local_rotation(0.f, 180.f, 0.f);
        hi_brute->transform()->set_local_scale({ 25.0f, 25.0f, 25.0f });


        hi_brute->transform()->set_local_position(XMFLOAT3(0.0, 25.0f, -130.0f));
    }
}

void Chess_Scene::Spawn_SK_MagicConstruct(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    float offsetX = 0.0f;
    float offsetY = -50.0f;
    float offsetZ = +200.0f;
    // DW설명 : 인사 애니메이션 오브젝트 생성
    {
        auto hi_brute = ObjectManager::instance()->create_game_object("SK_MagicConstruct");

        // 메쉬 설정
        auto hi_brute_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/SK_MagicConstruct/SK_MagicConstruct.gltf", true);
        // 메쉬에 맞는 애니메이션 추가
		ReadGLTFMesh* gltf_mesh = static_cast<ReadGLTFMesh*>(hi_brute_Mesh.get());
        gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Dodge.gltf");
        gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack03.gltf","attack");
        gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack02.gltf");
        gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack01.gltf");
        gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack.gltf");
        gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Stun.gltf");
        gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Roar.gltf");
    
        // 렌더 컴포넌트 추가
        auto renderer = hi_brute->add_component<RenderComponent>();
        renderer->set_mesh(hi_brute_Mesh);

        // 애니메이션 컴포넌트 추가
        hi_brute->add_glTF_conponent_pack(); // 이 함수가 애니메이션과 소켓 컴포넌트 추가함

        auto animation_renderer = hi_brute->get_component<AnimationComponent>();
        animation_renderer->add_state_mapping(common::packet::OBJECT_STATE::IDLE, "hi_brute_mesh", hi_brute_Mesh);
        animation_renderer->add_state_mapping(common::packet::OBJECT_STATE::ATTACK, "attack", hi_brute_Mesh);
        animation_renderer->set_state(common::packet::OBJECT_STATE::ATTACK);
        // 재질 및 쉐이더 설정
        std::string material = "skinned_animation_SK_MagicConstruct";

        ResourceManager::instance()->create_material(material);
        ResourceManager::instance()->set_shader_for_material(material, "skinned");

        // 원하는 무기 붙이기
        auto socket_compnenet = hi_brute->get_component<SocketComponenet>();
        socket_compnenet->add_connecting("ik_hand_l_sword", "hand_l", "Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf", { 0.0623f, -0.8154f, 0.1643f }, { -10.f,90.f,-179.f }, { 2.f,2.f,2.f });

        // 스키닝 애니메이션 pso 설정              
        renderer->set_pso_name("skinned");

        // 위치, 회전 정보
        hi_brute->transform()->set_local_rotation(0.f, 180.f, 0.f);
        hi_brute->transform()->set_local_scale({ 25.0f, 25.0f, 25.0f });


        hi_brute->transform()->set_local_position(XMFLOAT3(0.0, 25.0f, -130.0f));
    }
    //{
    //    auto hi_brute = ObjectManager::instance()->create_game_object("SK_MagicConstruct_gltf");

    //    // 메쉬 설정
    //    auto hi_brute_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/SK_MagicConstruct/SK_MagicConstruct.gltf", true);
    //    // 메쉬에 맞는 애니메이션 추가
    //    ReadGLTFMesh* gltf_mesh = static_cast<ReadGLTFMesh*>(hi_brute_Mesh.get());
    //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Dodge.gltf");
    //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack03.gltf", "attack");
    //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack02.gltf");
    //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack01.gltf");
    //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack.gltf");
    //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Stun.gltf");
    //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Roar.gltf");

    //    // 렌더 컴포넌트 추가
    //    auto renderer = hi_brute->add_component<RenderComponent>();
    //    renderer->set_mesh(hi_brute_Mesh);

    //    // 애니메이션 컴포넌트 추가
    //    hi_brute->add_glTF_conponent_pack(); // 이 함수가 애니메이션과 소켓 컴포넌트 추가함
    //    
    //    auto animation_renderer = hi_brute->get_component<AnimationComponent>();
    //    animation_renderer->add_state_mapping(common::packet::OBJECT_STATE::T_POSE, "t_pose", hi_brute_Mesh);
    //    animation_renderer->add_state_mapping(common::packet::OBJECT_STATE::ATTACK, "attack", hi_brute_Mesh);
    //    animation_renderer->set_state(common::packet::OBJECT_STATE::T_POSE);
    //    // 재질 및 쉐이더 설정
    //    std::string material = "skinned_animation_SK_MagicConstruct_gltf";

    //    ResourceManager::instance()->create_material(material);
    //    ResourceManager::instance()->set_shader_for_material(material, "skinned");

    //    // 원하는 무기 붙이기
    //    auto socket_compnenet = hi_brute->get_component<SocketComponenet>();
    //    socket_compnenet->add_connecting("ik_hand_l_sword", "hand_l", "Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf", { 0.0623f, -0.8154f, 0.1643f }, { -10.f,90.f,-179.f }, { 2.f,2.f,2.f });

    //    // 스키닝 애니메이션 pso 설정              
    //    renderer->set_pso_name("skinned");

    //    // 위치, 회전 정보
    //    hi_brute->transform()->set_local_rotation(0.f, 180.f, 0.f);
    //    hi_brute->transform()->set_local_scale({ 25.0f, 25.0f, 25.0f });


    //    hi_brute->transform()->set_local_position(XMFLOAT3(0.0, 0.0f, 0.0f));
    //}
}

void Chess_Scene::Spawn_UI(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // 1. HP Frame (뒤에 렌더링될 프레임)
    auto hp_frame_obj = ObjectManager::instance()->create_game_object("HP_Frame");
    auto hp_frame = hp_frame_obj->add_component<UIRenderComponent>();

    hp_frame->set_screen_position(30.0f, 30.0f);      // 화면 왼쪽 상단
    hp_frame->set_size(410.0f, 30.0f);                 // Bar보다 좀 더 큼
    hp_frame->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색 (텍스처 원본 색)
    hp_frame->set_texture("Resource/UI/HP_Bar_Frame.dds");

    // 2. HP Bar (앞에 렌더링될 바)
    auto hp_bar_obj = ObjectManager::instance()->create_game_object("HP_Bar");
    auto hp_bar = hp_bar_obj->add_component<UIRenderComponent>();

    hp_bar->set_screen_position(30.0f, 30.0f);        // Frame보다 안쪽
    hp_bar->set_size(500.0f, 30.0f);                   // Frame보다 작게
    hp_bar->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
    hp_bar->set_texture("Resource/UI/HP_Bar.dds");
}

void Chess_Scene::Spawn_Monster_HP_UI(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	auto monster_hp_frame_obj = ObjectManager::instance()->create_game_object("Monster_HP_Frame");
	auto monster_hp_ui_renderer = monster_hp_frame_obj->add_component<MonsterHPUIRenderComponent>();
	monster_hp_ui_renderer->set_hp_back_texture("Resource/UI/HP_Bar_Frame.dds");
	monster_hp_ui_renderer->set_hp_bar_texture("Resource/UI/HP_Bar.dds");
}
