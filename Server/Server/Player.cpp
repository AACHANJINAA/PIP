#include "pch.h"
#include "Player.h"

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
		Actor((int)owner_id),
		_owner_id{ owner_id }
	{
		// 1. Transform 추가
		AddComponent<GAME::TransformComponent>();

		// 2. 물리 컨트롤러 추가 (플레이어도 이제 물리 적용!)
		AddComponent<GAME::CharacterControllerComponent>();

		SetName("Player_" + std::to_string(owner_id));
	}

	bool Player::ValidateHit(JPH::PhysicsSystem* physics, const JPH::Shape* attackShape,
		const JPH::RMat44& attackTransform, uint32_t timestamp, const common::Vec3& attackerPos, int32_t damage)
	{
		// 1. 자신의 히스토리 버퍼에서 timestamp에 맞는 위치/회전(Snapshot) 추출
		auto snapshot = GetSnapshotAt(timestamp);

		// 2. HitboxComponent에게 정밀 판정 위임
		std::string hitPart;
		if (auto hc = GetComponent<HitboxComponent>()) {
			if (hc->CheckCollision(physics, attackShape, attackTransform, snapshot, hitPart)) {
				// 3. 피격 처리 (데미지, 넉백 등)
				//TODO: ApplyDamage(damage);
				//GetComponent<CharacterControllerComponent>()->AddImpact(knockback);
				return true;
			}
		}
		return false;
	}
}
