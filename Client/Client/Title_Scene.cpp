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
}

void Title_Scene::release_upload_buffers()
{
   
}

void Title_Scene::scene_process(float deltaTime)
{
    // 씬 업데이트 로직 (필요시)
}