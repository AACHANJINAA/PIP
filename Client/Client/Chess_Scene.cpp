#include "stdafx.h"
#include "Chess_Scene.h"

#include "FreeCameraScript.h"
#include "ObjectManager.h"
#include "GameObject.h"

#include "GltfAnimationScript.h"

#include "TransformComponent.h"
#include "RenderComponent.h"
#include "ResourceManager.h"

#include "TerrainLoader.h"


void Chess_Scene::build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	// =========================필요한 메시 로드==================================

    ResourceManager::instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");

	// =====================================================================



    // 카메라 생성
    auto cameraObject = ObjectManager::instance()->create_game_object("FreeCamera");
    cameraObject->add_component<FreeCameraScript>();
    cameraObject->transform()->set_local_position(XMFLOAT3(0.0f, 50.0f, 10.0f)); // 20m 높이
    cameraObject->transform()->set_local_rotation(10.0f, 0.0f, 0.0f); // 약간 아래 보기

    load_scene_from_file("Resource/DDSMapData/ExportedClientData.json", device, commandList);

	// DW설명 : 브루트 소년단 생성 함수 호출
    SpawnBTS(device, commandList);

	// DW설명 : 그래미 워크 생성 함수 호출
	SpawnGrammy_Walk(device, commandList);

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

}

void Chess_Scene::release_upload_buffers()
{
    ResourceManager::instance()->release_upload_buffers();
}

void Chess_Scene::SpawnBTS(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    float offsetX = 0.0f;
    float offsetY = -50.0f;
    float offsetZ = +200.0f;
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


        hi_brute->transform()->set_local_position(XMFLOAT3(0.0f + offsetX, 25.0f + offsetY, -130.0f + offsetZ));
        ResourceManager::instance()->upload_pending_meshes(device, commandList);
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



        hi_brute->transform()->set_local_position(XMFLOAT3((-100.f + i * 50.f) + offsetX, 50.0f + offsetY, -80.0f + offsetZ));
        ResourceManager::instance()->upload_pending_meshes(device, commandList);
    }
}

void Chess_Scene::SpawnGrammy_Walk(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    float offsetX = 0.0f;
    float offsetY = -50.0f;
    float offsetZ = +200.0f;
    // DW설명 : 인사 애니메이션 오브젝트 생성
    {
        auto hi_brute = ObjectManager::instance()->create_game_object("Grammy_Walk");
        // GltfAnimationScript추가

        hi_brute->add_component<GltfAnimationScript>();
        //// RenderComponent
        auto renderer = hi_brute->add_component<RenderComponent>();

        auto hi_brute_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/Gramma_Walk/Gramma_Walk.gltf", true);
        renderer->set_mesh(hi_brute_Mesh);

        // 재질 및 쉐이더 설정
        std::string material = "skinned_animation_Gramma_Walk";

        ResourceManager::instance()->create_material(material);
        ResourceManager::instance()->set_shader_for_material(material, "skinned");

        // gltf
        renderer->set_pso_name("skinned");

        // 위치, 회전 정보
        hi_brute->transform()->set_local_rotation(0.f, 180.f, 0.f);
        hi_brute->transform()->set_local_scale({ 25.0f, 25.0f, 25.0f });


        hi_brute->transform()->set_local_position(XMFLOAT3(0.0f + offsetX, 25.0f + offsetY, -130.0f + offsetZ));
        ResourceManager::instance()->upload_pending_meshes(device, commandList);
    }
}
