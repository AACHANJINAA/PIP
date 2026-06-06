#pragma once
#include "Object.h"
class GameObject;
class Component : public Object
{
public:
    Component();
	Component(const std::string& name) : Object(name), _gameObject{} {}
    virtual ~Component() = default;

    // 모든 컴포-넌트는 이 타입을 가집니다. 기본적으로는 의존성이 없음을 의미합니다.
    using required_components = std::tuple<>;

    // [수정] 반환 타입을 shared_ptr로 변경하고, 내부적으로 weak_ptr를 lock()하여 사용
    std::shared_ptr<GameObject> game_object() const { return _gameObject.lock(); }

    // [수정] 이제 shared_ptr를 받아서 weak_ptr에 대입
    void set_game_object(const std::shared_ptr<GameObject>& gameObject) { _gameObject = gameObject; }

protected:
    // [수정] GameObject를 가리키는 포인터를 weak_ptr로 변경
    std::weak_ptr<GameObject> _gameObject;
};

