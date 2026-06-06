#include "stdafx.h"
#include "ObjectManager.h"
#include "GameObject.h"
#include "Component.h"
#include "TransformComponent.h"
#include <algorithm>

#include "Behavior.h"

std::shared_ptr<GameObject> ObjectManager::create_game_object(const std::string& name)
{
    // 1. make_shared로 객체를 생성합니다. 이제 생성자는 안전합니다.
    auto newGameObject = std::make_shared<GameObject>(name);

    // 2. shared_ptr 생성이 완료된 후, init()을 호출하여 나머지 초기화를 진행합니다.
    // 이 시점에는 shared_from_this()를 안전하게 호출할 수 있습니다.
    newGameObject->init();

    // 3. 목록에 추가하고 반환합니다.
    _gameObjects.push_back(newGameObject);
    _newGameObjects.push(newGameObject);
    return newGameObject;
}

void ObjectManager::request_destruction(const std::shared_ptr<Object>& objectToDestroy)
{
    _destructionQueue.push_back(objectToDestroy);
}

void ObjectManager::process_destructions()
{
    if (_destructionQueue.empty()) return;

    auto queueToProcess = std::move(_destructionQueue);

	auto destroy_time_start = std::chrono::steady_clock::now();
    for (auto it = queueToProcess.begin(); it != queueToProcess.end(); ++it)
    {
		auto obj = *it;
		if (!obj)
		{
			continue;
		}
        if (obj->is_persistent())
        {
            continue;
        }
    	auto destroy_time_end = std::chrono::steady_clock::now();
		auto duration = 
            std::chrono::duration_cast<std::chrono::milliseconds>(destroy_time_end - destroy_time_start).count();
		if (duration > 500.f)
		{
			std::move(it, queueToProcess.end(), std::back_inserter(_destructionQueue));
            break;
		}
        if (!obj) continue;

        if (auto gameObj = std::dynamic_pointer_cast<GameObject>(obj))
        {
            remove_game_object_from_list(gameObj);
        }
        // --- [추가] 컴포넌트 파괴 로직 ---
        else if (auto component = std::dynamic_pointer_cast<Component>(obj))
        {
            // 컴포넌트가 속한 게임오브젝트가 아직 유효하다면,
            if (component->game_object() && !component->game_object()->is_destroyed())
            {
                // 게임오브젝트에게 컴포넌트 제거를 요청합니다.
                component->game_object()->remove_component(component);
            }
        }
    }
}
void ObjectManager::remove_game_object_from_list(const std::shared_ptr<GameObject>& gameObject)
{
    if (!gameObject) return;


    // ---------------------------------------------------------
    // 1. [추가] 모든 컴포넌트의 on_destroy() 호출
    // ---------------------------------------------------------
    // GameObject 클래스 내부에 정의된 _components 리스트를 순회합니다.
    // (GameObject.h에 getter가 있다고 가정하거나, 친구 클래스라면 직접 접근)
    for (const auto& component : gameObject->components())
    {
        if (auto behavior = std::dynamic_pointer_cast<Behavior>(component))
        {
            // 비활성화 상태여도 파괴 시점의 정리는 필요하므로 무조건 호출합니다.
            behavior->on_destroy();
        }
    }

    // --- [변경] 부모로부터 연결 끊기 로직 ---
    if (auto transform = gameObject->transform())
    {
        // 1. 모든 자식들의 부모를 nullptr로 설정하여 연결을 끊습니다.
        // (자식 목록을 복사해서 순회해야 안전함)
        auto childrenCopy = transform->children();
        for (const auto& childTransform : childrenCopy)
        {
            childTransform->set_parent(nullptr);
        }

        // 2. 자기 자신의 부모를 nullptr로 설정합니다.
        // 이 함수 내부에서, 원래 부모의 자식 목록에서 자신을 제거하는 로직이 처리됩니다.
        transform->set_parent(nullptr);
    }

    // 메인 리스트에서 제거
    std::erase(_gameObjects, gameObject);
}

void ObjectManager::remove_game_object(const std::shared_ptr<GameObject>& gameObject)
{
    if (!gameObject) return;
    remove_game_object_from_list(gameObject);
}

std::shared_ptr<GameObject> ObjectManager::find_by_name(const std::string& name)
{
    auto it = std::find_if(_gameObjects.begin(), _gameObjects.end(),
        [&name](const std::shared_ptr<GameObject>& obj) {
            return obj && obj->name() == name && !obj->is_destroyed();
        });
	// O(N) 탐색 결과 반환
	return (it != _gameObjects.end()) ? *it : nullptr;
}

