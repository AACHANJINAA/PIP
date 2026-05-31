#include "stdafx.h"
#include "Boss_Scene.h"

#include "CameraComponent.h"
#include "FreeCameraScript.h"
#include "GameObject.h"
#include "NetworkManager.h"
#include "ObjectManager.h"
#include "ReadGLTFMesh.h"
#include "ResourceManager.h"
#include "SceneManager.h"
#include "ShadowManager.h"
#include "TransformComponent.h"
#include "UIFrameRenderComponent.h"
#include "UIManager.h"

void Boss_Scene::build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    ShadowManager::instance()->set_shadow_max_distance(600.0f);
	SceneManager::instance()->build_skybox(device, commandList,
		"Resource/SkyBox/",
		"farmland/farmland_skybox.dds",
		"farmland/farmland_specular.dds",
		"farmland/farmland_diffuse.txt",
		"BRDF.dds");

	// =========================필요한 메시 로드==================================
	//ResourceManager::instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");
	//ResourceManager::instance()->load_mesh("Resource/Character/Brute_Walk/Brute_Walk.gltf", true, "walk");
	ResourceManager::instance()->load_mesh("Resource/Character/BoneGolem/BoneGolem.gltf", true);
	ResourceManager::instance()->load_mesh("Resource/Character/BoneGolem/BoneGolemRd.gltf", true);
	ResourceManager::instance()->load_mesh("Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf", true);
    ResourceManager::instance()->load_mesh("Resource/Elevator/Elevator.gltf", false);
	// =========================================================================

	load_scene_from_file("Resource/1-BossScene/Boss_Landscape_ExportedClientData.json", device, commandList);

    // TestMesh(device, commandList);

	// 카메라 생성 (이름을 "Camera"로 통일)
	auto cameraObject = ObjectManager::instance()->create_game_object("Camera");
	cameraObject->add_component<FreeCameraScript>();
	cameraObject->set_layer("Camera");
	cameraObject->transform()->set_local_position(XMFLOAT3(0.0f, 1.0f, 0.0f));
	cameraObject->transform()->set_local_rotation(90.0f, 0.0f, 0.0f); // 약간 아래 보기

	auto cameraComp = cameraObject->add_component<CameraComponent>();
	cameraComp->set_main_camera();
    
    Spawn_UI(device, commandList);
}

void Boss_Scene::release_upload_buffers()
{
	CLOG("Boss_Scene: Releasing upload buffers");
}

void Boss_Scene::scene_process(float deltaTime)
{
	// 씬 업데이트 로직 (필요시)
}
void Boss_Scene::Spawn_UI(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // 1. HP Frame (뒤에 렌더링될 프레임)
    auto hp_frame_obj = ObjectManager::instance()->create_game_object("HP_Frame");
    auto hp_frame = hp_frame_obj->add_component<UIFrameRenderComponent>();

    hp_frame->set_screen_position(30.0f, 30.0f);      // 화면 왼쪽 상단
    hp_frame->set_size(410.0f, 30.0f);                 // Bar보다 좀 더 큼
    hp_frame->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색 (텍스처 원본 색)
    hp_frame->set_texture("Resource/UI/HP_Bar_Frame.dds");
    UIManager::instance()->add_ui(UILayer::BACKGROUND, "PlayerHPFrame", hp_frame_obj);

    // 2. HP Bar (앞에 렌더링될 바)
    auto hp_bar_obj = ObjectManager::instance()->create_game_object("HP_Bar");
    auto hp_bar = hp_bar_obj->add_component<UIRenderComponent>();

    hp_bar->set_screen_position(42.0f, 38.0f);        // Frame보다 안쪽
    hp_bar->set_size(390.0f, 14.0f);                   // Frame보다 작게
    hp_bar->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
    hp_bar->set_texture("Resource/UI/HP_Bar.dds");
    UIManager::instance()->add_ui(UILayer::MIDDLE, "PlayerHPBar", hp_bar_obj);

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
    logo_ui_background->set_size(412.5f, 250.f);// Frame보다 작게
    logo_ui_background->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
    logo_ui_background->set_texture("Resource/UI/game_title_alpha.dds");
    UIManager::instance()->add_ui(UILayer::MIDDLE, "UI_Background_UI", _logo_ui_background_obj);

}


void Boss_Scene::TestMesh(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    {
        auto T1 = ObjectManager::instance()->create_game_object("TestMesh");

        //// RenderComponent
        auto renderer = T1->add_component<RenderComponent>();

        auto T1_Mesh = ResourceManager::instance()->load_mesh("Resource/Elevator/Elevator.gltf");
        renderer->set_mesh(T1_Mesh);

        // 재질 및 쉐이더 설정
        std::string material = "Test_Material";

        ResourceManager::instance()->create_material(material);
        ResourceManager::instance()->set_shader_for_material(material, "gltf");

        // gltf
        renderer->set_pso_name("gltf");

        // 위치, 회전 정보
        T1->transform()->set_local_rotation(0.f, 0.f, 0.f);
        T1->transform()->set_local_scale({ 3.f, 3.0f, 3.0f });


        T1->transform()->set_local_position(XMFLOAT3(0.f, 10.f, 0.f));
    }
}