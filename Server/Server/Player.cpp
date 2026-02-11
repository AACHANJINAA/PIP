#include "pch.h"
#include "Player.h"

#include <algorithm>

#include "HitboxComponent.h"
#include "MapDataManager.h"


namespace PIP::GAME
{
	Player::Player(int64_t owner_id)
		:
		_name {"DefaultName" },
		_hp { 100 },
		_max_hp{ 100 },
		_level { 0 },
		_exp { 0 },
		_damage{ 10 },
		Actor(owner_id),
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
			// 3. 실제 데미지 적용
			short old_hp = _hp;
			_hp -= static_cast<short>(damage);
			_hp = std::max<short>(_hp, 0);
			MYLOG("[HitTest] HIT SUCCESS! Part: " << hitPart << " HP: " << old_hp << " -> " << _hp);

			// 2. [핵심] 즉시 위치 이동 (넉백 좌표 계산)
			common::Vec3 currentPos = GetPosition();
			common::Vec3 attackerPos = dynamic_cast<Actor*>(attacker)->GetPosition();

			// 공격자 반대 방향으로 2.5m 이동
			common::Vec3 dir = common::Normalize(currentPos - attackerPos);
			dir.y = 0; // 수평 이동만
			common::Vec3 knockbackPos = currentPos + (dir * 2.5f);

			// 지형 높이 보정 (밀려난 곳이 절벽 밖이거나 땅 속일 수 있음)
			knockbackPos = MapDataManager::Instance()->AdjustPositionToGround(knockbackPos);

			// 서버상 위치 즉시 업데이트
			SetPosition(knockbackPos);

			MYLOG("[HitTest] HIT SUCCESS! New Pos: " << knockbackPos.x << ", " << knockbackPos.z);
			return true;
		}
		return false;
	}
}
