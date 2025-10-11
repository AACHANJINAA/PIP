#include "stdafx.h"
#include "SceneManager.h"

#include "Chess_Scene.h"
#include "GameFramework.h"
#include "ObjectManager.h"
#include "ResourceManager.h"

SceneManager::SceneManager()
{

}

SceneManager::~SceneManager()
{

}

void SceneManager::initialize()
{
    register_scene<Chess_Scene>("ChessScene");
    //register_scene<Lobby_Scene>("LobbyScene");

    // 처음 씬
    change_scene("ChessScene");
}

void SceneManager::change_scene(const std::string& scene_name)
{
	_requestedSceneName = scene_name;
}

void SceneManager::process_scene_change_if_requested(ID3D12Device* device
	,ID3D12CommandAllocator* command_allocator
	, ID3D12GraphicsCommandList* command_list)
{
    // 전환 요청이 없으면 아무것도 하지 않고 즉시 리턴합니다.
    if (_requestedSceneName.empty())
    {
        return;
    }

    // --- 여기서부터는 모든 게임 로직이 멈춘 안전한 시점입니다 ---

    std::string scene_to_load = _requestedSceneName;
    _requestedSceneName.clear();

	auto game_framework = GameFramework::Instance();
    // 1. GPU가 이전 프레임의 모든 작업을 마칠 때까지 기다립니다.
    game_framework->WaitForGpuComplete();

    // 2. 현재 씬의 영속성 없는 모든 오브젝트를 파괴 목록으로 옮깁니다.
    ObjectManager::Instance()->clear_non_persistent_objects();
    // 파괴 목록에 있는 오브젝트들을 실제로 소멸시킵니다.
    ObjectManager::Instance()->process_destructions();

    // 3. 새로운 씬을 생성합니다.
    auto it = _scene_creators.find(scene_to_load);
    if (it == _scene_creators.end()) {
        CERROR("등록되지 않은 씬: " << scene_to_load);
        return;
    }
    _currentScene = it->second();
    if (!_currentScene) 
    {
        CERROR("씬이 널 포인터임")
        return;
    }

    // 4. 새로운 씬의 리소스를 로드하고 GPU에 업로드합니다.

    command_allocator->Reset();
    command_list->Reset(command_allocator, nullptr);

    _currentScene->build_objects(device, command_list);
    ResourceManager::Instance()->upload_pending_meshes(device, command_list);

    // 5. 리소스 업로드 커맨드를 실행하고 완료될 때까지 기다립니다.
    command_list->Close();
    ID3D12CommandList* ppd3dCommandLists[] = { command_list };
    game_framework->command_queue()->ExecuteCommandLists(1, ppd3dCommandLists);
    game_framework->WaitForGpuComplete();

    // TODO: 임시 업로드 버퍼 해제 로직
    //ResourceManager::Instance()->release_upload_buffers();
    // --- [추가] 모든 씬 전환 작업이 끝난 후 ---
	// 이제 더 이상 사용되지 않는 이전 씬의 메시들을 메모리에서 완전히 해제합니다.
    ResourceManager::Instance()->unload_unused_meshes();
}
