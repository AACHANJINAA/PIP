#include "stdafx.h"
#include "Title_Scene.h"
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

void Title_Scene::build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // 카메라 생성
    auto cameraObject = ObjectManager::instance()->create_game_object("FreeCamera");
    cameraObject->add_component<FreeCameraScript>();
    cameraObject->set_layer("Camera");
    cameraObject->transform()->set_local_position(XMFLOAT3(0.0f, 500.0f, 10.0f));
    cameraObject->transform()->set_local_rotation(90.0f, 0.0f, 0.0f); // 약간 아래 보기

    auto cameraComp = cameraObject->add_component<CameraComponent>();
    cameraComp->set_main_camera();

    Spawn_UI(device, commandList);
}

void Title_Scene::release_upload_buffers()
{
   
}

void Title_Scene::scene_process(float deltaTime)
{
    // 씬 업데이트 로직 (필요시)
}

void Title_Scene::Spawn_UI(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // titel scene
    auto title_ui_background_obj = ObjectManager::instance()->create_game_object("title_ui");
    auto title_ui_background = title_ui_background_obj->add_component<UIRenderComponent>();

    title_ui_background->set_screen_position(0.0f, 0.0f);        // Frame보다 안쪽
    title_ui_background->set_size(FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);// Frame보다 작게
    title_ui_background->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
    title_ui_background->set_texture("Resource/UI/Title_UI.dds");
    UIManager::instance()->add_ui(UILayer::BACKGROUND, "Title_UI", title_ui_background_obj);

    // logo
    auto logo_ui_background_obj = ObjectManager::instance()->create_game_object("logo_ui");
    auto logo_ui_background = logo_ui_background_obj->add_component<UIRenderComponent>();

    logo_ui_background->set_screen_position(FRAME_BUFFER_WIDTH - 410.0f, 0.0f);        // Frame보다 안쪽
    logo_ui_background->set_size(412.5f, 250.f);// Frame보다 작게
    logo_ui_background->set_color(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));  // 흰색
    logo_ui_background->set_texture("Resource/UI/game_title_alpha.dds");
    UIManager::instance()->add_ui(UILayer::MIDDLE, "Logo_UI", logo_ui_background_obj);
}
