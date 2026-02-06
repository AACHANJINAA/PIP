#pragma once
#include "GameObject.h"
#include "TransformComponent.h"

namespace PIP::GAME
{
    class Actor : public GameObject {
    public:
        Actor(int id) : GameObject(id) {}
        virtual ~Actor() override = default;

        // --- 공통 데이터 접근 ---

        // [중요] 컴포넌트를 통해 위치와 회전을 가져오는 헬퍼 함수
        virtual common::Vec3 GetPosition() const {
            auto tc = const_cast<Actor*>(this)->GetComponent<TransformComponent>();
            return tc ? tc->GetPosition() : common::Vec3{ 0,0,0 };
        }

        virtual common::Quat GetRotation() const {
            auto tc = const_cast<Actor*>(this)->GetComponent<TransformComponent>();
            return tc ? tc->GetRotation() : common::Quat{ 0,0,0,1 };
        }

        // [핵심] 매 프레임 위치/회전 기록 (Room::UpdatePhysics에서 호출)
        void RecordSnapshot(uint32_t timestamp) {
            _history.push_back({ timestamp, GetPosition(), GetRotation() });
            if (_history.size() > 30) _history.pop_front(); // 1초 유지
        }

        // [핵심] 특정 시점의 가장 가까운 데이터 찾기
        common::ObjectSnapshot GetSnapshotAt(uint32_t timestamp) const {
            if (_history.empty()) return { 0, GetPosition(), GetRotation() };
            auto it = std::lower_bound(_history.begin(), _history.end(), timestamp,
                [](const common::ObjectSnapshot& s, uint32_t t) { return s._timestamp < t; });
            return (it == _history.end()) ? _history.back() : *it;
        }

        // --- 공통 피격 인터페이스 ---
        virtual bool ValidateHit(JPH::PhysicsSystem* physics,
            const JPH::Shape* attackShape,
            const JPH::RMat44& attackTransform,
            uint32_t timestamp,
            const common::Vec3& attackerPos,
            int32_t damage) = 0;
    protected:
        std::deque<common::ObjectSnapshot> _history;
    };
}
