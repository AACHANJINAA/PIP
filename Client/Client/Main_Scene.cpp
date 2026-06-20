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
#include "LeverScript.h"
#include "MainPlayerScript.h"
#include "OtherPlayerScript.h"
#include "ParticleSystemComponent.h"
#include "ParticleRenderComponent.h"
#include "SocketComponenet.h"
#include "ReadGLTFMesh.h"
#include "NetworkManager.h"

void Main_Scene::build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    ShadowManager::instance()->set_shadow_max_distance(250.0f);
    // 효과음 테스트
	// 테스트용 Dust 사운드 끄기 요청
	// SoundManager::instance()->load_sound("Swing", "Resource/Sound/Dust.wav", true);
	// SoundManager::instance()->play_3d("Swing", {-360,10,-212},SoundType::SFX,1.0f,true);


	// 1. Skybox 로드 (모든 Scene 공통)
	SceneManager::instance()->build_skybox(device, commandList,
		"Resource/SkyBox/",
		"cloudy/cloudy_skybox.dds",
		"cloudy/cloudy_specular.dds",
		"diffuse.txt",
		"BRDF.dds");

	// 2. MainScene 전용 Landscape 로드
	SceneManager::instance()->build_main_landscapes(device, commandList);
	SceneManager::instance()->generate_grass_for_all_landscapes("Grass", "Resource/Foliage/SM_Grass_01.gltf");
	SceneManager::instance()->generate_grass_for_all_landscapes("Rock", "Resource/Foliage/SM_Dead_grass_01.gltf");

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
	ResourceManager::instance()->load_mesh("Resource/Character/SK_MagicConstruct/SK_MagicConstruct.gltf", true);
	ResourceManager::instance()->load_mesh("Resource/Character/Bandit_Rd_NPC/Bandit_Rd_NPC.gltf",true);
	ResourceManager::instance()->load_mesh("Resource/Lever/Lever.gltf", true);
	auto idle_brute_mesh = ResourceManager::instance()->load_mesh("Resource/Character/Brute_idle/Brute_idle.gltf", true, "idle");
	dynamic_pointer_cast<ReadGLTFMesh>(idle_brute_mesh)->load_animation_only("Resource/Character/Brute_Attack_animation/Brute_Attack_animation.gltf", "attack");
	// =========================================================================

	// 그 외
	load_scene_from_file("Resource/MainLandscape_Meshes/Landscape_0_0_MapData/Landscape_0_0_ExportedClientData.json", device, commandList);
	load_scene_from_file("Resource/MainLandscape_Meshes/Landscape_0_-1_MapData/Landscape_0_-1_ExportedClientData.json", device, commandList);
	//load_foliage_from_file("Resource/Foliage/Foliage_tree_0_0_MapData/Foliage_tree_0_0_MapData.json", device, commandList);

	// 오두막
	load_scene_from_file("Resource/MainLandscape_Meshes/Landscape_-1_-1_MapData/Landscape_-1_-1_ExportedClientData.json", device, commandList);
	//load_foliage_from_file("Resource/Foliage/Foliage_stone_-1_-1_MapData/Foliage_stone_-1_-1_MapData.json", device, commandList);
	//load_foliage_from_file("Resource/Foliage/Foliage_tree_-1_-1_MapData/Foliage_tree_-1_-1_MapData.json", device, commandList);

	// 성
	load_scene_from_file("Resource/MainLandscape_Meshes/Landscape_-1_0_MapData/Landscape_-1_0_ExportedClientData.json", device, commandList);
	//load_foliage_from_file("Resource/Foliage/Foliage_tree_-1_0_MapData/Foliage_tree_-1_0_MapData.json", device, commandList);

	// 성당
	load_scene_from_file("Resource/MainLandscape_Meshes/Landscape_-2_-1_MapData/Landscape_-2_-1_ExportedClientData.json", device, commandList);

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
	spawn_ui_and_object(device, commandList);
	Spawn_Monster_HP_UI(device, commandList);
	//TestMesh(device, commandList);
	Spawn_Lever(device, commandList);

	//load_from_file_with_light("Resource/LeverAndPosition/SelectedMeshes_ClientData.json", device, commandList);
	ResourceManager::instance()->load_mesh("Resource/LeverAndPosition/Meshes/Cube_5E5A4B61.gltf", false);

	std::string path = "../../Common/World_Batch_glTF/Tile_X-1_Y-1/Tile_X-1_Y-1 Server Export Data.json";
	DebugDrawManager::instance()->LoadLocalDebugShape(path, "BP_house_03_Optimized15", "SM_House_Village_03_Merged");

	// [사운드] 전역 BGM 재생
	SoundManager::instance()->load_sound("MainBGM", "Resource/Sound/MainBGM.mp3", false);
	SoundManager::instance()->play("MainBGM", SoundType::BGM, 0.5f, true);
}

void Main_Scene::release_upload_buffers()
{
	CLOG("Main_Scene: Releasing upload buffers");
}

void Main_Scene::scene_process(float deltaTime)
{
	// 씬 업데이트 로직 (필요시)
	/*if (InputManager::instance()->IsKeyDown(VK_F10))
	{
		common::packet::CS_PACKET_DEBUG_COMMAND debug_pkt;
		debug_pkt._type = common::packet::PacketType::C2S_P_DEBUG_COMMAND;
		debug_pkt._size = sizeof(debug_pkt);
		debug_pkt._command = common::packet::DebugCommandType::CHANGE_SCENE_BOSS;
		NetworkManager::instance()->send_packet(reinterpret_cast<const char*>(&debug_pkt), sizeof(debug_pkt));
	}*/

	if (InputManager::instance()->IsKeyDown(VK_F9)) // 디버깅
	{
		set_cinematic_mode(true);
	}

	if (true == _isCinematicMode)
	{
		cinematic_sequence(deltaTime);
	}
}

