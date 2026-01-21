#include "pch.h"
#include "GameObject.h"
namespace PIP::GAME
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
