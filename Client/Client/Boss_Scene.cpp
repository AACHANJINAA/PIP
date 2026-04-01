#include "stdafx.h"
#include "Boss_Scene.h"

#include "CameraComponent.h"
#include "FreeCameraScript.h"
#include "GameObject.h"
#include "ObjectManager.h"
#include "ReadGLTFMesh.h"
#include "ResourceManager.h"
#include "SceneManager.h"
#include "TransformComponent.h"

void Boss_Scene::build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{

	SceneManager::instance()->build_skybox(device, commandList, 
		"Resource\\SkyBox\\", 
		"night_field\\night_field_skybox.dds", 
		"night_field\\night_field_diffsue.dds", 
		"night_field\\night_field_specular.dds",
		"IBL_BRDF_LUT.dds");

	//SceneManager::instance()->build_terrain(device, commandList);
	
	// =========================필요한 메시 로드==================================
	ResourceManager::instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");
	ResourceManager::instance()->load_mesh("Resource/Character/Brute_Walk/Brute_Walk.gltf", true, "walk");
	ResourceManager::instance()->load_mesh("Resource/Character/BoneGolem/BoneGolem.gltf", true);
	ResourceManager::instance()->load_mesh("Resource/Character/BoneGolem/BoneGolemRd.gltf", true);
	ResourceManager::instance()->load_mesh("Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf", true, "idle");
	auto idle_brute_mesh = ResourceManager::instance()->load_mesh("Resource/Character/Brute_idle/Brute_idle.gltf", true, "idle");
	dynamic_pointer_cast<ReadGLTFMesh>(idle_brute_mesh)->load_animation_only("Resource/Character/Brute_Attack_animation/Brute_Attack_animation.gltf", "attack");
	// =========================================================================

	load_scene_from_file("Resource/1-BossScene/ExportedClientData.json", device, commandList);

	// 카메라 생성
	auto cameraObject = ObjectManager::instance()->create_game_object("FreeCamera");
	cameraObject->add_component<FreeCameraScript>();
	cameraObject->set_layer("Camera");
	cameraObject->transform()->set_local_position(XMFLOAT3(0.0f, 500.0f, 10.0f));
	cameraObject->transform()->set_local_rotation(90.0f, 0.0f, 0.0f); // 약간 아래 보기

	auto cameraComp = cameraObject->add_component<CameraComponent>();
	cameraComp->set_main_camera();
}

void Boss_Scene::release_upload_buffers()
{
	CLOG("Boss_Scene: Releasing upload buffers");
}

void Boss_Scene::scene_process(float deltaTime)
{
	// 씬 업데이트 로직 (필요시)
}