void Main_Scene::set_cinematic_mode(bool isCinematic)
{
	if (_isCinematicMode == isCinematic) return;

	_isCinematicMode = isCinematic;
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

	// [추가] Q 가이드 UI (HP Bar 좌측에 렌더링)
	auto q_guide_obj = ObjectManager::instance()->create_game_object("PlayerQGuide_UI");
	auto q_guide = q_guide_obj->add_component<UIRenderComponent>();
	q_guide->set_screen_position(200.0f, 110.0f); // MP Bar 좌측 끝 아래
	q_guide->set_size(36.0f, 36.0f);
	q_guide->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	q_guide->set_texture("Resource/UI/Q_interaction_UI_OFF.png"); // 기본 UI 텍스처
	ResourceManager::instance()->load_texture("Resource/UI/Q_interaction_UI_ON.png", true); // ON 텍스처 미리 로드
	UIManager::instance()->add_ui(UILayer::MIDDLE, "PlayerQGuide_UI", q_guide_obj);
	UIManager::instance()->set_visible(UILayer::MIDDLE, "PlayerQGuide_UI", false); // 처음에는 숨김

	// [추가] E 가이드 UI (Q 가이드 우측에 렌더링)
	auto e_guide_obj = ObjectManager::instance()->create_game_object("PlayerEGuide_UI");
	auto e_guide = e_guide_obj->add_component<UIRenderComponent>();
	e_guide->set_screen_position(240.0f, 110.0f);
	e_guide->set_size(36.0f, 36.0f);
	e_guide->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	e_guide->set_texture("Resource/UI/E_interaction_UI_OFF.png");
	ResourceManager::instance()->load_texture("Resource/UI/E_interaction_UI_ON.png", true); // ON 텍스처 미리 로드
	UIManager::instance()->add_ui(UILayer::MIDDLE, "PlayerEGuide_UI", e_guide_obj);
	UIManager::instance()->set_visible(UILayer::MIDDLE, "PlayerEGuide_UI", true); // 항상 보이도록

	// [추가] 조작법 UI (화면 중앙)
	auto controls_ui_obj = ObjectManager::instance()->create_game_object("Controls_UI_Main");
	auto controls_ui = controls_ui_obj->add_component<UIRenderComponent>();
	controls_ui->set_screen_position((FRAME_BUFFER_WIDTH - 800.0f) / 2.0f, (FRAME_BUFFER_HEIGHT - 600.0f) / 2.0f);
	controls_ui->set_size(800.0f, 600.0f);
	controls_ui->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	controls_ui->set_texture("Resource/UI/Controls_UI.png");
	UIManager::instance()->add_ui(UILayer::FRONT, "Controls_UI_Main", controls_ui_obj);
	UIManager::instance()->set_visible(UILayer::FRONT, "Controls_UI_Main", false); // 처음에 숨김

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
	{
		auto interact_ui_obj = ObjectManager::instance()->create_game_object("interact_ui");
		auto interact_ui = interact_ui_obj->add_component<UIRenderComponent>();
		interact_ui->set_screen_position(FRAME_BUFFER_WIDTH / 2.0f - 100.f, FRAME_BUFFER_HEIGHT - 150.f); // 위치는 나중에 지정해 줄것임
		interact_ui->set_size(100.f, 100.f); // 크기도 나중에 조정할 것임
		interact_ui->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
		interact_ui->set_texture("Resource/UI/F_interaction_UI.dds");
		UIManager::instance()->add_ui(UILayer::MIDDLE, "F_interaction_UI", interact_ui_obj);
		UIManager::instance()->set_visible(UILayer::MIDDLE, "F_interaction_UI", false); // 처음에는 보이지 않도록 설정
	}
	{
		auto interact_ui_obj = ObjectManager::instance()->create_game_object("Lever_interact_ui_0");
		auto interact_ui = interact_ui_obj->add_component<UIRenderComponent>();
		interact_ui->set_screen_position(FRAME_BUFFER_WIDTH / 2.0f - 100.f, FRAME_BUFFER_HEIGHT - 150.f); // 위치는 나중에 지정해 줄것임
		interact_ui->set_size(40.f, 40.f); // 크기도 나중에 조정할 것임
		interact_ui->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
		interact_ui->set_texture("Resource/UI/F_interaction_UI.dds");
		UIManager::instance()->add_ui(UILayer::MIDDLE, "Lever_interact_ui_0", interact_ui_obj);
		UIManager::instance()->set_visible(UILayer::MIDDLE, "Lever_interact_ui_0", false); // 처음에는 보이지 않도록 설정
	}
	{
		auto interact_ui_obj = ObjectManager::instance()->create_game_object("Lever_interact_ui_1");
		auto interact_ui = interact_ui_obj->add_component<UIRenderComponent>();
		interact_ui->set_screen_position(FRAME_BUFFER_WIDTH / 2.0f - 100.f, FRAME_BUFFER_HEIGHT - 150.f); // 위치는 나중에 지정해 줄것임
		interact_ui->set_size(40.f, 40.f); // 크기도 나중에 조정할 것임
		interact_ui->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
		interact_ui->set_texture("Resource/UI/F_interaction_UI.dds");
		UIManager::instance()->add_ui(UILayer::MIDDLE, "Lever_interact_ui_1", interact_ui_obj);
		UIManager::instance()->set_visible(UILayer::MIDDLE, "Lever_interact_ui_1", false); // 처음에는 보이지 않도록 설정
	}


	// 7. 퀘스트 마커 UI (?/!)
	auto quest_marker_obj = ObjectManager::instance()->create_game_object("quest_marker_ui");
	auto quest_marker_ui = quest_marker_obj->add_component<UIRenderComponent>();
	quest_marker_ui->set_screen_position(0.f, 0.f); 
	quest_marker_ui->set_size(40.f, 40.f); 
	quest_marker_ui->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 노란색으로 ? 표시 느낌
	quest_marker_ui->set_texture("Resource/UI/Quest_Question_UI.png");
	UIManager::instance()->add_ui(UILayer::MIDDLE, "QuestMarker_UI", quest_marker_obj);
	UIManager::instance()->set_visible(UILayer::MIDDLE, "QuestMarker_UI", false);
	ResourceManager::instance()->load_texture("Resource/UI/Quest_Exclamation_UI.png", true);

    // 8. 퀘스트 배경 UI (검은색 반투명 그라데이션)
    auto quest_bg_obj = ObjectManager::instance()->create_game_object("quest_bg_ui");
    auto quest_bg_ui = quest_bg_obj->add_component<UIRenderComponent>();
    float bg_w = 400.f;
    float bg_h = 100.f;
    quest_bg_ui->set_screen_position(0.f, FRAME_BUFFER_HEIGHT / 2.0f - bg_h / 2.0f);
    quest_bg_ui->set_size(bg_w, bg_h);
    quest_bg_ui->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.8f)); // 반투명
    quest_bg_ui->set_texture("Resource/UI/Quest_BG.png");
    UIManager::instance()->add_ui(UILayer::BACKGROUND, "QuestBanner_UI", quest_bg_obj); // 이름은 그대로 유지해서 로직 안깨지게 함
    UIManager::instance()->set_visible(UILayer::BACKGROUND, "QuestBanner_UI", false);

    // 8-1. 퀘스트 고정 타이틀 글씨 (마을 주변 몬스터 제거)
    auto quest_title_obj = ObjectManager::instance()->create_game_object("quest_title_ui");
    auto quest_title_ui = quest_title_obj->add_component<UIRenderComponent>();
    float title_w = 250.f;
    float title_h = 50.f;
    quest_title_ui->set_screen_position(20.f, FRAME_BUFFER_HEIGHT / 2.0f - title_h / 2.0f - 15.f);
    quest_title_ui->set_size(title_w, title_h);
    quest_title_ui->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    quest_title_ui->set_texture("Resource/UI/Quest_Title_1.png");
    UIManager::instance()->add_ui(UILayer::MIDDLE, "QuestTitle_UI", quest_title_obj);
    UIManager::instance()->set_visible(UILayer::MIDDLE, "QuestTitle_UI", false);
	ResourceManager::instance()->load_texture("Resource/UI/Quest_Title_2.png", true);
	ResourceManager::instance()->load_texture("Resource/UI/Quest_Reward.png", true); // [추가]

	// [추가] 퀘스트 보상 알림 (화면 중앙 배너)
	auto quest_reward_obj = ObjectManager::instance()->create_game_object("quest_reward_ui");
	auto quest_reward_ui = quest_reward_obj->add_component<UIRenderComponent>();
	float reward_w = 1200.f;
	float reward_h = 112.f;
	quest_reward_ui->set_screen_position(FRAME_BUFFER_WIDTH / 2.0f - reward_w / 2.0f, FRAME_BUFFER_HEIGHT / 2.0f - 150.f);
	quest_reward_ui->set_size(reward_w, reward_h);
	quest_reward_ui->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	quest_reward_ui->set_texture("Resource/UI/Quest_Reward.png");
	UIManager::instance()->add_ui(UILayer::FRONT, "QuestRewardBanner_UI", quest_reward_obj);
	UIManager::instance()->set_visible(UILayer::FRONT, "QuestRewardBanner_UI", false);

	// [추가] 퀘스트 시작 연출용 "도와줘!!" UI와 "스토리 보드" UI 등록
	{
		// 1. 도와줘 UI
		auto help_ui_obj = ObjectManager::instance()->create_game_object("Help_Me_UI");
		auto help_ui = help_ui_obj->add_component<UIRenderComponent>();
		float help_posX = FRAME_BUFFER_WIDTH / 2.0f - 400.0f;
		float help_posY = FRAME_BUFFER_HEIGHT / 2.0f - 100.0f;
		help_ui->set_screen_position(help_posX, help_posY);
		help_ui->set_size(800.f, 200.f);
		help_ui->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f));
		help_ui->set_texture("Resource/UI/Help_Me.png");
		UIManager::instance()->add_ui(UILayer::MIDDLE, "Help_Me_UI", help_ui_obj);
		UIManager::instance()->set_visible(UILayer::MIDDLE, "Help_Me_UI", false);

		// 2. 퀘스트 스토리 보드 UI
		auto story_ui_obj = ObjectManager::instance()->create_game_object("Quest_Story_UI");
		auto story_ui = story_ui_obj->add_component<UIRenderComponent>();
		float story_posX = FRAME_BUFFER_WIDTH / 2.0f - 475.0f;
		float story_posY = FRAME_BUFFER_HEIGHT / 2.0f - 265.0f;
		story_ui->set_screen_position(story_posX, story_posY);
		story_ui->set_size(950.f, 530.f);
		story_ui->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		story_ui->set_texture("Resource/UI/Quest_Story.png");
		UIManager::instance()->add_ui(UILayer::MIDDLE, "Quest_Story_UI", story_ui_obj);
		UIManager::instance()->set_visible(UILayer::MIDDLE, "Quest_Story_UI", false);
	}

    // 9. 퀘스트 텍스트(진행도) UI (00/00 등 총 5자리)
    float num_w = 20.f;
    float num_h = 30.f;
    float start_x = 20.f; // 타이틀 바로 아래에 배치
    float start_y = FRAME_BUFFER_HEIGHT / 2.0f + 10.f; // 중앙보다 살짝 아래
    for (int i = 0; i < 5; ++i) {
        std::string name = "QuestNumber_" + std::to_string(i);
        auto num_obj = ObjectManager::instance()->create_game_object(name);
        auto num_ui = num_obj->add_component<UIRenderComponent>();
        num_ui->set_screen_position(start_x + i * (num_w * 0.8f), start_y);
        num_ui->set_size(num_w, num_h);
        num_ui->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
        num_ui->set_texture("Resource/UI/Quest_Numbers.png");
        num_ui->set_uv_scale(1.0f / 11.0f, 1.0f);
        UIManager::instance()->add_ui(UILayer::MIDDLE, name, num_obj);
        UIManager::instance()->set_visible(UILayer::MIDDLE, name, false);
    }

	// 10. 파티 UI
	// --- 파티 UI 설정 ---
	float party_ui_scale = 0.7f;                    // 기존 0.5f에서 0.7f로 상향 (전체적인 크기 증가)
	float icon_base_size = 150.0f;                  // 아이콘 기본 크기 상향
	float icon_size = icon_base_size * party_ui_scale; // 실제 적용 크기 

	float party_screen_startX = 20.0f;               // 좌측 여백 살짝 줄임
	float party_screen_startY = 120.0f;              // 시작 높이 살짝 위로
	float party_slot_total_gap = 80.0f;              // 슬롯 간 간격 (크기가 커졌으니 간격도 넓힘)

	// 바의 시작 위치 (아이콘 오른쪽)
	float bar_startX = party_screen_startX + icon_size + 15.0f;
	float bar_startY = 0;

	for (int i = 1; i < 4; ++i) {
		std::string idx_str = std::to_string(i);
		float current_y = party_screen_startY + (i * party_slot_total_gap);
		bar_startY = current_y + 20.0f;

		// 1. ID 아이콘 (크기 체감 확 되도록 조정)
		auto icon_obj = ObjectManager::instance()->create_game_object("PartyIDIcon_" + idx_str);
		auto icon_comp = icon_obj->add_component<UIRenderComponent>();
		icon_comp->set_screen_position(party_screen_startX, current_y);
		icon_comp->set_size(icon_size, icon_size);
		icon_comp->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		icon_comp->set_texture("Resource/UI/ID/Player_1.dds");
		icon_comp->set_texture("Resource/UI/ID/Player_2.dds");
		icon_comp->set_texture("Resource/UI/ID/Player_3.dds");
		icon_comp->set_texture("Resource/UI/ID/Player_4.dds");
		UIManager::instance()->add_ui(UILayer::MIDDLE, "PartyIDIcon_" + idx_str, icon_obj);

		// 2. HP 프레임 & 바 (크기 키움)
		float party_hp_w = 410.0f * party_ui_scale; // 약 287px
		float party_hp_h = 26.0f * party_ui_scale;  // 약 18px

		auto party_hp_frame_obj = ObjectManager::instance()->create_game_object("PartyHPFrame_" + idx_str);
		auto party_hp_frame_comp = party_hp_frame_obj->add_component<UIRenderComponent>();
		party_hp_frame_comp->set_screen_position(bar_startX, bar_startY);
		party_hp_frame_comp->set_size(party_hp_w, party_hp_h);
		party_hp_frame_comp->set_texture("Resource/UI/HP_Bar_Frame.dds");
		UIManager::instance()->add_ui(UILayer::BACKGROUND, "PartyHPFrame_" + idx_str, party_hp_frame_obj);

		auto party_hp_bar_obj = ObjectManager::instance()->create_game_object("PartyHP_" + idx_str);
		auto party_hp_bar_comp = party_hp_bar_obj->add_component<UIRenderComponent>();
		// 여백도 스케일에 맞게 조정 (기존 11, 4의 0.7배)
		party_hp_bar_comp->set_screen_position(bar_startX + (11.0f * party_ui_scale), bar_startY + (4.0f * party_ui_scale));
		party_hp_bar_comp->set_size((410.0f - 24.0f) * party_ui_scale, (26.0f - 9.0f) * party_ui_scale);
		party_hp_bar_comp->set_texture("Resource/UI/HP_Bar.dds");
		UIManager::instance()->add_ui(UILayer::MIDDLE, "PartyHP_" + idx_str, party_hp_bar_obj);

		// 3. MP 프레임 & 바 (HP 바로 아래 배치)
		float party_mp_y_offset = party_hp_h + 5.0f; // HP바 바로 아래 5px 여백
		float party_mp_current_y = bar_startY + party_mp_y_offset;

		auto party_mp_frame_obj = ObjectManager::instance()->create_game_object("PartyMPFrame_" + idx_str);
		auto party_mp_frame_comp = party_mp_frame_obj->add_component<UIRenderComponent>();
		party_mp_frame_comp->set_screen_position(bar_startX, party_mp_current_y);
		party_mp_frame_comp->set_size(party_hp_w, party_hp_h);
		party_mp_frame_comp->set_texture("Resource/UI/HP_Bar_Frame.dds");
		UIManager::instance()->add_ui(UILayer::BACKGROUND, "PartyMPFrame_" + idx_str, party_mp_frame_obj);

		auto party_mp_bar_obj = ObjectManager::instance()->create_game_object("PartyMP_" + idx_str);
		auto party_mp_bar_comp = party_mp_bar_obj->add_component<UIRenderComponent>();
		party_mp_bar_comp->set_screen_position(bar_startX + (11.0f * party_ui_scale), party_mp_current_y + (4.0f * party_ui_scale));
		party_mp_bar_comp->set_size((410.0f - 24.0f) * party_ui_scale, (26.0f - 9.0f) * party_ui_scale);
		party_mp_bar_comp->set_texture("Resource/UI/MP_Bar.dds");
		UIManager::instance()->add_ui(UILayer::MIDDLE, "PartyMP_" + idx_str, party_mp_bar_obj);

		// --- 생성 즉시 모든 요소 숨기기 ---
		UIManager::instance()->set_visible(UILayer::MIDDLE, "PartyIDIcon_" + idx_str, false);
		UIManager::instance()->set_visible(UILayer::BACKGROUND, "PartyHPFrame_" + idx_str, false);
		UIManager::instance()->set_visible(UILayer::MIDDLE, "PartyHP_" + idx_str, false);
		UIManager::instance()->set_visible(UILayer::BACKGROUND, "PartyMPFrame_" + idx_str, false);
		UIManager::instance()->set_visible(UILayer::MIDDLE, "PartyMP_" + idx_str, false);

		// 슬롯 정보 등록
		UIManager::instance()->init_party_slots(i, party_hp_bar_comp, party_mp_bar_comp, icon_comp);
	}
}

