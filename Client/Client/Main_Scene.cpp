#include "stdafx.h"
#include "Main_Scene.h"
#include "SceneManager.h"

#include "FreeCameraScript.h"
#include "ObjectManager.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ResourceManager.h"
#include "CameraComponent.h"
#include "DebugDrawManager.h"
#include "MonsterHPUIRenderComponent.h"
#include "AnimationComponent.h"
#include "ReadGLTFMesh.h"
#include "UIFrameRenderComponent.h"
#include "UIManager.h"
#include "UIRenderComponent.h"
#include "SoundManager.h"
#include "InputManager.h"
#include "NetworkManager.h"
#include "QuestNPCScript.h"
#include "ShadowManager.h"

void Main_Scene::build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    ShadowManager::instance()->set_shadow_max_distance(250.0f);
    // 효과음 테스트
	SoundManager::instance()->load_sound("Swing", "Resource/Sound/Dust.wav", true);
    SoundManager::instance()->play_3d("Swing", {-360,10,-212},SoundType::SFX,1.0f,true);


	// 1. Skybox 로드 (모든 Scene 공통)
    SceneManager::instance()->build_skybox(device, commandList,
        "Resource/SkyBox/",
        "cloudy/cloudy_skybox.dds",
        "cloudy/cloudy_specular.dds",
        "diffuse.txt",
        "BRDF.dds");

	// 2. MainScene 전용 Landscape 로드
	SceneManager::instance()->build_main_landscapes(device, commandList);

    // 3. 미니맵 활성화 -> 지형 이후에 호출해야함
    //SceneManager::instance()->build_minimap(device, commandList);

	// =========================필요한 메시 로드==================================
	ResourceManager::instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");
	ResourceManager::instance()->load_mesh("Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf"); // 검
	ResourceManager::instance()->load_mesh("Resource/Character/DragonBrute/SK_DragonBrute.gltf",true);
	ResourceManager::instance()->load_mesh("Resource/Character/Brute_Walk/Brute_Walk.gltf", true, "walk");
	ResourceManager::instance()->load_mesh("Resource/Character/BoneGolem/BoneGolem.gltf", true);
	ResourceManager::instance()->load_mesh("Resource/Character/BoneGolem/BoneGolemRd.gltf", true);
	ResourceManager::instance()->load_mesh("Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf", true);
    ResourceManager::instance()->load_mesh("Resource/Character/Bandit_Rd_NPC/Bandit_Rd_NPC.gltf",true);
	auto idle_brute_mesh = ResourceManager::instance()->load_mesh("Resource/Character/Brute_idle/Brute_idle.gltf", true, "idle");
	dynamic_pointer_cast<ReadGLTFMesh>(idle_brute_mesh)->load_animation_only("Resource/Character/Brute_Attack_animation/Brute_Attack_animation.gltf", "attack");
	// =========================================================================

    // 그 외
    load_scene_from_file("Resource/MainLandscape_Meshes/Landscape_0_0_MapData/Landscape_0_0_ExportedClientData.json", device, commandList);
    load_scene_from_file("Resource/MainLandscape_Meshes/Landscape_0_-1_MapData/Landscape_0_-1_ExportedClientData.json", device, commandList);
    load_foliage_from_file("Resource/Foliage/Foliage_tree_0_0_MapData/Foliage_tree_0_0_MapData.json", device, commandList);

    // 오두막
    load_scene_from_file("Resource/MainLandscape_Meshes/Landscape_-1_-1_MapData/Landscape_-1_-1_ExportedClientData.json", device, commandList);
    //load_foliage_from_file("Resource/Foliage/Foliage_stone_-1_-1_MapData/Foliage_stone_-1_-1_MapData.json", device, commandList);
    load_foliage_from_file("Resource/Foliage/Foliage_tree_-1_-1_MapData/Foliage_tree_-1_-1_MapData.json", device, commandList);

    // 성
	load_scene_from_file("Resource/MainLandscape_Meshes/Landscape_-1_0_MapData/Landscape_-1_0_ExportedClientData.json", device, commandList);
    load_foliage_from_file("Resource/Foliage/Foliage_tree_-1_0_MapData/Foliage_tree_-1_0_MapData.json", device, commandList);

    // 성당
    load_scene_from_file("Resource/MainLandscape_Meshes/Landscape_-2_-1_MapData/Landscape_-2_-1_ExportedClientData.json", device, commandList);

    // NPC
    auto questNpc = ObjectManager::instance()->create_game_object("QuestNPC");
    questNpc->add_component<QuestNPCScript>();

	// 카메라 생성
	auto cameraObject = ObjectManager::instance()->create_game_object("Camera");
    auto cameraComp = cameraObject->add_component<CameraComponent>(45.f);

	auto freeCameraScript = cameraObject->add_component<FreeCameraScript>();
	cameraObject->set_layer("Camera");
    if(InputManager::instance()->GetIsShowCusor())
    {
        InputManager::instance()->ChangeShowCusor();
    }

	cameraComp->set_main_camera();

	Spawn_UI(device, commandList);
    Spawn_Monster_HP_UI(device, commandList);
	//TestMesh(device, commandList);

    std::string path = "../../Common/World_Batch_glTF/Tile_X-1_Y-1/Tile_X-1_Y-1 Server Export Data.json";
    DebugDrawManager::instance()->LoadLocalDebugShape(path, "BP_house_03_Optimized15", "SM_House_Village_03_Merged");
}

