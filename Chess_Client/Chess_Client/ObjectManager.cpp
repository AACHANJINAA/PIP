#include "stdafx.h"
#include "ObjectManager.h"
#include "GameObject.h"
#include "Component.h"
#include "TransformComponent.h"
#include <algorithm> // for std::erase

// =================================================================
// 1. 객체 생성 및 파괴 구현
// =================================================================

// [변경] GameObject를 생성하고, layer를 설정한 뒤, _gameObjects 단일 리스트에 추가합니다.
std::shared_ptr<GameObject> ObjectManager::create_game_object(const std::string& name, int layer)
{
    auto newGameObject = std::make_shared<GameObject>(name);
    newGameObject->set_layer(layer); // TODO: GameObject에 layer 기능이 추가되면 이 줄의 주석을 해제합니다.

    //std::lock_guard<std::mutex> lock(_mutex);
    _gameObjects.push_back(newGameObject);
    _newGameObjects.push(newGameObject); // [추가] 새로운 객체 큐에도 추가
    return newGameObject;
}

// [변경] 파괴할 객체를 _destructionQueue에 추가하기만 합니다.
void ObjectManager::request_destruction(std::shared_ptr<Object> objectToDestroy)
{
    //std::lock_guard<std::mutex> lock(_mutex);
    _destructionQueue.push_back(objectToDestroy);
}

// [변경] GameFramework에 의해 프레임 끝에서 호출됩니다.
void ObjectManager::process_destructions()
{
    if (_destructionQueue.empty()) return;

    // 큐에 있는 동안 또 다른 파괴 요청이 들어올 수 있으므로, 처리할 목록을 복사해와서 안전하게 처리합니다.
        auto queueToProcess = _destructionQueue;
    _destructionQueue.clear();

    for (auto& obj : queueToProcess)
    {
        if (auto gameObj = std::dynamic_pointer_cast<GameObject>(obj))
        {
            // TODO : on_destroy() 콜백을 호출합니다. (GameObject에 구현 필요)
            gameObj->on_destroy();

            // 실제 리스트에서 제거합니다.
            remove_game_object_from_list(gameObj);
        }
        else if (auto component = std::dynamic_pointer_cast<Component>(obj))
        {
            // TODO: 컴포넌트가 속한 게임오브젝트에서 해당 컴포넌트를 제거합니다. (GameObject에 구현 필요)
            if(component->game_object())
                 component->game_object()->remove_component(component);
        }
    }
}

void ObjectManager::process_new_game_objects()
{
    // 처리할 새 객체가 없으면 즉시 반환
    if (_newGameObjects.empty()) return;

    // 이번 프레임에 처리할 객체들을 임시로 담을 벡터
    std::vector<std::shared_ptr<GameObject>> processedThisFrame;
    processedThisFrame.reserve(_newGameObjects.size());

    // --- Awake 단계 ---
    // 큐가 빌 때까지 모든 객체의 awake()를 호출
    while (!_newGameObjects.empty())
    {
        // 1. 큐에서 객체를 하나 가져옴
        std::shared_ptr<GameObject> newObj = _newGameObjects.front();
        _newGameObjects.pop();

        if (newObj && !newObj->is_destroyed())
        {
            // 2. awake() 호출
            newObj->awake();
            // 3. start() 호출을 위해 임시 벡터에 저장
            processedThisFrame.push_back(newObj);
        }
    }

    // --- Start 단계 ---
    // Awake가 모두 끝난 객체들의 start()를 호출
    for (const auto& obj : processedThisFrame)
    {
        if (obj && !obj->is_destroyed())
        {
            obj->start();
        }
    }
}

// [추가] GameObject를 리스트에서 제거하고, 부모-자식 관계를 정리하는 내부 함수입니다.
void ObjectManager::remove_game_object_from_list(std::shared_ptr<GameObject> gameObject)
{
    if (!gameObject) return;

    // 부모 Transform이 있다면, 부모의 자식 목록에서 자신을 제거합니다.
    if (auto transform = gameObject->transform())
    {
        if (auto parentTransform = transform->parent().lock())
        {
        	parentTransform->remove_child(transform); //TODO:  TransformComponent에 remove_child 구현 필요
        }
    }

    // 메인 리스트에서 제거합니다.
    //std::lock_guard<std::mutex> lock(_mutex);
    std::erase(_gameObjects, gameObject);
}


// =================================================================
// 2. 객체 검색 구현
// =================================================================

// [대체] 이름으로 객체를 찾습니다.
std::shared_ptr<GameObject> ObjectManager::find_by_name(const std::string& name)
{
    //std::lock_guard<std::mutex> lock(_mutex);
    for (const auto& obj : _gameObjects)
    {
        if (obj && obj->name() == name) return obj;
    }
    return nullptr;
}

// [대체] 레이어로 객체들을 찾습니다.
std::vector<std::shared_ptr<GameObject>> ObjectManager::find_by_layer(int layer)
{
    std::vector<std::shared_ptr<GameObject>> foundObjects;
    //std::lock_guard<std::mutex> lock(_mutex);
    for (const auto& obj : _gameObjects)
    {
       if (obj && obj->layer() == layer)
       {
           foundObjects.push_back(obj);
       }
    }
    return foundObjects;
}

// =================================================================
// [제거된 기능의 구현]
// - 기존 .cpp에 있던 MakeObject, PushObject, DeleteObject 등의 함수 구현은 모두 제거됩니다.
// =================================================================