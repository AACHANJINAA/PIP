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
#include "ReadGLTFMesh.h"
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
    cameraObject->add_component<FreeCameraScript>();
    cameraObject->set_layer("Camera");

    auto cameraComp = cameraObject->add_component<CameraComponent>();
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
        auto black_background_obj = ObjectManager::instance()->create_game_object("black_background");
        auto black_background = black_background_obj->add_component<UIRenderComponent>();

        black_background->set_screen_position(0.0f, 0.0f);        // Frame보다 안쪽
        black_background->set_size(FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);// Frame보다 작게
        black_background->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 원본 색상
        black_background->set_texture("Resource/UI/just_black_background.dds");
        UIManager::instance()->add_ui(UILayer::BACKGROUND, "Black_Background_UI", black_background_obj);
    }

    {
        // titel scene
        _title_ui_obj = ObjectManager::instance()->create_game_object("title_ui");
        auto title_ui_background = _title_ui_obj->add_component<UIRenderComponent>();

        title_ui_background->set_screen_position(0.0f, 0.0f);        // Frame보다 안쪽
        title_ui_background->set_size(FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);// Frame보다 작게
        title_ui_background->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f));  // 흰색
        title_ui_background->set_texture("Resource/UI/PIP_GAMES.dds");
        UIManager::instance()->add_ui(UILayer::MIDDLE, "Title_UI", _title_ui_obj);
    }

    // 우측 상당 logo
    //auto logo_ui_background_obj = ObjectManager::instance()->create_game_object("logo_ui");
    //auto logo_ui_background = logo_ui_background_obj->add_component<UIRenderComponent>();

    //logo_ui_background->set_screen_position(FRAME_BUFFER_WIDTH - 410.0f, 0.0f);        // Frame보다 안쪽
    //logo_ui_background->set_size(412.5f, 250.f);// Frame보다 작게
    //logo_ui_background->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
    //logo_ui_background->set_texture("Resource/UI/game_title_alpha.dds");
    //UIManager::instance()->add_ui(UILayer::MIDDLE, "Logo_UI", logo_ui_background_obj);
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

void Title_Scene::Spawn_Resource(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // 1. Skybox 로드 (모든 Scene 공통)
    SceneManager::instance()->build_skybox(device, commandList,
        "Resource/SkyBox/",
        "farmland/farmland_skybox.dds",
        "farmland/farmland_specular.dds",
        "farmland/farmland_diffuse.txt",
        "BRDF.dds");

    // 2. MainScene 전용 Landscape 로드
    SceneManager::instance()->build_main_landscapes(device, commandList);

    // 3. 미니맵 활성화 -> 지형 이후에 호출해야함
    //SceneManager::instance()->build_minimap(device, commandList);

    // =========================필요한 메시 로드==================================
    ResourceManager::instance()->load_mesh("Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf", true);
    // =========================================================================

    // 오두막
    load_scene_from_file("Resource/MainLandscape_Meshes/Landscape_-1_-1_MapData/Landscape_-1_-1_ExportedClientData.json", device, commandList);

    // 성
    //load_scene_from_file("Resource/MainLandscape_Meshes/Landscape_-1_0_MapData/Landscape_-1_0_ExportedClientData.json", device, commandList);
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

void Title_Scene::Opening_UI_Sequence(float deltaTime)
{
    static float alpha = 0.0f;
    alpha += deltaTime * 0.5f; // 10초 동안 페이드 인
    _title_ui_obj->get_component<UIRenderComponent>()->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, alpha));
    if (alpha >= 1.0f)
    {
        alpha = 1.0f;
        _isOpeningUIEnd = true; // 오프닝 연출이 끝났음을 표시
    }
    else
    {
        return;
    }

	_currentOpeningState = TITLE_SCENE_STATE::OPENING_SEQUENCE; // 오프닝 씬으로 상태 변경
    alpha = 0.f;
}

void Title_Scene::Resource_Loading_Sequence(float deltaTime)
{
    static int frameWait = 0;
    frameWait++;

    // 창이 생성되고 최소 2프레임은 Present 되어야 검은 화면이 모니터에 나옵니다.
    if (frameWait == 1)
    {
        GameFramework::instance()->WaitForGpuComplete();

        auto device = GameFramework::instance()->device().Get();
        auto cmdQueue = GameFramework::instance()->command_queue().Get();
        auto cmdAlloc = GameFramework::instance()->command_allocator().Get();
        auto cmdList = GameFramework::instance()->command_list().Get();

        cmdAlloc->Reset();
        cmdList->Reset(cmdAlloc, nullptr);

        // [1] 무거운 리소스 로딩 시작 (이때 모니터는 완벽한 검은 화면 유지됨)
        Spawn_Resource(device, cmdList);

        cmdList->Close();
        ID3D12CommandList* ppCommandLists[] = { cmdList };
        cmdQueue->ExecuteCommandLists(1, ppCommandLists);
        GameFramework::instance()->WaitForGpuComplete();

        // [2] 로딩이 끝난 직후 음악 재생!
        SoundManager::instance()->load_sound("TitleBgm", "Resource/Sound/monster_hunter_ost.mp3", false);
        SoundManager::instance()->play("TitleBgm", SoundType::BGM, 1.0f, true);

        frameWait = 0;
        // [3] 로딩 완료 후 다음 상태(로고 페이드 인)로 전환
        _currentOpeningState = TITLE_SCENE_STATE::OPENING_UI_SEQUENCE;
    }
}

void Title_Scene::Opening_Sequence(float deltaTime)
{
	// 오프닝 연출 로직 구현 (예: 타이틀 화면 애니메이션, 페이드 인/아웃 등)



    _currentOpeningState = TITLE_SCENE_STATE::CONNECTING_SERVER;
}
