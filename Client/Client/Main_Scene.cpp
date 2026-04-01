#include "stdafx.h"
#include "Main_Scene.h"
#include "SceneManager.h"

#include "FreeCameraScript.h"
#include "ObjectManager.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ResourceManager.h"
#include "CameraComponent.h"
#include "MonsterHPUIRenderComponent.h"
#include "ReadGLTFMesh.h"
#include "UIFrameRenderComponent.h"
#include "UIManager.h"
#include "UIRenderComponent.h"

void Main_Scene::build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	// 1. Skybox 로드 (모든 Scene 공통)
	SceneManager::instance()->build_skybox_if_needed(device, commandList);

	// 2. MainScene 전용 Landscape 로드
	SceneManager::instance()->build_main_landscapes(device, commandList);

	// =========================필요한 메시 로드==================================
	ResourceManager::instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");
	ResourceManager::instance()->load_mesh("Resource/Character/DragonBrute/SK_DragonBrute.gltf",true);
	ResourceManager::instance()->load_mesh("Resource/Character/Brute_Walk/Brute_Walk.gltf", true, "walk");
	ResourceManager::instance()->load_mesh("Resource/Character/BoneGolem/BoneGolem.gltf", true);
	ResourceManager::instance()->load_mesh("Resource/Character/BoneGolem/BoneGolemRd.gltf", true);
	ResourceManager::instance()->load_mesh("Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf", true);
	auto idle_brute_mesh = ResourceManager::instance()->load_mesh("Resource/Character/Brute_idle/Brute_idle.gltf", true, "idle");
	dynamic_pointer_cast<ReadGLTFMesh>(idle_brute_mesh)->load_animation_only("Resource/Character/Brute_Attack_animation/Brute_Attack_animation.gltf", "attack");
	// =========================================================================

    // 오두막
	load_scene_from_file("Resource/MainLandscape_Meshes/LandscapeStreamingProxy_-1_-1_0_MapData/LandscapeStreamingProxy_-1_-1_0_ExportedClientData.json", device, commandList);

	// 카메라 생성
	auto cameraObject = ObjectManager::instance()->create_game_object("FreeCamera");
	cameraObject->add_component<FreeCameraScript>();
	cameraObject->set_layer("Camera");
	cameraObject->transform()->set_local_position(XMFLOAT3(0.0f, 500.0f, 10.0f));
	cameraObject->transform()->set_local_rotation(90.0f, 0.0f, 0.0f); // 약간 아래 보기

	auto cameraComp = cameraObject->add_component<CameraComponent>();
	cameraComp->set_main_camera();

	Spawn_UI(device, commandList);
    Spawn_Monster_HP_UI(device, commandList);
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
    hp_bar->set_size(385.0f, 14.0f);                   // Frame보다 작게
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


}

void Main_Scene::Spawn_Monster_HP_UI(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    auto monster_hp_frame_obj = ObjectManager::instance()->create_game_object("Monster_HP_Frame");
    auto monster_hp_ui_renderer = monster_hp_frame_obj->add_component<MonsterHPUIRenderComponent>();
    monster_hp_ui_renderer->set_hp_back_texture("Resource/UI/HP_Bar_Frame.dds");
    monster_hp_ui_renderer->set_hp_bar_texture("Resource/UI/HP_Bar.dds");
}