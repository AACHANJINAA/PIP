#pragma once

class GameObject;
class Component
{
public:
	//TODO: shared_ptr·Î ¹Ù²Ü°Í
	Component(GameObject* Owner) :_GameObject(Owner) {};
	virtual ~Component() = default;

	virtual void start();
	virtual void update(float DeltaTime);

	GameObject* get_Gameobject() const { return _GameObject; }

protected:
	GameObject* _GameObject;
};