std::vector<std::shared_ptr<GameObject>> ObjectManager::find_by_layer(uint32_t layerMask)
{
    // KJ 예시 
    //// 1. Chess_Scene에서 플레이어 생성 시
    //auto playerObject = ObjectManager::Instance()->create_game_object("MainPlayer");
    //playerObject->set_layer("Player"); // 이름으로 레이어 설정

    //// 2. 물리 시스템에서 플레이어와 적의 충돌만 검사하고 싶을 때
    //uint32_t playerLayer = LayerManager::Instance()->get_layer_value("Player");
    //uint32_t enemyLayer = LayerManager::Instance()->get_layer_value("Enemy");
    //uint32_t collisionMask = playerLayer | enemyLayer; // 두 레이어를 합친 마스크

    //auto objectsToTest = ObjectManager::Instance()->find_by_layer_mask(collisionMask);

    std::vector<std::shared_ptr<GameObject>> foundObjects;
    for (const auto& obj : _gameObjects)
    {
        // 비트 AND 연산으로 해당 레이어에 속하는지 확인
        if (obj && (obj->layer_mask() & layerMask) != 0)
        {
            foundObjects.push_back(obj);
        }
    }
    return foundObjects;
}

void ObjectManager::process_new_game_objects()
{
    if (_newGameObjects.empty()) return;

    // 이번 프레임에 처리할 객체들을 임시 벡터로 옮깁니다.
    std::vector<std::shared_ptr<GameObject>> processedThisFrame;
    while (!_newGameObjects.empty())
    {
        processedThisFrame.push_back(_newGameObjects.front());
        _newGameObjects.pop();
    }

    // Awake 단계: 모든 새 객체의 awake()를 먼저 호출
    for (const auto& newObj : processedThisFrame)
    {
        if (newObj && !newObj->is_destroyed())
        {
            newObj->awake();
        }
    }

    // Start 단계: 모든 awake()가 끝난 후 start()를 호출
    for (const auto& newObj : processedThisFrame)
    {
        if (newObj && !newObj->is_destroyed())
        {
            newObj->start();
        }
    }
}

void ObjectManager::clear_non_persistent_objects()
{
    // _allGameObjects를 직접 수정하면 반복자가 무효화될 수 있으므로,
	// 파괴할 오브젝트 목록을 따로 만듭니다.
    std::vector<std::shared_ptr<GameObject>> objects_to_destroy;

    for (const auto& game_object : _gameObjects)
    {
        // is_persistent() 플래그가 false인 오브젝트만 파괴 목록에 추가합니다.
        if (game_object && !game_object->is_persistent())
        {
            objects_to_destroy.push_back(game_object);
        }
    }

    // 목록에 있는 모든 오브젝트에 대해 파괴를 요청합니다.
    for (const auto& game_object : objects_to_destroy)
    {
        Object::destroy(game_object);
    }

    _npcMap.clear();
    // process_destructions()가 다음 프레임에 실제로 메모리에서 제거할 것입니다.
}

std::shared_ptr<GameObject> ObjectManager::find_object(const std::string& name)
{
    auto it = std::find_if(_gameObjects.begin(), _gameObjects.end(), [&name](const std::shared_ptr<GameObject>& object)
        {
            return object->name() == name;
        });

    // 찾지 못하면 nullptr 반환 (end() 역참조 방지)
    if (it == _gameObjects.end())
    {
        return nullptr;
    }

    return *it;
}

std::shared_ptr<GameObject> ObjectManager::find_object(const int& id)
{
    auto it = std::find_if(_gameObjects.begin(), _gameObjects.end(), [id](const std::shared_ptr<GameObject>& object)
        {
            return object->unique_id() == id;
        });

    // 찾지 못하면 nullptr 반환 (end() 역참조 방지)
    if (it == _gameObjects.end())
    {
        return nullptr;
    }

    return *it;
}

std::shared_ptr<GameObject> ObjectManager::find_npc(int64_t id)
{
    auto it = _npcMap.find(id);
    if (it != _npcMap.end())
        return it->second;
    return nullptr;
}
void ObjectManager::register_npc(int64_t id, const std::shared_ptr<GameObject>& npc)
{
    _npcMap[id] = npc;
}
void ObjectManager::unregister_npc(int64_t id)
{
    _npcMap.erase(id);
}

void ObjectManager::spawn_monster(size_t how_many_you_want_npc_count)
{

}
