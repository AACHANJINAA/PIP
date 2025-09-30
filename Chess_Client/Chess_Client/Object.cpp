#include "stdafx.h"
#include "Object.h"
#include "ObjectManager.h"

// 정적 멤버 변수 초기화 (컨벤션에 따라 snake_case로 변경)
std::atomic_uint64_t Object::next_id{ 1 };

Object::Object(const std::string& name)
    : _name(name), _uniqueId(next_id++) // 컨벤션에 맞게 next_id 사용
{
    // 생성 시 고유 ID가 자동으로 할당됩니다.
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