void Main_Scene::Spawn_Monster_HP_UI(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	auto monster_hp_frame_obj = ObjectManager::instance()->create_game_object("Monster_HP_Frame");
	auto monster_hp_ui_renderer = monster_hp_frame_obj->add_component<MonsterHPUIRenderComponent>();
	monster_hp_ui_renderer->set_hp_back_texture("Resource/UI/HP_Bar_Frame.dds");
	monster_hp_ui_renderer->set_hp_bar_texture("Resource/UI/HP_Bar.dds");
}

void Main_Scene::Spawn_Lever(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	{
		auto lever_obj = ObjectManager::instance()->create_game_object("Lever0"); // 건물 뒷 편
		lever_obj->add_component<LeverScript>();

		XMFLOAT3 pos_lever1 = {124.9f, 7.7f, -170.5f};

		// 위치, 회전 정보
		lever_obj->transform()->set_local_rotation(-90.f, 0.f, -90.f);
		lever_obj->transform()->set_local_scale({ 0.5f, 0.5f, 0.5f });
		lever_obj->transform()->set_local_position(pos_lever1);

		Light spotLight;
		spotLight.m_bEnable = true;
		spotLight.m_nType = 2; // SPOT_LIGHT 
		spotLight.m_vPosition = { pos_lever1.x, pos_lever1.y + 7.0f, pos_lever1.z };
		spotLight.m_vDirection = { 0.0f, -1.0f, 0.0f };
		spotLight.m_cDiffuse = { 2.0f, 4.0f, 2.0f, 1.0f };
		spotLight.m_fRange = 800.0f;
		spotLight.m_vAttenuation = { 1.0f, 0.001f, 0.0001f };
		spotLight.m_fTheta = cosf(XMConvertToRadians(15.0f));
		spotLight.m_fPhi = cosf(XMConvertToRadians(30.0f));
		spotLight.m_fFalloff = 1.0f;

		LightManager::instance()->add_light(std::move(spotLight));
	}

	{
		auto lever_obj = ObjectManager::instance()->create_game_object("Lever1"); // 나무 있는 곳
		lever_obj->add_component<LeverScript>();

		XMFLOAT3 pos_lever2 = { 148.5f, 7.5f, -35.6f };

		// 위치, 회전 정보
		lever_obj->transform()->set_local_rotation(-90.f, -90.f, -90.f);
		lever_obj->transform()->set_local_scale({ 0.5f, 0.5f, 0.5f });
		lever_obj->transform()->set_local_position(pos_lever2);

		Light spotLight;
		spotLight.m_bEnable = true;
		spotLight.m_nType = 2; // SPOT_LIGHT 
		spotLight.m_vPosition = { pos_lever2.x, pos_lever2.y + 7.0f, pos_lever2.z };
		spotLight.m_vDirection = { 0.0f, -1.0f, 0.0f };
		spotLight.m_cDiffuse = { 2.0f, 4.0f, 2.0f, 1.0f };
		spotLight.m_fRange = 800.0f;
		spotLight.m_vAttenuation = { 1.0f, 0.001f, 0.0001f };
		spotLight.m_fTheta = cosf(XMConvertToRadians(15.0f));
		spotLight.m_fPhi = cosf(XMConvertToRadians(30.0f));

		LightManager::instance()->add_light(std::move(spotLight));
	}
}


