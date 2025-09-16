#pragma once

class GameObject;

class Component
{
public:
	Component(GameObject* Owner) :_GameObject(Owner) {};
	virtual ~Component() = default;

	virtual void Start();
	virtual void Update(float DeltaTime);

	GameObject* GetGameObject() const { return _GameObject; }

protected:
	GameObject* _GameObject;
};

