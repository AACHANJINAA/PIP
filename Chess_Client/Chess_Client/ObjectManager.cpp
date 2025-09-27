#include "stdafx.h"
#include "ObjectManager.h"
#include "GameObject.h"
#include "Component.h"
#include "TransformComponent.h"
#include <algorithm>

std::shared_ptr<GameObject> ObjectManager::create_game_object(const std::string& name)
{
    // [롤백] make_shared를 사용하여 GameObject를 생성하고 리스트에 추가
    auto newGameObject = std::make_shared<GameObject>(name);
    _gameObjects.push_back(newGameObject);
    _newGameObjects.push(newGameObject);
    return newGameObject;;
}

void ObjectManager::request_destruction(std::shared_ptr<Object> objectToDestroy)
{
    _destructionQueue.push_back(objectToDestroy);
}

void ObjectManager::process_destructions()
{
    if (_destructionQueue.empty()) return;

    auto queueToProcess = _destructionQueue;
    _destructionQueue.clear();

    for (auto& obj : queueToProcess)
    {
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
void ObjectManager::remove_game_object_from_list(std::shared_ptr<GameObject> gameObject)
{
    if (!gameObject) return;

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

std::shared_ptr<GameObject> ObjectManager::find_by_name(const std::string& name)
{
    auto it = std::find_if(_gameObjects.begin(), _gameObjects.end(),
        [&name](const std::shared_ptr<GameObject>& obj) {
            return obj && obj->name() == name && !obj->is_destroyed();
        });
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