void Main_Scene::TestMesh(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	{
		auto T1 = ObjectManager::instance()->create_game_object("TestMesh");

		//// RenderComponent
		auto renderer = T1->add_component<RenderComponent>();
		auto animation = T1->add_component<AnimationComponent>();


		auto T1_Mesh = ResourceManager::instance()->load_mesh("Resource/Lever/Lever.gltf", true);
		renderer->set_mesh(T1_Mesh);

		dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only("Resource/Lever/Animation/Lever_UP.gltf", "UP");
		animation->add_animation("idle",T1_Mesh,"UP");
		animation->play("idle", true);

		// 재질 및 쉐이더 설정
		std::string material = "Test_Material";

		ResourceManager::instance()->create_material(material);
		ResourceManager::instance()->set_shader_for_material(material, "skinned");

		// skinned
		renderer->set_pso_name("skinned");

		// 위치, 회전 정보
		T1->transform()->set_local_rotation(0.f, 120.f, 0.f);
		T1->transform()->set_local_scale({ 70.0f, 70.0f, 70.0f });


		T1->transform()->set_local_position(XMFLOAT3(0.f, 100.f, -0.f));
	}

	{
		auto T1 = ObjectManager::instance()->create_game_object("TestMesh");

		//// RenderComponent
		auto renderer = T1->add_component<RenderComponent>();
		//auto animation = T1->add_component<AnimationComponent>();


		auto T1_Mesh = ResourceManager::instance()->load_mesh("Resource/Lever/Lever.gltf");
		renderer->set_mesh(T1_Mesh);

		/*dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only("Resource/Lever/Animation/Lever_UP.gltf", "UP");
		animation->add_animation("idle", T1_Mesh, "UP");
		animation->play("idle", true);*/

		// 재질 및 쉐이더 설정
		std::string material = "Test_Material";

		ResourceManager::instance()->create_material(material);
		ResourceManager::instance()->set_shader_for_material(material, "gltf");

		// gltf
		renderer->set_pso_name("gltf");

		// 위치, 회전 정보
		T1->transform()->set_local_rotation(0.f, 100.f, 0.f);
		T1->transform()->set_local_scale({ 70.0f, 70.0f, 70.0f });


		T1->transform()->set_local_position(XMFLOAT3(0.f, 100.f, -0.f));
	}
}

