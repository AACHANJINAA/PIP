#pragma once
#include <memory>

class GameObject;

class Component
{
public:
	Component(GameObject* Owner) :_gameObject(Owner) {};
	virtual ~Component() = default;

	virtual void start();
	virtual void update(float DeltaTime);

	std::shared_ptr<GameObject> get_Gameobject() const { return _gameObject; }

protected:
	std::shared_ptr<GameObject> _gameObject;
};

