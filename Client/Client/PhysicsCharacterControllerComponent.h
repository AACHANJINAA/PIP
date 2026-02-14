#pragma once

#include <Jolt/Physics/Character/CharacterVirtual.h>
#include "Behavior.h"

class PhysicsCharacterControllerComponent : public Behavior {
public:
	PhysicsCharacterControllerComponent() : Behavior("PhysicsCharacterControllerComponent") {}
    virtual ~PhysicsCharacterControllerComponent();

    void initialize(float height = 1.8f, float radius = 0.5f);
    // GameObject::fixed_update에 의해 1/60초마다 자동 호출됨
    void fixed_update(float deltaTime) override;

    void set_position(const common::Vec3& pos);
    void set_velocity(const common::Vec3& vel);

    common::Vec3 get_position() const;
    common::Vec3 get_velocity() const;

private:
    JPH::Ref<JPH::CharacterVirtual> _character;
	JPH::CharacterVirtualSettings* _settings = nullptr;
	float _halfHeight = 0.f;
};