void Main_Scene::spawn_ui_and_object(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	
	int player_id = NetworkManager::instance()->get_my_session_id() % 4;

	int other_player_id_1 = (player_id % 4) + 1; // 다른 플레이어 ID 계산
	int other_player_id_2 = (player_id % 4) + 2; // 다른 플레이어 ID 계산
	int other_player_id_3 = (player_id % 4) + 3; // 다른 플레이어 ID 계산

	// 1. 페이드 아웃 UI 생성
	{
		// black_background
		_blackBackground_ui_obj = ObjectManager::instance()->create_game_object("Cinematic_black_background");
		auto black_background = _blackBackground_ui_obj->add_component<UIRenderComponent>();

		black_background->set_screen_position(0.0f, 0.0f);        // Frame보다 안쪽
		black_background->set_size(FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);// Frame보다 작게
		black_background->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f));  // 원본 색상
		black_background->set_texture("Resource/UI/just_black_background.dds");
		UIManager::instance()->add_ui(UILayer::FRONT, "Black_Background_UI", _blackBackground_ui_obj);
		_blackBackground_ui_obj->set_enabled(false);
	}

	// 기사 모델 1
	{
		auto T1 = ObjectManager::instance()->create_game_object("Cinematic_KnightMesh_1");

		//// RenderComponent
		auto renderer = T1->add_component<RenderComponent>();
		auto animation = T1->add_component<AnimationComponent>();


		std::string animationpath = "Resource/Character/DarkKnight/DKF_animations/";
		// 메시 설정 (애니메이션 포함)
		auto T1_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf", true);
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Idle_Alert.gltf", "idle");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Walk_Alert_Fwd.gltf", "walk");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Run_Alert_Fwd.gltf", "run");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Attack_01.gltf", "attack01");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Skill_01.gltf", "skill01");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Skill_01_end.gltf", "skill01_end");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Death.gltf", "death");
		
		renderer->set_mesh(T1_Mesh);

		// 애니메이션 설정
		animation->add_animation("idle", T1_Mesh, "idle");
		animation->add_animation("walk", T1_Mesh, "walk");
		animation->add_animation("run", T1_Mesh, "run");
		animation->add_animation("attack", T1_Mesh, "attack01");
		animation->add_animation("skill", T1_Mesh, "skill01");
		animation->add_animation("skill_end", T1_Mesh, "skill01_end");
		animation->add_animation("die", T1_Mesh, "death");

		animation->play("idle", true);

		// 재질 및 쉐이더 설정
		std::string material = "Knight_Material";

		ResourceManager::instance()->create_material(material);
		ResourceManager::instance()->set_shader_for_material(material, "skinned");

		// gltf
		renderer->set_pso_name("skinned");

		// 위치, 회전 정보
		T1->transform()->set_local_rotation(0.f, 90.f, 0.f);
		T1->transform()->set_local_scale({ 1.f, 1.0f, 1.0f });


		// T1->transform()->set_local_position(XMFLOAT3(242.4f, 138.0f, 114.5f));
		T1->transform()->set_local_position(XMFLOAT3(194.5f, 5.06f, -59.4f));

		_dummy_player_1 = T1; // 나중에 플레이어 위치로 이동할 때 사용할 더미 플레이어 오브젝트
		_dummy_player_1->set_enabled(false); // 처음에는 비활성화 상태로 시작
	}

	// 기사 모델 2
	{
		auto T1 = ObjectManager::instance()->create_game_object("Cinematic_KnightMesh_2");

		//// RenderComponent
		auto renderer = T1->add_component<RenderComponent>();
		auto animation = T1->add_component<AnimationComponent>();

		std::string animationpath = "Resource/Character/DarkKnight/DKF_animations/";
		// 메시 설정 (애니메이션 포함)
		auto T1_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf", true);
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Idle_Alert.gltf", "idle");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Walk_Alert_Fwd.gltf", "walk");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Run_Alert_Fwd.gltf", "run");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Attack_01.gltf", "attack01");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Skill_01.gltf", "skill01");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Skill_01_end.gltf", "skill01_end");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Death.gltf", "death");

		renderer->set_mesh(T1_Mesh);

		// 애니메이션 설정
		animation->add_animation("idle", T1_Mesh, "idle");
		animation->add_animation("walk", T1_Mesh, "walk");
		animation->add_animation("run", T1_Mesh, "run");
		animation->add_animation("attack", T1_Mesh, "attack01");
		animation->add_animation("skill", T1_Mesh, "skill01");
		animation->add_animation("skill_end", T1_Mesh, "skill01_end");
		animation->add_animation("die", T1_Mesh, "death");

		animation->play("idle", true);

		// 색상설정 (다른 플레이어 색상 적용)
		renderer->set_force_player_color_id(other_player_id_1);

		// 재질 및 쉐이더 설정
		std::string material = "Knight_Material";

		ResourceManager::instance()->create_material(material);
		ResourceManager::instance()->set_shader_for_material(material, "skinned");

		// gltf
		renderer->set_pso_name("skinned");

		// 위치, 회전 정보
		T1->transform()->set_local_rotation(0.f, 0.f, 0.f);
		T1->transform()->set_local_scale({ 1.f, 1.0f, 1.0f });


		// T1->transform()->set_local_position(XMFLOAT3(242.4f, 138.0f, 114.5f));
		T1->transform()->set_local_position(XMFLOAT3(184.3f, 5.06f, -50.7f));

		_dummy_player_2 = T1; // 나중에 플레이어 위치로 이동할 때 사용할 더미 플레이어 오브젝트
		_dummy_player_2->set_enabled(false); // 처음에는 비활성화 상태로 시작
	}

	// 기사 모델 3
	{
		auto T1 = ObjectManager::instance()->create_game_object("Cinematic_KnightMesh_3");

		//// RenderComponent
		auto renderer = T1->add_component<RenderComponent>();
		auto animation = T1->add_component<AnimationComponent>();

		std::string animationpath = "Resource/Character/DarkKnight/DKF_animations/";
		// 메시 설정 (애니메이션 포함)
		auto T1_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf", true);
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Idle_Alert.gltf", "idle");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Walk_Alert_Fwd.gltf", "walk");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Run_Alert_Fwd.gltf", "run");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Attack_01.gltf", "attack01");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Skill_01.gltf", "skill01");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Skill_01_end.gltf", "skill01_end");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Death.gltf", "death");

		renderer->set_mesh(T1_Mesh);

		// 애니메이션 설정
		animation->add_animation("idle", T1_Mesh, "idle");
		animation->add_animation("walk", T1_Mesh, "walk");
		animation->add_animation("run", T1_Mesh, "run");
		animation->add_animation("attack", T1_Mesh, "attack01");
		animation->add_animation("skill", T1_Mesh, "skill01");
		animation->add_animation("skill_end", T1_Mesh, "skill01_end");
		animation->add_animation("die", T1_Mesh, "death");

		animation->play("idle", true);

		// 색상설정 (다른 플레이어 색상 적용)
		renderer->set_force_player_color_id(other_player_id_2);

		// 재질 및 쉐이더 설정
		std::string material = "Knight_Material";

		ResourceManager::instance()->create_material(material);
		ResourceManager::instance()->set_shader_for_material(material, "skinned");

		// gltf
		renderer->set_pso_name("skinned");

		// 위치, 회전 정보
		T1->transform()->set_local_rotation(0.f, -90.f, 0.f);
		T1->transform()->set_local_scale({ 1.f, 1.0f, 1.0f });


		// T1->transform()->set_local_position(XMFLOAT3(242.4f, 138.0f, 114.5f));
		T1->transform()->set_local_position(XMFLOAT3(176.5f, 5.06f, -59.2f));

		_dummy_player_3 = T1; // 나중에 플레이어 위치로 이동할 때 사용할 더미 플레이어 오브젝트
		_dummy_player_3->set_enabled(false); // 처음에는 비활성화 상태로 시작
	}

	// 기사 모델 4
	{
		auto T1 = ObjectManager::instance()->create_game_object("Cinematic_KnightMesh_4");

		//// RenderComponent
		auto renderer = T1->add_component<RenderComponent>();
		auto animation = T1->add_component<AnimationComponent>();

		std::string animationpath = "Resource/Character/DarkKnight/DKF_animations/";
		// 메시 설정 (애니메이션 포함)
		auto T1_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf", true);
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Idle_Alert.gltf", "idle");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Walk_Alert_Fwd.gltf", "walk");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Run_Alert_Fwd.gltf", "run");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Attack_01.gltf", "attack01");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Skill_01.gltf", "skill01");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Skill_01_end.gltf", "skill01_end");
		std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only(animationpath + "Anim_DKF_Death.gltf", "death");

		renderer->set_mesh(T1_Mesh);

		// 애니메이션 설정
		animation->add_animation("idle", T1_Mesh, "idle");
		animation->add_animation("walk", T1_Mesh, "walk");
		animation->add_animation("run", T1_Mesh, "run");
		animation->add_animation("attack", T1_Mesh, "attack01");
		animation->add_animation("skill", T1_Mesh, "skill01");
		animation->add_animation("skill_end", T1_Mesh, "skill01_end");
		animation->add_animation("die", T1_Mesh, "death");

		animation->play("idle", true);

		// 색상설정 (다른 플레이어 색상 적용)
		renderer->set_force_player_color_id(other_player_id_3);

		// 재질 및 쉐이더 설정
		std::string material = "Knight_Material";

		ResourceManager::instance()->create_material(material);
		ResourceManager::instance()->set_shader_for_material(material, "skinned");

		// gltf
		renderer->set_pso_name("skinned");

		// 위치, 회전 정보
		T1->transform()->set_local_rotation(0.f, 180.f, 0.f);
		T1->transform()->set_local_scale({ 1.f, 1.0f, 1.0f });


		// T1->transform()->set_local_position(XMFLOAT3(242.4f, 138.0f, 114.5f));
		T1->transform()->set_local_position(XMFLOAT3(185.3f, 5.06f, -68.6f));

		_dummy_player_4 = T1; // 나중에 플레이어 위치로 이동할 때 사용할 더미 플레이어 오브젝트
		_dummy_player_4->set_enabled(false); // 처음에는 비활성화 상태로 시작
	}

	// 더미 플레이어 무기 및 파티클 세팅
	std::shared_ptr<GameObject> dummies[4] = { _dummy_player_1, _dummy_player_2, _dummy_player_3, _dummy_player_4 };
	int colors_idx[4] = { player_id, other_player_id_1, other_player_id_2, other_player_id_3 };
	static const DirectX::XMFLOAT3 PlayerColors[4] = {
		DirectX::XMFLOAT3(0.863f, 0.078f, 0.235f), // crimson red
		DirectX::XMFLOAT3(0.0f, 1.0f, 0.498f), // spring green
		DirectX::XMFLOAT3(1.0f, 0.843f, 0.0f), // gold
		DirectX::XMFLOAT3(0.541f, 0.169f, 0.886f), // violet
	};

	for (int i = 0; i < 4; ++i) {
		if (!dummies[i]) continue;

		auto socket = dummies[i]->add_component<SocketComponenet>();
		auto currentWeaponObject = socket->add_connecting(
			"Cinematic_Weapon_" + std::to_string(i),
			"hand_r", 
			"Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf",
			{ 0.f,0.f,0.f },   
			{ -10.f, -80.f, -9.0f },       
			{ 10.f, 10.f, 10.f }
		);
		currentWeaponObject->get_component<RenderComponent>()->set_enabled(false);

		auto skillRender = currentWeaponObject->get_component<RenderComponent>();
		auto gltfMesh = std::dynamic_pointer_cast<ReadGLTFMesh>(skillRender->mesh());
		auto targets = gltfMesh->extract_particle_targets(50000);

		auto particleEffectObject = ObjectManager::instance()->create_game_object("Cinematic_Particle_" + std::to_string(i));
		auto psComp = particleEffectObject->add_component<ParticleSystemComponent>();
		
		DirectX::XMFLOAT4 color = { PlayerColors[colors_idx[i] % 4].x, PlayerColors[colors_idx[i] % 4].y, PlayerColors[colors_idx[i] % 4].z, 0.5f };
		psComp->init_particles(targets, color, 0.05f); // burst_radius는 기본값(-1.0f) 유지

		auto prComp = particleEffectObject->add_component<ParticleRenderComponent>();
		prComp->set_pso_name("particle_draw");
		prComp->set_particle_system(psComp);

		particleEffectObject->transform()->set_local_position({ 0, 0, 0 });
		particleEffectObject->transform()->set_parent(currentWeaponObject->transform());
		particleEffectObject->set_enabled(false); 

		_dummy_player_weapons[i] = currentWeaponObject;
		_dummy_player_particles[i] = particleEffectObject;
	}

	// 분수대
	{
		auto T1 = ObjectManager::instance()->create_game_object("Cinematic_fountain");

		//// RenderComponent
		auto renderer = T1->add_component<RenderComponent>();

		// 메시 설정 (애니메이션 미포함)
		auto T1_Mesh = ResourceManager::instance()->load_mesh("Resource/LeverAndPosition/Meshes/SM_fountain_01_1B971041.gltf");
		renderer->set_mesh(T1_Mesh);

		// 색상설정 (없음)
		

		// 애니메이션 설정 (없음)

		// 재질 및 쉐이더 설정
		std::string material = "fountain_Material";

		ResourceManager::instance()->create_material(material);
		ResourceManager::instance()->set_shader_for_material(material, "gltf");

		// gltf
		renderer->set_pso_name("gltf");
		renderer->set_enabled(false); // 처음에는 분수대 메시 렌더링 끔 (파티클로만 연출할 예정)

		// 위치, 회전 정보
		T1->transform()->set_local_rotation(0.f, 0.f, 0.f);
		T1->transform()->set_local_scale({ 1.0f, 1.0f, 1.0f });


		// T1->transform()->set_local_position(XMFLOAT3(242.4f, 138.0f, 114.5f));
		T1->transform()->set_local_position(XMFLOAT3(185.1f, 4.86f, -59.5f));

		// 파티클 준비
		_dummy_particle_fountain = ObjectManager::instance()->create_game_object("Cinematic_Particle_fountain");

		auto fountainParticleMesh = dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh);

		auto targets = fountainParticleMesh->extract_particle_targets(300000);

		_dummy_particle_fountain->set_enabled(false); // 처음에는 비활성화 상태로 시작

		auto psComp = _dummy_particle_fountain->add_component<ParticleSystemComponent>();

		DirectX::XMFLOAT4 color = { 1.f,1.f,1.f,1.f };

		psComp->init_particles(targets, color, 0.05f, 15.0f);

		// 4. 렌더 컴포넌트 추가
		auto prComp = _dummy_particle_fountain->add_component<ParticleRenderComponent>();
		prComp->set_pso_name("particle_draw");

		prComp->set_particle_system(psComp);

		// 5. 위치 동기화
		_dummy_particle_fountain->transform()->set_local_scale({ 1.5f, 1.5f, 1.5f });
		_dummy_particle_fountain->transform()->set_local_position({ 185.1f, 4.86f, -59.5f });

		// 초기에는 꺼둠
		_dummy_particle_fountain->set_enabled(false);
	}

	// 노래
	SoundManager::instance()->load_sound("Cinematic_fountain_BGM", "Resource/Sound/Monster_Hunter_World_OST_Journey_to_the_Truth.mp3", false);
}

