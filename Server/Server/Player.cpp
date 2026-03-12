#include "pch.h"
#include "Player.h"

#include <algorithm>

#include "HitboxComponent.h"
#include "MapDataManager.h"
#include "server.h"
#include "PlayerControllerComponent.h"


namespace PIP::GAME
{
	Player::Player(int64_t owner_id)
		:
		_hp { 100 },
		_max_hp{ 100 },
		_level { 0 },
		_exp { 0 },
		_damage{ 10 },
		_owner_id{ owner_id }
	{
		SetFaction(Faction::FACTION_PLAYER);
		AddComponent<GAME::TransformComponent>();
		AddComponent<GAME::PlayerControllerComponent>(Layers::MOVING);

		
		auto hitbox = AddComponent<HitboxComponent>();
		float height = 1.8f;
		float radius = 0.5f;
		float halfCylinderHeight = 0.5f * height - radius;
		JPH::Ref<JPH::Shape> bodyShape = new JPH::CapsuleShape(halfCylinderHeight, radius);
		hitbox->AddHitbox("Body", bodyShape, { 0.0f, 0.9f, 0.0f });

		SetName("Player_" + std::to_string(_owner_id));
	}

	void Player::init(int64_t id)
	{
		_owner_id = id;
		_hp = 100;
		_max_hp = 100;
		_level = 0;
		_exp = 0;
		_damage = 10;
		_state = common::packet::OBJECT_STATE::IDLE;
		_hitCooldown = 0.0f;
		_history.clear();

		SetPosition({ 10.0f, 10.0f, 10.0f });
		if (auto pc = GetComponent<PlayerControllerComponent>()) {
			pc->SetMoveVelocity({ 0,0,0 });
			pc->AddImpact({ 0, 0, 0 });
		}
	}

	bool Player::ValidateHit(JPH::PhysicsSystem* physics, const JPH::Shape* attackShape,
	                         const JPH::RMat44& attackTransform, uint32_t timestamp, GameObject* attacker, int32_t damage)
	{
		if (_hitCooldown > 0.0f) return false; 

		
		auto snapshot = GetSnapshotAt(timestamp);

		
		std::string hitPart;
		auto hc = GetComponent<HitboxComponent>();
		if (!hc) {
			MYERROR("[HitTest] Player " << GetName() << " has NO HitboxComponent!");
			return false;
		}
		if (hc && hc->CheckCollision(physics, attackShape, attackTransform, snapshot, hitPart))
		{
			short old_hp = _hp;
			_hp -= static_cast<short>(damage);
			_hp = std::max<short>(_hp, 0);
			MYLOG("[Combat] HIT SUCCESS! Target: " << GetName() << " | Damage: " << damage << " | HP: " << old_hp << " ->" << _hp);

			if (auto cc = GetComponent<CharacterControllerComponent>()) {
				common::Vec3 currentPos = GetPosition();
				common::Vec3 attackerPos = dynamic_cast<Actor*>(attacker)->GetPosition();
				common::Vec3 dir = common::Normalize(currentPos - attackerPos);
				dir.y = 0;

				
				cc->AddImpact(dir * 20.0f);
			}
			_hitCooldown = 0.3f; 
			return true;
		}
		return false;
	}

	void Player::Update(float deltaTime, JPH::TempAllocator* allocator)
	{
		if (_hitCooldown > 0.0f) _hitCooldown -= deltaTime;
		Actor::Update(deltaTime, allocator);
	}
}
