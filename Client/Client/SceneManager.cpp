#include "stdafx.h"
#include "SceneManager.h"

#include "Chess_Scene.h"
#include "Tool_Scene.h"
#include "GameFramework.h"

#include "ObjectManager.h"
#include "ResourceManager.h"

#include "SkyboxMesh.h"
#include "SkyboxRenderComponent.h"
#include "TerrainLoader.h"
#include "TerrainRenderComponent.h"

SceneManager::SceneManager()
{

}

SceneManager::~SceneManager()
{

}

void SceneManager::initialize(ID3D12Device* device, ID3D12GraphicsCommandList* command_list)
{
	build_skybox(device, command_list);
	build_terrain(device, command_list);

	register_scene<Chess_Scene>("ChessScene");
	register_scene<Tool_Scene>("ToolScene");
	//register_scene<Lobby_Scene>("LobbyScene");

	change_scene("ChessScene");
}

void SceneManager::release()
{
	_currentScene.reset();
	_requestedSceneName.clear();
	_scene_creators.clear();
}

void SceneManager::change_scene(const std::string& scene_name)
{
	_requestedSceneName = scene_name;
}

void SceneManager::process_scene_change_if_requested(ID3D12Device* device
	,ID3D12CommandAllocator* command_allocator
	, ID3D12GraphicsCommandList* command_list)
{
	if (_requestedSceneName.empty())
	{
		return;
	}


	std::string scene_to_load = _requestedSceneName;
	_requestedSceneName.clear();

	auto game_framework = GameFramework::instance();
	game_framework->WaitForGpuComplete();

	ObjectManager::instance()->clear_non_persistent_objects();
	ObjectManager::instance()->process_destructions();
	ResourceManager::instance()->unload_unused_meshes();

	auto it = _scene_creators.find(scene_to_load);
	if (it == _scene_creators.end()) {
		CERROR("scene load failed: " << scene_to_load);
		return;
	}
	_currentScene = it->second(); 
	if (!_currentScene) 
	{
		CERROR("scene creation failed");
		return;
	}

	command_allocator->Reset();
	command_list->Reset(command_allocator, nullptr);

	_currentScene->build_objects(device, command_list);
	ResourceManager::instance()->process_pending_uploads(device, command_list, UINT_MAX);

	command_list->Close();
	ID3D12CommandList* ppd3dCommandLists[] = { command_list };
	game_framework->command_queue()->ExecuteCommandLists(1, ppd3dCommandLists);
	game_framework->WaitForGpuComplete();

	//ResourceManager::instance()->release_upload_buffers(UINT64_MAX); // TODO: 오류 날수 도 있음
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////skybox, terrain///////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


float SceneManager::get_terrain_size() const
{
	if (!_terrainObject)
		return 512.0f; // 기본값

	return 512.0f; // TODO: TerrainMesh에서 실제 크기 가져오기
}

// 모든 terrain을 포함하는 가장 큰 범위 계산
//float SceneManager::get_total_terrain_size() const
//{
//	if (_terrainObjects.empty())
//		return 512.0f;
//
//	float min_x = FLT_MAX, max_x = -FLT_MAX;
//	float min_z = FLT_MAX, max_z = -FLT_MAX;
//
//	for (const auto& [name, terrain_obj] : _terrainObjects)
//	{
//		auto render_comp = terrain_obj->get_component<TerrainRenderComponent>();
//		if (!render_comp) continue;
//
//		auto terrain_mesh =
//			std::dynamic_pointer_cast<TerrainLoader>(render_comp->mesh());
//		if (!terrain_mesh) continue;
//
//		const auto& info = terrain_mesh->get_terrain_info();
//		min_x = std::min(min_x, info.bounds.x);
//		max_x = std::max(max_x, info.bounds.y);
//		min_z = std::min(min_z, info.bounds.z);
//		max_z = std::max(max_z, info.bounds.w);
//	}
//
//	float width = max_x - min_x;
//	float depth = max_z - min_z;
//	return std::max(width, depth);
//}


void SceneManager::build_skybox(ID3D12Device* device, ID3D12GraphicsCommandList* command_list)
{
	if (_skyboxObject) 
	{
		return;
	}

	ResourceManager::instance()->load_skybox("Resource\\SkyBox\\skybox_texture.dds");

	ResourceManager::instance()->load_ibl_maps();
	
	_skyboxObject = ObjectManager::instance()->create_game_object("skybox");
	_skyboxObject->set_persistent(true);

	auto rendercomp = _skyboxObject->add_component<SkyboxRenderComponent>();

	auto skyboxmesh = std::make_shared<SkyboxMesh>(device, command_list);

	rendercomp->set_mesh(skyboxmesh);
	rendercomp->set_pso_name("skybox");


	_skyboxObject->transform()->set_local_scale({ 1.0f, 1.0f, 1.0f });
	_skyboxObject->transform()->set_local_position({ 0.0f, 0.0f, 0.0f });

	ResourceManager::instance()->register_manual_mesh("SkyBox", skyboxmesh);
}

void SceneManager::build_terrain(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{

	if (_terrainObject) {
		return;
	}

	// 1. Terrain 생성 (Grid Mesh만 생성)
	auto terrain = std::make_shared<TerrainLoader>(
		"../../Common/MapData/Heightmap.json"
	);

	ResourceManager::instance()->set_current_command_list(cmdList);

	// 2. ResourceManager
	terrain->load_textures_to_resource_manager(
		"Resource\\HeightMap\\rocky_terrain\\rocky_terrain_02_4k.gltf",
		"Resource\\HeightMap\\aerial_rocks\\textures\\aerial_rocks_04_diff_4k.dds"
	);

	// 3. GPU
	ResourceManager::instance()->register_manual_mesh("Terrain", terrain);

	// 4. GameObject
	_terrainObject = ObjectManager::instance()->create_game_object("terrain");
	_terrainObject->set_persistent(true);

	auto render_comp = _terrainObject->add_component<TerrainRenderComponent>();
	render_comp->set_mesh(terrain);
	render_comp->set_pso_name("terrain");  // Terrain pso

	// 5. Transform 설정 (초기화)
	_terrainObject->transform()->set_local_position({ 0.0f, 0.0f, 0.0f });
	_terrainObject->transform()->set_local_scale({ 1.0f, 1.0f, 1.0f });

	auto pos = _terrainObject->transform()->local_position();
	auto scale = _terrainObject->transform()->local_scale();
}