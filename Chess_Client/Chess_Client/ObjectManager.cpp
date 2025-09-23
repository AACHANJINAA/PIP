#include "stdafx.h"
#include "ObjectManager.h"
#include "MainPlayer.h"
#include "OtherPlayer.h"
#include "Shader.h"
#include "RenderComponent.h"

#include "stdafx.h"
#include "ObjectManager.h"
#include "GameObject.h"
#include "Component.h"
#include "TransformComponent.h"

// --- 객체 생성 및 파괴 ---
std::shared_ptr<GameObject> ObjectManager::create_game_object(const std::string& name, int layer)
{
    auto newGameObject = std::make_shared<GameObject>(name);
    newGameObject->set_layer(layer);

    std::lock_guard<std::mutex> lock(_mutex);
    _gameObjects.push_back(newGameObject);
    return newGameObject;
}

void ObjectManager::request_destruction(std::shared_ptr<Object> objectToDestroy)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _destructionQueue.push_back(objectToDestroy);
}

void ObjectManager::process_destructions()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_destructionQueue.empty()) return;

    for (auto& obj : _destructionQueue)
    {
        if (auto gameObj = std::dynamic_pointer_cast<GameObject>(obj))
        {
            // on_destroy() 콜백 호출 (GameObject에 구현 필요)
            // gameObj->on_destroy();
            remove_game_object_from_list(gameObj);
        }
        else if (auto component = std::dynamic_pointer_cast<Component>(obj))
        {
            // 컴포넌트가 속한 게임오브젝트에서 해당 컴포넌트 제거 (GameObject에 구현 필요)
            // component->game_object()->remove_component(component);
        }
    }
    _destructionQueue.clear();
}

void ObjectManager::remove_game_object_from_list(std::shared_ptr<GameObject> gameObject)
{
    // 부모로부터 연결 끊기
    if (auto parent = gameObject->transform()->parent().lock())
    {
        // parent->remove_child(gameObject->transform()); // Transform에 구현 필요
    }
    // 관리 목록에서 제거
    std::erase(_gameObjects, gameObject);
}

// --- 객체 검색 ---
std::shared_ptr<GameObject> ObjectManager::find_by_name(const std::string& name)
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (const auto& obj : _gameObjects)
    {
        if (obj->name() == name) return obj;
    }
    return nullptr;
}

std::vector<std::shared_ptr<GameObject>> ObjectManager::find_by_layer(int layer)
{
    std::vector<std::shared_ptr<GameObject>> foundObjects;
    std::lock_guard<std::mutex> lock(_mutex);
    for (const auto& obj : _gameObjects)
    {
        if (obj->layer() == layer)
        {
            foundObjects.push_back(obj);
        }
    }
    return foundObjects;
}


