#pragma once
namespace PIP::GAME
{
	class GameObject;
	class Component
	{
    public:
        Component(GameObject* owner);
        virtual ~Component() = default;

        virtual void Initialize() {}
        virtual void Update(float deltaTime) {}
        virtual void PhysicsUpdate(float deltaTime) {}

        GameObject* GetOwner() const { return _owner; }
        template <typename T>
        T* GetComponent();

    protected:
        GameObject* _owner;
	};
}


