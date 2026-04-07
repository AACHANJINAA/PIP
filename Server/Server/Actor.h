#pragma once
#include "CharacterControllerComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"

namespace PIP::GAME
{
    enum class Faction : uint32_t {
        FACTION_PLAYER = 0,
        FACTION_MONSTER = 1,
        FACTION_NEUTRAL = 2,
    	FACTION_UNKNOWN = 9999,
        // 추가 파벌 정의 가능
	};
    class Actor : public GameObject {
    public:
        Actor() : _factionId(Faction::FACTION_UNKNOWN) {}
        virtual ~Actor() override = default;

        // --- 상태 관리 메서드 ---
        bool IsActive() const { return _isActive; }
        virtual void SetActive(bool active) { _isActive = active; }

        std::chrono::milliseconds GetRespawnDelay() const { return _respawnDelay; }
        void SetRespawnDelay(float delay_seconds) { _respawnDelay = std::chrono::milliseconds(static_cast<int64_t>(delay_seconds * 1000.0f)); }

		void SetDeathAnimationTime(const std::chrono::milliseconds& duration) { _deathAnimationDuration = duration; }
        std::chrono::milliseconds GetDeathAnimationTime() const { return _deathAnimationDuration; } // 고정값, 필요시 조정
        // --- 공통 데이터 접근 ---

        // [중요] 컴포넌트를 통해 위치와 회전을 가져오는 헬퍼 함수
        virtual common::Vec3 GetPosition() const {
            // [중요] 캐릭터 컨트롤러가 있다면 Jolt 바디의 좌표가 '진실'임
            if (auto cc = const_cast<Actor*>(this)->GetComponent<CharacterControllerComponent>()) {
                return cc->GetPosition(); // 발바닥 보정값이 들어간 cc의 GetPosition 호출
            }
            // 물리 객체가 없는 일반 오브젝트만 트랜스폼 참조
            auto tc = const_cast<Actor*>(this)->GetComponent<TransformComponent>();
            return tc ? tc->GetPosition() : common::Vec3{ 0,0,0 };
        }

        virtual common::Vec3 GetVelocity() const {
            if (auto cc = const_cast<Actor*>(this)->GetComponent<CharacterControllerComponent>()) {
                return cc->GetVelocity();
            }
            return { 0,0,0 };
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

        void SetFaction(Faction factionId) { _factionId = factionId; }
        Faction GetFaction() const { return _factionId; }

        virtual int32_t GetHP() const { return 0; }
        virtual void SetHP(int hp) {}

        virtual common::packet::EntityState GetState() const { return common::packet::EntityState::COUNT; }
		virtual void SetState(const common::packet::EntityState& state) {}
    protected:
        std::deque<common::ObjectSnapshot> _history;
        Faction _factionId;

        // --- 새로 추가될 필드 ---
        bool    _isActive = true;      // 활성화 상태 (false면 업데이트/렌더링 제외)
        std::chrono::milliseconds _respawnDelay {10000};  // 리스폰 대기 시간
		std::chrono::milliseconds _deathAnimationDuration{ 1000 }; // 사망 애니메이션 시간 (필요시 조정)
    };
}
