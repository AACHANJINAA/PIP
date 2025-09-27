#include "stdafx.h"
#include "Object.h"
#include "ObjectManager.h"

Object::Object(const std::string& name)
    : _name(name)
{
    // ID 할당은 ObjectManager의 책임
}

// [제거] _nextInstanceId 초기화 코드를 제거합니다.

// destroy 함수는 우선 그대로 둡니다.
void Object::destroy(std::shared_ptr<Object> obj_to_destroy, float delay)
{
    if (!obj_to_destroy || obj_to_destroy->is_destroyed()) return;

    obj_to_destroy->set_destroyed(true);

    // ObjectManager에 파괴 요청을 전달하는 것은 동일합니다.
    ObjectManager::Instance()->request_destruction(obj_to_destroy);
}