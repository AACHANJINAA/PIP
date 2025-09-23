#pragma once
#include "Object.h"
class GameObject;
class Component : public Object
{
public:
    Component();
    virtual ~Component() = default;

    // getter
    GameObject* game_object() const { return _gameObject; }
    // set_ 접두사 사용
    void set_game_object(GameObject* gameObject) { _gameObject = gameObject; }

protected:
    // 멤버 변수 (코드 컨벤션: _ + camelCase)
    GameObject* _gameObject;
};

