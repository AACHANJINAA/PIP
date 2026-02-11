#include "pch.h"
#include "Player.h"

#include <algorithm>

#include "HitboxComponent.h"


namespace PIP::GAME
{
	Player::Player(long long owner_id) 
		:
		_name {"DefaultName" },
		_hp { 100 },
		_max_hp{ 100 },
		_level { 0 },
		_exp { 0 },
		_damage{ 10 },
		Actor(static_cast<int>(owner_id)),
		_owner_id{ owner_id }
	{
		SetFaction(Faction::FACTION_PLAYER);
		// 1. Transform 추가
		AddComponent<GAME::TransformComponent>();
		// 2. 물리 컨트롤러 추가 (플레이어도 이제 물리 적용!)
		AddComponent<GAME::CharacterControllerComponent>();

		// [추가] 히트박스 설정 (NPC와 동일한 크기의 캡슐)
		auto hitbox = AddComponent<HitboxComponent>();
		float height = 1.8f;
		float radius = 0.5f;
		float halfCylinderHeight = 0.5f * height - radius;
		JPH::Ref<JPH::Shape> bodyShape = new JPH::CapsuleShape(halfCylinderHeight, radius);
		hitbox->AddHitbox("Body", bodyShape, { 0.0f, 0.9f, 0.0f });

		SetName("Player_" + std::to_string(owner_id));
	}

	bool Player::ValidateHit(JPH::PhysicsSystem* physics, const JPH::Shape* attackShape,
	                         const JPH::RMat44& attackTransform, uint32_t timestamp, GameObject* attacker, int32_t damage)
	{
		// 1. 과거 시점 위치 복구
		auto snapshot = GetSnapshotAt(timestamp);

		// 2. 히트박스 충돌 검사
		std::string hitPart;
		auto hc = GetComponent<HitboxComponent>();
		if (!hc) {
			MYERROR("[HitTest] Player " << GetName() << " has NO HitboxComponent!");
			return false;
		}
		if (hc && hc->CheckCollision(physics, attackShape, attackTransform, snapshot, hitPart))
		{
			MYLOG("[HitTest] HIT SUCCESS! Part: " << hitPart << " HP: " << _hp << " -> " << (_hp - damage));
			// 3. 실제 데미지 적용
			_hp -= static_cast<short>(damage);
			_hp = std::max<short>(_hp, 0);

			// 4. 넉백 효과 (공격자 방향 기준)
			if (auto cc = GetComponent<CharacterControllerComponent>()) {
				using namespace common::VectorHelper;
				common::Vec3 knockbackDir = common::Normalize(GetPosition() - dynamic_cast<Actor*>(attacker)->GetPosition());
				cc->AddImpact(knockbackDir * 10.0f); // 플레이어는 NPC보다 약간 적은 넉백
			}

			return true;
		}
		return false;
	}
}
