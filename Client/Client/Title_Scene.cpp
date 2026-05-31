#include "stdafx.h"
#include "Title_Scene.h"
#include "SceneManager.h"

#include "resource1.h"


#include "FreeCameraScript.h"
#include "ObjectManager.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ResourceManager.h"
#include "CameraComponent.h"
#include "MonsterHPUIRenderComponent.h"
#include "AnimationComponent.h"
#include "ReadGLTFMesh.h"
#include "InputManager.h"
#include "UIFrameRenderComponent.h"
#include "UIManager.h"
#include "UIRenderComponent.h"
#include "SoundManager.h"
#include "NetworkManager.h"
#include "GameFramework.h"

extern HINSTANCE hInst;

void Title_Scene::build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // 일단 검은 창을 띄우고 로드하도록 수정
    
    // 오프닝 연출에 필요한 리소스 로드 및 캐릭터 생성
    //Spawn_Resource(device, commandList);

    // UI 생성
    Spawn_UI(device, commandList);

    // 카메라 생성
    auto cameraObject = ObjectManager::instance()->create_game_object("Camera");
    auto cameraComp = cameraObject->add_component<CameraComponent>(45.f);
    cameraObject->add_component<FreeCameraScript>();
	cameraObject->transform()->set_local_position({ 239.44f, 138.1f, 112.19f });
	// 처음에는 45도 위쪽을 바라보도록 설정 -> 나중에 오프닝 연출에서 카메라 이동 시켜야할듯
	cameraObject->transform()->set_local_rotation(-45.0f, 195.0f, 0.0f);
    cameraObject->set_layer("Camera");
    cameraComp->set_main_camera();

    // 오디오 재생 -> 리소스 로드 이후에 노래 재생
    //SoundManager::instance()->load_sound("TitleBgm", "Resource/Sound/monster_hunter_ost.mp3", false);
   // SoundManager::instance()->play("TitleBgm", SoundType::BGM, 1.0f, true);

	_currentOpeningState = TITLE_SCENE_STATE::RESOURCE_LOADING; // 리소스 로딩이 끝났으니 다음 상태로 넘어감
}

void Title_Scene::release_upload_buffers()
{
   
}

void Title_Scene::scene_process(float deltaTime)
{
    // 씬 업데이트 로직 (필요시)

    switch (_currentOpeningState)
    {
	case TITLE_SCENE_STATE::RESOURCE_LOADING:
        Resource_Loading_Sequence(deltaTime);
		break;
    case TITLE_SCENE_STATE::OPENING_UI_SEQUENCE:
        Opening_UI_Sequence(deltaTime);
        break;
    case TITLE_SCENE_STATE::OPENING_SEQUENCE:
        Opening_Sequence(deltaTime);
        break;
    case TITLE_SCENE_STATE::CONNECTING_SERVER:
        _isConnectedToServer = InterRoom();
        break;

	case TITLE_SCENE_STATE::CONNECTED:
		SoundManager::instance()->stop("TitleBgm");
        break;
    case TITLE_SCENE_STATE::END:
        break;
    default:
        break;
    }
}

void Title_Scene::Spawn_UI(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    {
		// black_background
        _blackBackground_ui_obj = ObjectManager::instance()->create_game_object("black_background");
        auto black_background = _blackBackground_ui_obj->add_component<UIRenderComponent>();

        black_background->set_screen_position(0.0f, 0.0f);        // Frame보다 안쪽
        black_background->set_size(FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);// Frame보다 작게
        black_background->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 원본 색상
        black_background->set_texture("Resource/UI/just_black_background.dds");
        UIManager::instance()->add_ui(UILayer::BACKGROUND, "Black_Background_UI", _blackBackground_ui_obj);
    }

    {
        // titel scene
        _title_ui_obj = ObjectManager::instance()->create_game_object("title_ui");
        auto title_ui_background = _title_ui_obj->add_component<UIRenderComponent>();

        title_ui_background->set_screen_position(0.0f, 0.0f);        // Frame보다 안쪽
        title_ui_background->set_size(FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);// Frame보다 작게
        title_ui_background->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f));  // 흰색
        title_ui_background->set_texture("Resource/UI/PIP_GAMES_LOGO.dds");
        UIManager::instance()->add_ui(UILayer::MIDDLE, "Title_UI", _title_ui_obj);
    }

    {
        // 우측 상당 logo
        _logo_ui_background_obj = ObjectManager::instance()->create_game_object("logo_ui");
        auto logo_ui_background = _logo_ui_background_obj->add_component<UIRenderComponent>();

        logo_ui_background->set_screen_position(100.0f, 50.0f);        // Frame보다 안쪽
        logo_ui_background->set_size(412.5f * 2, 250.0f * 2);// Frame보다 작게
        logo_ui_background->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f));  // 흰색
        logo_ui_background->set_texture("Resource/UI/game_title_alpha.dds");
        UIManager::instance()->add_ui(UILayer::MIDDLE, "Logo_UI", _logo_ui_background_obj);
    }
}

