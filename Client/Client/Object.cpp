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
void Object::destroy(const std::shared_ptr<Object>& obj_to_destroy, float delay)
{
    if (obj_to_destroy->is_persistent()) {
        // 영속 객체는 destroy 요청을 무시하거나 경고를 남깁니다.
        return;
    }
    if (!obj_to_destroy || obj_to_destroy->is_destroyed()) return;

	//TODO: delay 기능 구현 필요
    // 어떻게 할까?
	// delay > 0이면 일정 시간 후에 파괴 요청을 큐에 넣도록 타이머를 설정하는 로직 필요
	// delay <= 0이면 즉시 파괴 요청을 큐에 넣습니다.

    obj_to_destroy->set_destroyed(true);

    // ObjectManager에 파괴 요청을 전달하는 것은 동일합니다.
    ObjectManager::instance()->request_destruction(obj_to_destroy);
}