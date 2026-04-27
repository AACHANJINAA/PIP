#include "pch.h"
#include "InventoryComponent.h"

namespace PIP::GAME
{
	void InventoryComponent::Initialize()
	{
		Component::Initialize();
	}

	void InventoryComponent::Update(float deltaTime)
	{
		Component::Update(deltaTime);
	}

	void InventoryComponent::Update(float deltaTime, JPH::TempAllocator* allocator)
	{
		Component::Update(deltaTime, allocator);
	}

	void InventoryComponent::PhysicsUpdate(float deltaTime)
	{
		Component::PhysicsUpdate(deltaTime);
	}

	void InventoryComponent::PhysicsUpdate(float deltaTime, JPH::TempAllocator* tempAllocator)
	{
		Component::PhysicsUpdate(deltaTime, tempAllocator);
	}
}
