#pragma once
#include "Component.h"
#include "../../Common/JoltHelper.h"

namespace PIP::GAME
{
    class TransformComponent : public Component
    {
    public:
        TransformComponent(GameObject* owner) : Component(owner) {}

        void SmoothRotateTo(const common::Vec3& dir, float dt);

        const common::Vec3& GetPosition() const { return _position; }
        const common::Quat& GetRotation() const { return _rotation; }
        common::Vec3 GetForward() const;
        common::Vec3 GetRight() const;

        JPH::Vec3 GetJoltPosition() const { return Utils::ToJolt(_position); }
        JPH::Quat GetJoltRotation() const { return Utils::ToJolt(_rotation); }

        // Jolt 타입 지원 추가
        void SetPosition(const JPH::RVec3& pos) { _position = Utils::FromJolt(pos); }
        void SetRotation(const JPH::Quat& rot) { _rotation = Utils::FromJolt(rot); }
        void SetPosition(const common::Vec3& pos) { _position = pos; }
        void SetRotation(const common::Quat& rot) { _rotation = rot; }

    private:
        common::Vec3 _position{ 0, 0, 0 };
        common::Quat _rotation{ 0, 0, 0, 1 };
    };
}
