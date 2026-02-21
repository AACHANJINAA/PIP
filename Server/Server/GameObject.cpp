#include "pch.h"
#include "GameObject.h"
namespace PIP::GAME
{
	std::atomic<int64_t> GameObject::_id_counter{ 1 };

	void GameObject::Update(float deltaTime)
	{
        for (auto& comp : _components)
        {
            comp->Update(deltaTime);
        }
    }

    void GameObject::Update(float deltaTime, JPH::TempAllocator* allocator)
    {
        for (auto& comp : _components)
        {
            comp->Update(deltaTime, allocator);
        }
    }

	void GameObject::PhysicsUpdate(float deltaTime)
    {
        for (auto& comp : _components)
        {
            comp->PhysicsUpdate(deltaTime);
        }
    }

	void GameObject::PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator)
	{
        for (const auto& component : _components)
        {
            component->PhysicsUpdate(deltaTime, allocator);
        }
	}
}