#include "stdafx.h"
#include "Object.h"

#include "GameObject.h"
#include "ObjectManager.h"

// static 멤버 변수 초기화
std::atomic_uint64_t Object::_nextInstanceId = 1;

Object::Object(const std::string& name)
    : _name(name), _instanceId(_nextInstanceId++) // 생성 시 고유 ID 할당 및 증가
{
}

void Object::destroy(std::shared_ptr<Object> obj_to_destroy, float delay)
{
    if (!obj_to_destroy || obj_to_destroy->is_destroyed()) return;

    // 파괴 플래그 설정
    obj_to_destroy->set_destroyed(true);

    // --- 타입에 따라 다른 처리 ---
    // 1. GameObject인 경우
    if (auto gameObj = std::dynamic_pointer_cast<GameObject>(obj_to_destroy))
    {
        // 자식들도 재귀적으로 파괴 요청
        for (const auto& child : gameObj->transform()->children())
        {
            if (child && child->game_object())
            {
                Object::destroy(child->game_object()); // 재귀 호출
            }
        }
        ObjectManager::Instance()->request_destruction(gameObj);
        return;
    }

    // 2. Component인 경우
    if (auto component = std::dynamic_pointer_cast<Component>(obj_to_destroy))
    {
        // 컴포넌트 파괴 요청 (ObjectManager에 위임)
        ObjectManager::Instance()->request_destruction(component);
        return;
    }

    // 3. 기타 리소스 (Material, Texture 등)인 경우
    // ... (나중에 확장)
}
