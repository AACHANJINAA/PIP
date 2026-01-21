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

	void GameObject::PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator)
	{
        // 내 컴포넌트들 중에 allocator 필요한 놈 있으면 다 호출해라
        for (const auto& component : _components)
        {
            // Component 클래스에 가상함수로 추가했으니 그냥 호출하면 됨
            component->PhysicsUpdate(deltaTime, allocator);
        }
	}
}