void Main_Scene::release_upload_buffers()
{
	CLOG("Main_Scene: Releasing upload buffers");
}

void Main_Scene::scene_process(float deltaTime)
{
	// 씬 업데이트 로직 (필요시)
}

void Main_Scene::Spawn_UI(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // 0. player id별 텍스쳐  
    auto name_img_obj = ObjectManager::instance()->create_game_object("Player_Name_Image");
    auto name_renderer = name_img_obj->add_component<UIRenderComponent>();

    name_renderer->set_screen_position(5.0f, 5.0f);
    name_renderer->set_size(200.f, 200.f);         
    name_renderer->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

    long long my_id = NetworkManager::instance()->get_my_session_id();
	std::string resource_name = "Resource/UI/ID/Player_" + std::to_string(my_id % 4 + 1) + ".dds";  
    name_renderer->set_texture(resource_name);
    UIManager::instance()->add_ui(UILayer::MIDDLE, "PlayerNameImage", name_img_obj);

	// 1. HP Frame (뒤에 렌더링될 프레임) & HP Bar (앞에 렌더링될 체력바)
    auto hp_frame_obj = ObjectManager::instance()->create_game_object("HP_Frame");
    auto hp_frame = hp_frame_obj->add_component<UIRenderComponent>();
    auto hp_bar_obj = ObjectManager::instance()->create_game_object("HP_Bar");
    auto hp_bar = hp_bar_obj->add_component<UIRenderComponent>();

    std::pair<float, float> hp_bar_pos = { 200.0f , 50.0f};
    std::pair<float, float> hp_bar_size = { 410.0f , 26.0f};

    hp_frame->set_screen_position(hp_bar_pos.first, hp_bar_pos.second);      // 화면 왼쪽 상단
    hp_frame->set_size(hp_bar_size.first, hp_bar_size.second);                 // Bar보다 좀 더 큼
    hp_frame->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색 (텍스처 원본 색)
    hp_frame->set_texture("Resource/UI/HP_Bar_Frame.dds");

    hp_bar->set_screen_position(hp_bar_pos.first + 11.0f, hp_bar_pos.second + 4.0f);        // Frame보다 안쪽
    hp_bar->set_size(hp_bar_size.first - 24.f, hp_bar_size.second - 9.f);                   // Frame보다 작게
    hp_bar->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
    hp_bar->set_texture("Resource/UI/HP_Bar.dds");

    UIManager::instance()->add_ui(UILayer::BACKGROUND, "PlayerHPFrame", hp_frame_obj);
    UIManager::instance()->add_ui(UILayer::MIDDLE, "PlayerHPBar", hp_bar_obj);

    // 2. MP Frame (뒤에 렌더링될 프레임) & MP Bar (앞에 렌더링될 체력바)
    auto mp_frame_obj = ObjectManager::instance()->create_game_object("MP_Frame");
    auto mp_frame = mp_frame_obj->add_component<UIRenderComponent>();
    auto mp_bar_obj = ObjectManager::instance()->create_game_object("MP_Bar");
    auto mp_bar = mp_bar_obj->add_component<UIRenderComponent>();

    std::pair<float, float> mp_bar_pos = { 200.0f , 80.0f };
    std::pair<float, float> mp_bar_size = { 410.0f , 26.0f };

    mp_frame->set_screen_position(mp_bar_pos.first, mp_bar_pos.second);     
    mp_frame->set_size(mp_bar_size.first, mp_bar_size.second);                 
    mp_frame->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  
    mp_frame->set_texture("Resource/UI/HP_Bar_Frame.dds");
    
    mp_bar->set_screen_position(mp_bar_pos.first + 11.0f, mp_bar_pos.second + 4.0f);        
    mp_bar->set_size(mp_bar_size.first - 24.f, mp_bar_size.second - 9.f);             
    mp_bar->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    mp_bar->set_texture("Resource/UI/MP_Bar.dds");

    UIManager::instance()->add_ui(UILayer::BACKGROUND, "PlayerMPFrame", mp_frame_obj);
    UIManager::instance()->add_ui(UILayer::MIDDLE, "PlayerMPBar", mp_bar_obj);

    // 3.사망 ui 배경
    auto death_ui_background_obj = ObjectManager::instance()->create_game_object("death_ui_background");
    auto death_ui_background = death_ui_background_obj->add_component<UIRenderComponent>();

    death_ui_background->set_screen_position(0.0f, 0.0f);        // Frame보다 안쪽
    death_ui_background->set_size(FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);// Frame보다 작게
    death_ui_background->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
    death_ui_background->set_texture("Resource/UI/TX_BG.dds");
    UIManager::instance()->add_ui(UILayer::MIDDLE, "Death_Background_UI", death_ui_background_obj);
    UIManager::instance()->set_visible(UILayer::MIDDLE, "Death_Background_UI", false); // 처음에는 보이지 않도록 설정

    // 4. 사망 ui
    auto death_ui_obj = ObjectManager::instance()->create_game_object("death_ui");
    auto death_ui = death_ui_obj->add_component<UIRenderComponent>();

    float scaleX = (float)(FRAME_BUFFER_WIDTH) / 1920.f; // x값 기준 배율

    float scaleY = (float)(FRAME_BUFFER_HEIGHT) / 512.f; // y값 기준 배율

    death_ui->set_screen_position(0.0f, 250.0f);        // Frame보다 안쪽
    death_ui->set_size(1920.f * scaleX, 512.f * scaleX);// Frame보다 작게
    death_ui->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
    death_ui->set_texture("Resource/UI/die_ui (1).dds");
    UIManager::instance()->add_ui(UILayer::FRONT, "Death_UI", death_ui_obj);
    UIManager::instance()->set_visible(UILayer::FRONT, "Death_UI", false); // 처음에는 보이지 않도록 설정

    // 5.로고
    auto _logo_ui_background_obj = ObjectManager::instance()->create_game_object("logo_ui");
    auto logo_ui_background = _logo_ui_background_obj->add_component<UIRenderComponent>();

    logo_ui_background->set_screen_position(FRAME_BUFFER_WIDTH - 410.0f, 0.0f);        // Frame보다 안쪽
    logo_ui_background->set_size( 412.5f, 250.f);// Frame보다 작게
    logo_ui_background->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
    logo_ui_background->set_texture("Resource/UI/game_title_alpha.dds");
    UIManager::instance()->add_ui(UILayer::MIDDLE, "UI_Background_UI", _logo_ui_background_obj);

	// 6. 상호작용 F 키 UI
    auto interact_ui_obj = ObjectManager::instance()->create_game_object("interact_ui");
    auto interact_ui = interact_ui_obj->add_component<UIRenderComponent>();
    interact_ui->set_screen_position(FRAME_BUFFER_WIDTH / 2.0f - 100.f, FRAME_BUFFER_HEIGHT - 150.f); // 위치는 나중에 지정해 줄것임
	interact_ui->set_size(100.f, 100.f); // 크기도 나중에 조정할 것임
    interact_ui->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
    interact_ui->set_texture("Resource/UI/F_interaction_UI.dds");
    UIManager::instance()->add_ui(UILayer::MIDDLE, "F_interaction_UI", interact_ui_obj);
	UIManager::instance()->set_visible(UILayer::MIDDLE, "F_interaction_UI", false); // 처음에는 보이지 않도록 설정
}

