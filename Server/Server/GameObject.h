#pragma once
#include <typeindex>
#include <vector>
#include <memory>
#include <unordered_map>
#include "Component.h"

namespace JPH { class TempAllocator; }

namespace PIP::GAME
{
    class GameObject
    {
    public:
        GameObject(int64_t id = -1) : _id{ id } {}
        virtual ~GameObject() = default;

        void Update(float deltaTime);
        // [추가]
        virtual void Update(float deltaTime, JPH::TempAllocator* allocator);

        void PhysicsUpdate(float deltaTime);
        void PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator);

        template <typename T, typename... Args>
        T* AddComponent(Args&&... args);

        template <typename T>
        inline T* GetComponent(); // inline 명시

		void SetName(std::string_view name) { _name = name; }
        const std::string& GetName() const { return _name; }

        int64_t GetId() const { return _id; }

        // [공통 인터페이스] 특정 공격에 맞았는지 검증하고 처리
        virtual bool ValidateHit(JPH::PhysicsSystem* physics,
                                 const JPH::Shape* attackShape,
                                 const JPH::RMat44& attackTransform,
                                 uint32_t timestamp,
                                 // 리와인드용
                                 GameObject* attacker,
                                 int32_t damage) = 0; // NPC, Player가 각각 구현

    private:
        int64_t _id;
        std::string _name;
        std::vector<std::unique_ptr<Component>> _components;
        std::unordered_map<std::type_index, Component*> _componentCache;
    };

    template <typename T>
    inline T* Component::GetComponent()
    {
        return _owner->GetComponent<T>();
    }

    template <typename T, typename... Args>
    T* GameObject::AddComponent(Args&&... args)
    {
        auto newComponent = std::make_unique<T>(this, std::forward<Args>(args)...);
        T* ptr = newComponent.get();

        _components.push_back(std::move(newComponent));
        _componentCache[typeid(T)] = ptr;

        ptr->Initialize();
        return ptr;
    }

    template <typename T>
    inline T* GameObject::GetComponent()
    {
        auto it = _componentCache.find(typeid(T));
        if (it != _componentCache.end())
        {
            return static_cast<T*>(it->second);
        }
        return nullptr;
    }
}