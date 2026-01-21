#pragma once
namespace JPH { class TempAllocator; }
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
        
        // [추가] 할당자 버전 업데이트 (AI 등)
        virtual void Update(float deltaTime, JPH::TempAllocator* allocator) {}

        virtual void PhysicsUpdate(float deltaTime) {}
        virtual void PhysicsUpdate(float deltaTime, JPH::TempAllocator* tempAllocator) {}

		GameObject* GetOwner() const { return _owner; }

        template <typename T>
        T* GetComponent();

    protected:
        GameObject* _owner;
	};
}