void Main_Scene::Spawn_Monster_HP_UI(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    auto monster_hp_frame_obj = ObjectManager::instance()->create_game_object("Monster_HP_Frame");
    auto monster_hp_ui_renderer = monster_hp_frame_obj->add_component<MonsterHPUIRenderComponent>();
    monster_hp_ui_renderer->set_hp_back_texture("Resource/UI/HP_Bar_Frame.dds");
    monster_hp_ui_renderer->set_hp_bar_texture("Resource/UI/HP_Bar.dds");
}


void Main_Scene::TestMesh(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    {
        auto T1 = ObjectManager::instance()->create_game_object("TestMesh");

        //// RenderComponent
        auto renderer = T1->add_component<RenderComponent>();

        auto T1_Mesh = ResourceManager::instance()->load_mesh("Resource/Test_glTF/DamagedHelmet.gltf");
        renderer->set_mesh(T1_Mesh);

        // 재질 및 쉐이더 설정
        std::string material = "Test_Material";

        ResourceManager::instance()->create_material(material);
        ResourceManager::instance()->set_shader_for_material(material, "gltf");

        // gltf
        renderer->set_pso_name("gltf");

        // 위치, 회전 정보
        T1->transform()->set_local_rotation(0.f, 120.f, 0.f);
        T1->transform()->set_local_scale({ 30.f, 30.0f, 30.0f });


        T1->transform()->set_local_position(XMFLOAT3(0.f, 100.f, -0.f));
    }
}
