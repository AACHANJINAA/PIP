#pragma once
#include "Component.h"
#include "JoltSetup.h"
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include "Vector3.h"

namespace PIP::GAME
{
	constexpr float ImpactFriction = 35.0f; // 넉백 감쇄 속도

    class CharacterControllerComponent : public Component {
    public:
        CharacterControllerComponent(GameObject* owner, const JPH::ObjectLayer& layer)
            : Component(owner), _physicsLayer{ layer } {
        }
        virtual ~CharacterControllerComponent() override;

        void Initialize() override {}
        void Initialize(JPH::PhysicsSystem* physicsSystem, float height = 1.8f, float radius = 0.3f);

        // 하위 클래스에서 각자의 역할에 맞게 구현
        void PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator) override = 0;

        // 공통 인터페이스
        void AddImpact(const common::Vec3& force) { _impactVelocity += force; }
        void AddImpulse(const common::Vec3& impulse);
        void SetPosition(const common::Vec3& position);

        void SetPhysicsActive(bool active);
        bool IsPhysicsActive() const { return _isPhysicsActive; }

        common::Vec3 GetPosition() const;
        common::Vec3 GetVelocity() const;
        bool IsGrounded() const;
        const JPH::Shape* GetShape() const;
        JPH::CharacterVirtual* GetCharacter() const { return _character.GetPtr(); }
        const common::Vec3& GetImpactVelocity() const { return _impactVelocity; }

    protected:
        bool _isPhysicsActive = true;
        float _halfHeight = 0.0f;
        common::Vec3 _impactVelocity = { 0,0,0 };

        JPH::PhysicsSystem* _physicsSystem = nullptr;
        JPH::Ref<JPH::CharacterVirtual> _character;
        JPH::Ref<JPH::CharacterVirtualSettings> _settings;
        JPH::ObjectLayer _physicsLayer;

        class CharacterContactListener : public JPH::CharacterContactListener {
        public:
            virtual void OnContactAdded(const JPH::CharacterVirtual* inCharacter, const JPH::BodyID& inBodyID2,
                const JPH::SubShapeID& inSubShapeID2, JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal,
                JPH::CharacterContactSettings& ioSettings) override {
            }
        } _contactListener;

    };
}
