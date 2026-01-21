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
        //[기존] 단순 물리 업데이트 (Transform 동기화 등)
        virtual void PhysicsUpdate(float deltaTime) {}

        //[신규] 할당자가 필요한 물리 업데이트(캐릭터 시뮬레이션 등)
        virtual void PhysicsUpdate(float deltaTime, JPH::TempAllocator* tempAllocator) {}

		GameObject* GetOwner() const { return _owner; }

        template <typename T>
        T* GetComponent();

    protected:
        GameObject* _owner;
	};
}