bool Title_Scene::InterRoom()
{
    if (DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DialogProc, 0) != IDOK)
    {
        // 지금은 취소를 누른 경우 프로그램 종료하도록 해둠
        // 추후에 다시 오프닝 장면을 재생하도록 수정할 수 있다.
        ::PostQuitMessage(0); // 사용자가 취소를 누르면 프로그램 종료
        return false; 
    }

    //NetworkManager::instance()->cleanup_network();

    // 주소구조체 설정 및 서버 연결
    if (!NetworkManager::instance()->connect_to_server())
    {
        //NetworkManager::instance()->cleanup_network();
		::PostQuitMessage(0); // 서버 연결에 실패하면 프로그램 종료
        return false;
    }
    // 최초 로그인 패킷 전송 (플레이어 이름 사용)
    NetworkManager::instance()->SendLoginPacket();

	// 방 목록 요청
    int room_to_enter = 0;
    NetworkManager::instance()->SendEnterRoomPacket(room_to_enter);
	_currentOpeningState = TITLE_SCENE_STATE::CONNECTED; // 서버 연결 후 상태 변경 -> 완료
    return true;
}

void Title_Scene::spawn_resource(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
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
    auto knight_mesh = ResourceManager::instance()->load_mesh("Resource/Character/DarkKnightNoneSword/SKM_DKF_Full.gltf", true);
    // =========================================================================

    spawn_opening_sequence_object();

    // 성
   load_scene_from_file("Resource/MainLandscape_Meshes/Landscape_-1_0_MapData/Landscape_-1_0_ExportedClientData.json", device, commandList, true);
   load_foliage_from_file("Resource/Foliage/Foliage_tree_-1_0_MapData/Foliage_tree_-1_0_MapData.json", device, commandList);
}

void Title_Scene::spawn_opening_sequence_object()
{
    // 앉아있는 기사 모델 소환
    {
        auto T1 = ObjectManager::instance()->create_game_object("KnightMesh");

        //// RenderComponent
        auto renderer = T1->add_component<RenderComponent>();
        auto animation = T1->add_component<AnimationComponent>();

		// 메시 설정 (애니메이션 포함)
        auto T1_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/DarkKnightNoneSword/SKM_DKF_Full.gltf", true);
        dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh)->load_animation_only("Resource/Character/DarkKnightNoneSword/animations/Sit_idle.gltf", "idle");
        renderer->set_mesh(T1_Mesh);

		// 애니메이션 설정
		animation->add_animation("idle", T1_Mesh, "idle");
		animation->play("idle", true);

        // 재질 및 쉐이더 설정
        std::string material = "Knight_Material";

        ResourceManager::instance()->create_material(material);
        ResourceManager::instance()->set_shader_for_material(material, "skinned");

        // gltf
        renderer->set_pso_name("skinned");

        // 위치, 회전 정보
        T1->transform()->set_local_rotation(0.f, 20.f, 0.f);
        T1->transform()->set_local_scale({ 1.f, 1.0f, 1.0f });


        // T1->transform()->set_local_position(XMFLOAT3(242.4f, 138.0f, 114.5f));
        T1->transform()->set_local_position(XMFLOAT3(242.4f, 137.7f, 114.5f));
    }
}