void Main_Scene::cinematic_sequence(float deltaTime)
{
	// 디버깅을 위해서 바로 넘김
	_isCutsceneDoneSent = true;

	// 컷씬에 필요한 변수들 준비
	float bgm_time = SoundManager::instance()->get_playback_position("Cinematic_fountain_BGM");
	auto psComp = _dummy_particle_fountain->get_component<ParticleSystemComponent>();

	// 컷씬 시작
	_blackBackground_ui_obj->set_enabled(true); // 페이드 아웃 UI 활성화

	// 0~3초동안 페이드 아웃 및 소리 서서히 끄기
	_cinematicTimer += deltaTime;
	if (_cinematicTimer <= 3.0f)
	{
		_blackBackground_ui_obj->get_component<UIRenderComponent>()->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, _cinematicTimer / 3.0f));
		SoundManager::instance()->set_master_volume(1.0f - (_cinematicTimer / 3.0f));
	}

	// 3~4초동안 완전히 검은 화면 유지 및 소리 완전히 끄고 컷씬에 필요한 설정 적용
	else if (_cinematicTimer > 3.0f && _cinematicTimer <= 4.0f)
	{
		_blackBackground_ui_obj->get_component<UIRenderComponent>()->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		SoundManager::instance()->set_master_volume(0.0f);
		SoundManager::instance()->stop_all();

		// 컷씬에 필요한 설정 적용 (예: 카메라 위치, 플레이어 위치 등)
		_dummy_player_1->set_enabled(true);
		_dummy_player_2->set_enabled(true);
		_dummy_player_3->set_enabled(true);
		_dummy_player_4->set_enabled(true);
		// 아직 파티클 렌더링은 끈 상태로 시작 (19초부터 켤 예정)
		fountain_particle_progress = 0.0f;
		_dummy_particle_fountain->set_enabled(false);


		// 음악 재생
		if(!SoundManager::instance()->is_playing("Cinematic_fountain_BGM"))
		{
			SoundManager::instance()->play("Cinematic_fountain_BGM", SoundType::BGM, 1.f, false);
		}


		// 완성되면 카메라 시네마틱 모드 키기
		/*auto cameraObject = ObjectManager::instance()->find_by_name("Camera");
		if (cameraObject)
		{
			if (auto camescript = cameraObject->get_component<FreeCameraScript>())
				camescript->set_sinamatic_camera_mode(true);
		}*/
	}

	// 여기서부터 노래 시간을 기준으로 컷씬 연출 진행
	// 0~3초동안 다시 페이드 인 및 소리 서서히 켜기
	if (bgm_time > 0.0f && bgm_time <= 3.0f)
	{
		_blackBackground_ui_obj->get_component<UIRenderComponent>()->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f - ((bgm_time - 0.0f) / 3.0f)));
		SoundManager::instance()->set_master_volume((bgm_time - 0.0f) / 3.0f);
	}
	// 3~19초 카메라가 플레이어 주변을 쭉 돌기(지정된 점을 지나는 베지어 곡선 형태로)
	else if (bgm_time > 3.0f && bgm_time <= 19.0f)
	{

	}
	// 16~21초 원 점점 퍼지기
	else if (bgm_time > 16.0f && bgm_time <= 21.0f)
	{
		_dummy_particle_fountain->set_enabled(true); // 분수대 메시 렌더링 켜기
		// 진행도는 이 시간동안 0~0.3f까지 증가
		fountain_particle_progress = (bgm_time - 16.0f) / 5.0f * 0.3f; // 0~0.3f
		psComp->set_compute_data(_dummy_particle_fountain->transform()->world_matrix(), _dummy_particle_fountain->transform()->local_position(), fountain_particle_progress);
	}
	// 21초~28초부터 분수 스폰 후 모여들기
	else if (bgm_time > 21.0f && bgm_time <= 28.0f)
	{
		// 진행도는 이 시간동안 0.3f ~ 1.f까지 증가
		fountain_particle_progress = 0.3f + ((bgm_time - 21.0f) / 7.0f * 0.7f); // 0.3f ~ 1.f
		psComp->set_compute_data(_dummy_particle_fountain->transform()->world_matrix(), _dummy_particle_fountain->transform()->local_position(), fountain_particle_progress);
	}
	// 28~31초 분수 유지 및 플레이어에게 라이팅 집중
	else if (bgm_time > 28.0f && bgm_time <= 31.0f)
	{

	}
	// 31~35초 분수 사라지기
	else if (bgm_time > 31.0f && bgm_time <= 35.0f)
	{
		fountain_particle_progress = 1.0f - ((bgm_time - 31.0f) / 4.0f); // 1.f ~ 0.f
		psComp->set_particle_dying(true);
		psComp->set_compute_data(_dummy_particle_fountain->transform()->world_matrix(), _dummy_particle_fountain->transform()->local_position(), fountain_particle_progress);
	}
	// 35~50초 플레이어 스킬 사용
	else if (bgm_time > 35.0f && bgm_time <= 50.0f)
	{
		float skill_progress = 1.0f;

		// 35초 진입 시점: 한 번만 애니메이션 실행
		if (bgm_time > 35.0f && !_isCinematicSkillPlayed) 
		{
			_isCinematicSkillPlayed = true;
			std::shared_ptr<GameObject> dummies[4] = { _dummy_player_1, _dummy_player_2, _dummy_player_3, _dummy_player_4 };
			for (int i = 0; i < 4; ++i) {
				if (dummies[i]) {
					dummies[i]->get_component<AnimationComponent>()->play("skill", false, 0.5f);
					dummies[i]->get_component<AnimationComponent>()->set_pause_at_progress(6.f / 27.f);
				}
			}
		}

		// 1. 35~40초: 스킬 파티클 모으기
		if (bgm_time <= 40.0f) {
			skill_progress = (bgm_time - 35.0f) / 5.0f; // 0.0 ~ 1.0
		}
		// 42~46초: 검 휘두르기
		else if (bgm_time > 42.0f && bgm_time <= 46.0f) {
			skill_progress = 1.0f;
		}
		// 46~50초: 파티클 소멸 (분수처럼 1.0 -> 0.0으로 진행도를 줄임)
		else if (bgm_time > 46.0f) {
			std::shared_ptr<GameObject> dummies[4] = { _dummy_player_1, _dummy_player_2, _dummy_player_3, _dummy_player_4 };
			for (int i = 0; i < 4; ++i) {
				if (dummies[i]) {
					// 소멸할 때는 따라가지 않도록 설정
					dummies[i]->get_component<SocketComponenet>()->set_isFollowAnimation(false);
				}
			}
			skill_progress = 1.0f - ((bgm_time - 46.0f) / 4.0f); // 1.0 ~ 0.0
		}

		// 40초 진입 시점: 일시정지 해제
		if (bgm_time > 40.0f && !_isCinematicParticleDying) { // 재사용 변수 대체 로직 (추후 수정)
			std::shared_ptr<GameObject> dummies[4] = { _dummy_player_1, _dummy_player_2, _dummy_player_3, _dummy_player_4 };
			for (int i = 0; i < 4; ++i) {
				if (dummies[i]) {
					// 40초를 넘긴 경우 resume이 반복 호출되지 않게 조치 필요 (여기선 단순 유지)
					dummies[i]->get_component<AnimationComponent>()->resume(0.8f);
				}
			}
		}

		// 46초 진입 시점: 파티클 소멸 시작 및 애니메이션 변경
		if (bgm_time > 46.0f && !_isCinematicParticleDying) {
			_isCinematicParticleDying = true;
			std::shared_ptr<GameObject> dummies[4] = { _dummy_player_1, _dummy_player_2, _dummy_player_3, _dummy_player_4 };
			for (int i = 0; i < 4; ++i) {
				if (dummies[i]) {
					dummies[i]->get_component<AnimationComponent>()->play("skill_end", false, 0.8f);
				}
				if (_dummy_player_particles[i]) {
					auto ps = _dummy_player_particles[i]->get_component<ParticleSystemComponent>();
					ps->set_particle_dying(true);
				}
			}
		}

		// 매 프레임 업데이트: 파티클 활성화 및 무기 위치 추적
		std::shared_ptr<GameObject> dummies_pos[4] = { _dummy_player_1, _dummy_player_2, _dummy_player_3, _dummy_player_4 };
		for (int i = 0; i < 4; ++i) {
			if (_dummy_player_particles[i] && _dummy_player_weapons[i] && dummies_pos[i]) {
				_dummy_player_particles[i]->set_enabled(true);
				auto ps = _dummy_player_particles[i]->get_component<ParticleSystemComponent>();
				
				DirectX::XMFLOAT4X4 weapon_world = _dummy_player_weapons[i]->transform()->world_matrix();
				
				DirectX::XMFLOAT3 player_pos = dummies_pos[i]->transform()->local_position();

				ps->set_compute_data(weapon_world, player_pos, skill_progress);
				ps->set_particle_dying(_isCinematicParticleDying);
			}
		}
	}
	// 50~51초 플레이어 idle 상태 만들기
	else if (bgm_time > 50.0f && bgm_time <= 51.0f)
	{
		std::shared_ptr<GameObject> dummies[4] = { _dummy_player_1, _dummy_player_2, _dummy_player_3, _dummy_player_4 };
		for (int i = 0; i < 4; ++i) {
			if (dummies[i]) {
				dummies[i]->get_component<AnimationComponent>()->play("idle", false);
			}
		}
	}

	// 마지막 넘어가기
	if (bgm_time >= 30.0f && !bgm_time)
	{
		//_isCutsceneDoneSent = true;

		
	}

	if (_isCutsceneDoneSent)
	{
		// 모든 사운드 끄기 및 볼륨 원복
		SoundManager::instance()->stop_all();
		SoundManager::instance()->set_master_volume(1.0f);

		// 컷씬 종료 패킷 전송 (단 한 번만)
		NetworkManager::instance()->SendCutsceneDonePacket();
	}

}
