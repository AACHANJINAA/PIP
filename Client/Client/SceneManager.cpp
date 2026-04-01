#include "stdafx.h"
#include "SceneManager.h"

#include "Chess_Scene.h"
#include "Tool_Scene.h"
#include "GameFramework.h"
#include "Main_Scene.h"
#include "Boss_Scene.h"
#include "NetworkManager.h"

#include "ObjectManager.h"
#include "ResourceManager.h"

#include "SkyboxMesh.h"
#include "SkyboxRenderComponent.h"
#include "TerrainLoader.h"
#include "TerrainRenderComponent.h"
#include "UIManager.h"

SceneManager::SceneManager()
{

}

SceneManager::~SceneManager()
{

}

void SceneManager::initialize(ID3D12Device* device, ID3D12GraphicsCommandList* command_list)
{
	register_scene<Chess_Scene>("ChessScene");
	register_scene<Main_Scene>("MainScene");
	register_scene<Tool_Scene>("ToolScene");
	register_scene<Boss_Scene>("BossScene");
	//register_scene<Lobby_Scene>("LobbyScene");

	change_scene("MainScene");
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

void SceneManager::process_scene_change_if_requested(ID3D12Device* device ,ID3D12CommandAllocator* command_allocator , ID3D12GraphicsCommandList* command_list)
{
	if (_requestedSceneName.empty())
	{
		return;
	}


	std::string scene_to_load = _requestedSceneName;
	_requestedSceneName.clear();

	auto game_framework = GameFramework::instance();
	game_framework->WaitForGpuComplete();

    UIManager::instance()->release();
	ObjectManager::instance()->clear_non_persistent_objects();
	ObjectManager::instance()->process_destructions();
	ResourceManager::instance()->unload_unused_meshes();

    // 기존 지형 오브젝트 정리
    if (_terrainObject)
    {
        ObjectManager::instance()->remove_game_object(_terrainObject);
        _terrainObject.reset();
    }

    for (auto& landscapeObj : _MainlandscapeObjects)
    {
        ObjectManager::instance()->remove_game_object(landscapeObj);
    }
    _MainlandscapeObjects.clear();

    auto it = _scene_creators.find(scene_to_load);
    if (it == _scene_creators.end()) {
        CERROR("scene load failed: " << scene_to_load);
        return;
    }
    _currentScene = it->second();
    _currentScene->set_scene_name(scene_to_load);
    if (!_currentScene)
    {
        CERROR("scene creation failed");
        return;
    }

    command_allocator->Reset();
    command_list->Reset(command_allocator, nullptr);

    _currentScene->build_objects(device, command_list);
    UINT64 nextFenceValue = game_framework->next_fence_value();
    ResourceManager::instance()->process_pending_uploads(device, command_list, nextFenceValue, 16);

	command_list->Close();
	ID3D12CommandList* ppd3dCommandLists[] = { command_list };
	game_framework->command_queue()->ExecuteCommandLists(1, ppd3dCommandLists);
	game_framework->WaitForGpuComplete();
    //TODO: 씬 전환 후 서버에게 패킷 전송 후 방입장 요청

    // 2. [추가] 씬 전환 및 리소스 로딩이 완벽히 끝났다면 서버에 보고!
	common::packet::CS_PACKET_PLAYER_READY ready_packet;
    ready_packet._type = common::packet::PacketType::C2S_P_PLAYER_READY;
    ready_packet._size = sizeof(ready_packet);

    // NetworkManager를 통해 서버로 전송
    NetworkManager::instance()->send_packet(reinterpret_cast<const char*>(&ready_packet), sizeof(ready_packet));
    CLOG("Scene Loading Complete! Sent READY to Server. Scene: " << _requestedSceneName);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////skybox, terrain///////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


std::shared_ptr<GameObject> SceneManager::get_terrain_object() const
{
	// 하위 호환: 첫 번째 타일 반환
	if (_MainlandscapeObjects.empty()) return nullptr;
	return _MainlandscapeObjects[0];
}

const std::vector<std::shared_ptr<GameObject>>& SceneManager::get_all_landscapes() const
{
	return _MainlandscapeObjects;
}

float SceneManager::get_terrain_size() const
{
	if (!_terrainObject)
		return 512.0f; // 기본값

	return 512.0f;
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


void SceneManager::build_skybox_if_needed(ID3D12Device* device, ID3D12GraphicsCommandList* command_list)
{
	if (_skyboxObject) 
	{
		return;
	}

	ResourceManager::instance()->load_skybox("Resource\\SkyBox\\night_field\\night_field_skybox.dds");

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

	//ResourceManager::instance()->set_current_command_list(cmdList);

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

void SceneManager::build_main_landscapes(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    std::filesystem::path landscapeBaseDir = "Resource/MainLandscape";

    if (!std::filesystem::exists(landscapeBaseDir))
    {
        CERROR("MainLandscape directory not found!");
        return;
    }

    int loadedCount = 0;

	//ResourceManager::instance()->set_current_command_list(cmdList);

    // 모든 Landscape## 폴더 순회
    for (const auto& entry : std::filesystem::directory_iterator(landscapeBaseDir))
    {
        if (!entry.is_directory()) continue;

        std::string folderName = entry.path().filename().string();

        // "Landscape"로 시작하는 폴더만 처리
        if (folderName.find("Landscape") != 0) continue;

        std::filesystem::path metadataPath = entry.path() / "metadata.json";

        if (!std::filesystem::exists(metadataPath))
        {
            CLOG("Skipping " << folderName << " - no metadata.json");
            continue;
        }

        // 1. metadata.json에서 월드 좌표 미리 읽기
        std::ifstream metaFile(metadataPath);
        nlohmann::json metaJson;
        try {
            metaFile >> metaJson;
            metaFile.close();
        }
        catch (...) {
            CERROR("Failed to parse: " << metadataPath);
            continue;
        }

        // 2. TerrainLoader 생성 (새로운 생성자 사용)
        std::string metadataPathStr = metadataPath.string();
        auto terrain = std::make_shared<TerrainLoader>(metadataPathStr, true); // 두 번째 인자 true = MainLandscape 형식
    	
    	//ResourceManager::instance()->set_current_command_list(cmdList);

        // 3. SharedTextures 경로 구성
        std::filesystem::path sharedTexPath = landscapeBaseDir / "SharedTextures";

        // 임시: 첫 번째 레이어(Rock)의 텍스처만 로드
        // 실제로는 metaJson["layers"]를 순회하며 모든 레이어 처리 필요
        //std::string baseTexPath = "Resource\\HeightMap\\aerial_rocks\\aerial_rocks_04_4k.gltf";
        //std::string detailTexPath = (sharedTexPath / "T_Dead_Grass_Albedo.dds").string();

        //terrain->load_textures_to_resource_manager(baseTexPath, detailTexPath);

        // 4. Weightmap 로드 (각 레이어별)
        std::vector<std::string> weightmapPaths;
        for (const auto& layer : metaJson["layers"])
        {
            std::string wmFile = layer.value("weightmap_file", "");
            if (!wmFile.empty())
            {
                std::string wmPath = (entry.path() / wmFile).string();
                weightmapPaths.push_back(wmPath);
            }
        }
        terrain->load_landscape_weightmaps(weightmapPaths);

		//terrain->upload_to_gpu(device, cmdList, 0);

        // 5. ResourceManager 등록
        std::string meshKey = "Landscape" + folderName;
        ResourceManager::instance()->register_manual_mesh(meshKey, terrain);

        // 6. GameObject 생성
        auto landscapeObj = ObjectManager::instance()->create_game_object(meshKey);
        landscapeObj->set_persistent(true);

        auto renderComp = landscapeObj->add_component<TerrainRenderComponent>();
        renderComp->set_mesh(terrain);
        renderComp->set_pso_name("terrain");

        // 7. Transform 설정 (월드 좌표 배치)
        landscapeObj->transform()->set_local_position(XMFLOAT3{0, 0.0f, 0 });
        landscapeObj->transform()->set_local_scale({ 1.0f, 1.0f, 1.0f });

        // 8. 벡터에 추가
        _MainlandscapeObjects.push_back(landscapeObj);
		loadedCount++;
    }
}