INT_PTR CALLBACK Title_Scene::DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    std::string SERVER_ADDR = "127.0.0.1";
    std::string PLAYER_NAME = "MyPlayer";
    switch (message)
    {
    case WM_INITDIALOG:
        SetDlgItemTextA(hWnd, IDC_EDIT1, SERVER_ADDR.c_str());
        SetDlgItemTextA(hWnd, IDC_EDIT3, PLAYER_NAME.c_str());
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            char buffer1[256];
            char buffer2[256];
            GetDlgItemTextA(hWnd, IDC_EDIT1, buffer1, 256);
            GetDlgItemTextA(hWnd, IDC_EDIT3, buffer2, 256);
            SERVER_ADDR.assign(buffer1);
            PLAYER_NAME.assign(buffer2);
            NetworkManager::instance()->set_name(PLAYER_NAME);
            NetworkManager::instance()->set_server_addr(SERVER_ADDR);
            EndDialog(hWnd, IDOK);
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hWnd, IDCANCEL);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void Title_Scene::Resource_Loading_Sequence(float deltaTime)
{
    static int frameWait = 0;
    frameWait++;

    // 창이 생성되고 최소 5프레임은 Present 되어야 검은 화면이 모니터에 나옴
    if (_isYouWantSeeTitleScene && frameWait == 1)
    {
        GameFramework::instance()->set_fullscreen_toggle(true);
    }
    if (frameWait == 5)
    {
        GameFramework::instance()->WaitForGpuComplete();

        auto device = GameFramework::instance()->device().Get();
        auto cmdQueue = GameFramework::instance()->command_queue().Get();
        auto cmdAlloc = GameFramework::instance()->command_allocator().Get();
        auto cmdList = GameFramework::instance()->command_list().Get();

        cmdAlloc->Reset();
        cmdList->Reset(cmdAlloc, nullptr);

        // [1] 무거운 리소스 로딩 시작 (이때 모니터는 완벽한 검은 화면 유지됨)
        spawn_resource(device, cmdList);

        cmdList->Close();
        ID3D12CommandList* ppCommandLists[] = { cmdList };
        cmdQueue->ExecuteCommandLists(1, ppCommandLists);
        GameFramework::instance()->WaitForGpuComplete();

        // [2] 로딩이 끝난 직후 음악 재생!
        if(_isYouWantSeeTitleScene)
        {
            SoundManager::instance()->load_sound("TitleBgm", "Resource/Sound/Monster Hunter Wilds Main Theme.mp3", false);
        }
        else
        {
			SoundManager::instance()->load_sound("TitleBgm", "Resource/Sound/monster_hunter_ost.mp3", false);
        }
        SoundManager::instance()->play("TitleBgm", SoundType::BGM, 1.0f, true);
        // 사운드가 재생되기 시작하면 오프닝 UI 연출 시작 (검은 화면에서 로고 페이드 인)

        frameWait = 0;
        // [3] 로딩 완료 후 다음 상태(로고 페이드 인)로 전환
        _currentOpeningState = TITLE_SCENE_STATE::OPENING_UI_SEQUENCE;
    }
}


void Title_Scene::Opening_UI_Sequence(float deltaTime)
{
    if (!_isYouWantSeeTitleScene)
    {
        _isOpeningUIEnd = true;
        _currentOpeningState = TITLE_SCENE_STATE::OPENING_SEQUENCE;
		return;
    }

    // 1. 노래가 실제로 재생 중인지 확인
    if (!SoundManager::instance()->is_playing("TitleBgm"))
    {
        return;
    }

    // deltaTime을 더하지 않고, FMOD의 현재 재생 시간(초)을 직접 가져오기
    // 음악이 렉으로 끊기면 UI도 같이 멈추고, 음악이 진행되면 UI도 정확히 그 시간에 맞추기
    float ui_timer = SoundManager::instance()->get_playback_position("TitleBgm");

    //static float ui_timer = 0.0f;
    ui_timer += deltaTime;

    static int uiNum = 1;
    static float alpha = 0.0f;

    if (uiNum == 1) // 첫 번째 UI 연출 (PIP Games 로고)
    {
        // 1. 대기: 0.0초 ~ 4.2초
        if (ui_timer <= 4.2f)
        {
            alpha = 0.0f;
        }
        // 2. 페이드 인 (0.3초 소요): 4.2초 ~ 4.5초
        else if (ui_timer <= 4.5f)
        {
            float duration = 0.3f;
            float progress = (ui_timer - 4.2f) / duration; // 0.0 ~ 1.0 비율
            alpha = progress;
        }
        // 3. 유지: 4.5초 ~ 6.5초
        else if (ui_timer <= 6.5f)
        {
            alpha = 1.0f;
        }
        // 4. 페이드 아웃 (2.0초 소요): 6.5초 ~ 8.5초
        else if (ui_timer <= 8.5f)
        {
            float duration = 2.0f;
            float progress = (ui_timer - 6.5f) / duration; // 0.0 ~ 1.0 비율
            alpha = 1.0f - progress; // 1.0에서 0.0으로 감소
        }
        // 5. 연출 종료 및 상태 전환: 8.5초 이후
        else
        {
            alpha = 0.0f;
            uiNum = 2;       // 두 번째 연출로 넘어감
        }

        // 로고 투명도 적용
        _title_ui_obj->get_component<UIRenderComponent>()->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, alpha));
    }
    else if (uiNum == 2) // 오프닝 시퀀스 종료
    {
        _isOpeningUIEnd = true;
        _currentOpeningState = TITLE_SCENE_STATE::OPENING_SEQUENCE;

        // 씬이 다시 호출될 경우를 대비해 초기화
        ui_timer = 0.0f;
        uiNum = 1;
        alpha = 0.0f;
    }
}



