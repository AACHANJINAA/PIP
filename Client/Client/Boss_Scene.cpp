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
#include "UIRenderComponent.h"
#include "MonsterHPUIRenderComponent.h"
#include "SoundManager.h"

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
    Spawn_Monster_HP_UI(device, commandList);

	// [사운드] 보스 BGM 재생
	SoundManager::instance()->load_sound("BossBGM", "Resource/Sound/BossBGM.mp3", false);
	SoundManager::instance()->play("BossBGM", SoundType::BGM, 0.7f, true);
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
	// 0. player id별 텍스쳐  
	{
		auto name_img_obj = ObjectManager::instance()->create_game_object("Player_Name_Image");
		auto name_renderer = name_img_obj->add_component<UIRenderComponent>();

		name_renderer->set_screen_position(5.0f, 5.0f);
		name_renderer->set_size(200.f, 200.f);
		name_renderer->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

		long long my_id = NetworkManager::instance()->get_my_session_id();
		std::string resource_name = "Resource/UI/ID/Player_" + std::to_string(my_id % 4 + 1) + ".dds";
		name_renderer->set_texture(resource_name);
		UIManager::instance()->add_ui(UILayer::MIDDLE, "PlayerNameImage", name_img_obj);
	}
	// 1. HP Frame (뒤에 렌더링될 프레임) & HP Bar (앞에 렌더링될 체력바)
	auto hp_frame_obj = ObjectManager::instance()->create_game_object("HP_Frame");
	auto hp_frame = hp_frame_obj->add_component<UIRenderComponent>();
	auto hp_bar_obj = ObjectManager::instance()->create_game_object("HP_Bar");
	auto hp_bar = hp_bar_obj->add_component<UIRenderComponent>();

	std::pair<float, float> hp_bar_pos = { 200.0f , 50.0f };
	std::pair<float, float> hp_bar_size = { 410.0f , 26.0f };

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
	logo_ui_background->set_size(412.5f, 250.f);// Frame보다 작게
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

	// 11. boss UI
	{
		// 보스 HP 바 설정값 (비율 유지)
		float boss_w = 1000.0f;
		float boss_h = 44.0f;

		// 화면 하단 중앙 계산
		float posX = FRAME_BUFFER_WIDTH / 2.0f - boss_w / 2.0f;
		float posY = FRAME_BUFFER_HEIGHT - 100.0f;

		// 보스 HP 프레임 (하단 중앙)
		auto boss_hp_frame_obj = ObjectManager::instance()->create_game_object("Boss_HP_Frame");
		auto boss_hp_frame = boss_hp_frame_obj->add_component<UIRenderComponent>();
		boss_hp_frame->set_screen_position(posX, posY);
		boss_hp_frame->set_size(boss_w, boss_h);
		boss_hp_frame->set_texture("Resource/UI/HP_Bar_Frame.dds");
		UIManager::instance()->add_ui(UILayer::BACKGROUND, "Boss_HP_Frame", boss_hp_frame_obj);
		UIManager::instance()->set_visible(UILayer::BACKGROUND, "Boss_HP_Frame", false);

		// 보스 HP 바 (플레이어 바의 오프셋/사이즈 축소 비율을 1000/410으로 확장 적용)
		auto boss_hp_bar_obj = ObjectManager::instance()->create_game_object("Boss_HP_Bar");
		auto boss_hp_bar = boss_hp_bar_obj->add_component<UIRenderComponent>();
		boss_hp_bar->set_screen_position(posX + 27.0f, posY + 10.0f);
		boss_hp_bar->set_size(944.0f, boss_h - 22.0f);
		boss_hp_bar->set_texture("Resource/UI/HP_Bar.dds");
		UIManager::instance()->add_ui(UILayer::MIDDLE, "Boss_HP_Bar", boss_hp_bar_obj);
		UIManager::instance()->set_visible(UILayer::MIDDLE, "Boss_HP_Bar", false);
	}
}

void Boss_Scene::Spawn_Monster_HP_UI(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	auto monster_hp_frame_obj = ObjectManager::instance()->create_game_object("Monster_HP_Frame");
	auto monster_hp_ui_renderer = monster_hp_frame_obj->add_component<MonsterHPUIRenderComponent>();
	monster_hp_ui_renderer->set_hp_back_texture("Resource/UI/HP_Bar_Frame.dds");
	monster_hp_ui_renderer->set_hp_bar_texture("Resource/UI/HP_Bar.dds");
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