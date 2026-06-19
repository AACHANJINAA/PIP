#pragma once
#include "Component.h"

namespace PIP::GAME {
    struct HitboxData {
        std::string name;
        JPH::Ref<JPH::Shape> shape;
        common::Vec3 localOffset;
        common::Quat localRotation;
    };

    class HitboxComponent : public Component {
    public:
        using Component::Component;

        void AddHitbox(const std::string& name, JPH::Shape* shape,
            common::Vec3 offset = { 0,0,0 }, common::Quat rot = { 0,0,0,1 }) {
            _hitboxes.push_back({ name, shape, offset, rot });
        }

        void ClearHitboxes() { _hitboxes.clear(); }

        // [핵심] 리와인드 충돌 검사: 특정 과거 스냅샷을 기준으로 내 히트박스들과 충돌했는지 확인
        bool CheckCollision(const JPH::PhysicsSystem* physics,
                            const JPH::Shape* attackShape,
                            const JPH::RMat44& attackTransform,
                            const common::ObjectSnapshot& pastData,
                            std::string& outHitPart) const;

        const std::vector<HitboxData>& GetHitboxes() const { return _hitboxes; }

    private:
        std::vector<HitboxData> _hitboxes;
    };
}