void Title_Scene::Opening_Sequence(float deltaTime)
{
    if(!_isYouWantSeeTitleScene)
    {
        _isOpeningEnd = true;
        _currentOpeningState = TITLE_SCENE_STATE::CONNECTING_SERVER;
		return;
	}


   


	// 오프닝 연출 로직 구현
    
    // 카메라가 하늘을 보다가 천천히 내려오면서 성 보이게 하기

    if (!_isYouWantSeeTitleScene)
    {
        _isOpeningEnd = true;
        _currentOpeningState = TITLE_SCENE_STATE::CONNECTING_SERVER;
        return;
    }

    float bgm_time = SoundManager::instance()->get_playback_position("TitleBgm");

    auto cameraObject = ObjectManager::instance()->find_by_name("Camera");
    if (!cameraObject || !cameraObject->transform()) return;

	cameraObject->get_component<FreeCameraScript>()->set_sinamatic_camera_mode(true); // 오프닝 시퀀스 동안 시네마틱 카메라 모드 활성화


    if (InputManager::instance()->IsKeyDown(VK_F8))
    {
        cameraObject->get_component<FreeCameraScript>()->set_sinamatic_camera_mode(false);
    }


    // --- 카메라 타임라인 설정 ---
    //const float startFadeOutTime = 8.5f; // 페이드 아웃
	const float startTime = 8.5f; // UI 연출 종료 -> 카메라 회전 시작하는 시간
	const float move_start_time = 16.5f; // 움직임 시작하는 시간 -> UI 연출이 끝나고 3초 정도는 카메라 회전만 하다가, 16.5초부터 움직임 시작 -> 이때가 성이 보이는 시점
    const float midTime = 23.4f; // 성 진입
    const float endTime = 32.0f; // 주인공 옆 도착
	const float spawnUIENDTime = 33.0f; // ui 페이트인 끝나는 시간 -> 초에 ui도 다 끝나야함

    // 위치 데이터
    XMFLOAT3 P0 = { 239.44f, 138.1f, 112.19f };
    XMFLOAT3 P1 = { 241.69f, 139.67f, 115.28f };
    XMFLOAT3 P2 = { 239.94f, 141.16f, 116.98f };

	// 각도 데이터 (Z축은 모두 0.0f로 통일)
    XMFLOAT3 R_Base = { -45.0f, 195.0f, 0.0f };

    // 각도 데이터 (Z축은 모두 0.0f로 통일)
   // 기존 R0(140, 18, 180) -> 정상 R0
    XMFLOAT3 R0 = { 30.0f, 195.0f, 0.0f };
    // 기존 R1(150, 5, 180) -> 정상 R1
    XMFLOAT3 R1 = { 32.5f, 175.4f, 0.0f };
    // 기존 R2(-149.7, 19.3, 180) -> 정상 R2
    XMFLOAT3 R2 = { 29.2f, 172.2f, 0.0f };

    XMFLOAT3 targetPos;
    XMFLOAT3 targetRot;

    float alpha{0.f};

    float black_alpha{0.f};
  
	// [구간 0] 카메라 각도 회전 (8.5f ~ 16.5f) 및 배경 페이드 아웃 (8.5f ~ 13.5f)
    if (bgm_time >= startTime && bgm_time < move_start_time)
    {
        const float targetDuration = 5.0f;   // 5초 동안 아주 천천히 (8.5초 ~ 13.5초 구간)

        // 진행률 계산 (현재 오디오 시간 - 8.5초) / 5.0초
        float progress = (bgm_time - startTime) / targetDuration;

        // 탈출 조건: 진행률이 100% (1.0)에 도달하거나 넘었을 때 (즉, 13.5초가 되었을 때)
        if (progress >= 1.0f)
        {
            black_alpha = 0.0f; // 안전하게 0으로 고정
        }
        else
        {
            // 알파값 계산 (1.0에서 0.0으로)
            black_alpha = 1.0f - progress;
        }

        // 배경 투명도 적용
        _blackBackground_ui_obj->get_component<UIRenderComponent>()->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, black_alpha));
        float t = (bgm_time - startTime) / (move_start_time - startTime);
        float smoothT = t * t * (3.0f - 2.0f * t);
        targetPos = P0; // 위치는 고정
        targetRot.x = R_Base.x + (R0.x - R_Base.x) * smoothT;
        targetRot.y = R_Base.y + (R0.y - R_Base.y) * smoothT;
        targetRot.z = 0.f; // Z축 회전 무시 (0으로 고정)
    }
    // [구간 1] 시작점 -> 중간 지점 (16.5s ~ 23.7s)
    else if (bgm_time >= move_start_time && bgm_time < midTime)
    {
        float t = (bgm_time - move_start_time) / (midTime - move_start_time);
        float smoothT = t * t * (3.0f - 2.0f * t);

        targetPos.x = P0.x + (P1.x - P0.x) * smoothT;
        targetPos.y = P0.y + (P1.y - P0.y) * smoothT;
        targetPos.z = P0.z + (P1.z - P0.z) * smoothT;

        targetRot.x = R0.x + (R1.x - R0.x) * smoothT;
        targetRot.y = R0.y + (R1.y - R0.y) * smoothT;
        targetRot.z = 0.f; // Z축 회전 무시 (0으로 고정)
    }
    // [구간 2] 중간 지점 -> 최종 지점 (23.7s ~ 31.0s)
    else if (bgm_time >= midTime && bgm_time <= endTime)
    {
        float t = (bgm_time - midTime) / (endTime - midTime);
        float smoothT = t * t * (3.0f - 2.0f * t);

        targetPos.x = P1.x + (P2.x - P1.x) * smoothT;
        targetPos.y = P1.y + (P2.y - P1.y) * smoothT;
        targetPos.z = P1.z + (P2.z - P1.z) * smoothT;

        auto InterpolateAngle = [](float start, float end, float ratio) {
            float diff = end - start;
            while (diff < -180.0f) diff += 360.0f;
            while (diff > 180.0f)  diff -= 360.0f;
            return start + diff * ratio;
            };

        targetRot.x = InterpolateAngle(R1.x, R2.x, smoothT);
        targetRot.y = InterpolateAngle(R1.y, R2.y, smoothT);
        targetRot.z = 0.f; // Z축 회전 무시 (0으로 고정)
    }
    // [구간 3] UI 페이드 인이 끝나는 시점까지 (31.0s ~ 33.0s) -> 카메라는 최종 지점에 고정된 상태로, UI 페이드 인이 끝나는 시점까지 유지
    else if (bgm_time > endTime && bgm_time <= spawnUIENDTime)
    {
        targetPos = P2;
        targetRot = R2;

		alpha = (bgm_time - endTime) / (spawnUIENDTime - endTime); // 0.0 ~ 1.0
    }
    else if (bgm_time > spawnUIENDTime && bgm_time <= spawnUIENDTime + 8.0f)
    {
        targetPos = P2;
        targetRot = R2;
		alpha = 1.0f; // UI도 완전히 나타난 상태로 고정
        _isOpeningEnd = true;
        _currentOpeningState = TITLE_SCENE_STATE::CONNECTING_SERVER;
        cameraObject->get_component<FreeCameraScript>()->set_sinamatic_camera_mode(false); // 오프닝 시퀀스 동안 시네마틱 카메라 모드 활성화
    }
    else // 연출 시작 전 대기 (13.5초 이전)
    {
        targetPos = P0;
        targetRot = R0;
    }

    if(!_isOpeningEnd)
    {
        cameraObject->transform()->set_local_position(targetPos);
        cameraObject->transform()->set_local_rotation(targetRot.x, targetRot.y, targetRot.z);
    }

    _logo_ui_background_obj->get_component<UIRenderComponent>()->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, alpha));

    // 다 끝나고 넘기기
    {
        //_isOpeningEnd = true;
        //_currentOpeningState = TITLE_SCENE_STATE::CONNECTING_SERVER;
    }
}
