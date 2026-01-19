#include "pch.h"
#include "GameObject.h"
namespace PIP
{
	void GameObject::Update(float deltaTime)
	{
        for (auto& comp : _components)
        {
            comp->Update(deltaTime);
        }
    }

	void GameObject::PhysicsUpdate(float deltaTime)
    {
        for (auto& comp : _components)
        {
            comp->PhysicsUpdate(deltaTime);
        }
    }
}
