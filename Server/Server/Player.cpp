#include "pch.h"
#include "Player.h"
#include "HitboxComponent.h"
#include "InventoryComponent.h"
#include "LuaManager.h"
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
		_damage{ 50 },
		_owner_id{ owner_id },
		_actionId{0}
	{
		SetFaction(Faction::FACTION_PLAYER);
		AddComponent<GAME::TransformComponent>();
		AddComponent<GAME::PlayerControllerComponent>(Layers::MOVING);
		AddComponent<GAME::InventoryComponent>();
		
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
		_mp = 100; // [추가]
		_max_mp = 100; // [추가]
		_level = 0;
		_exp = 0;
		_damage = 50;
		_history.clear();
		
		ResetState(); // [추가] 공통 초기화 로직 호출

		SetPosition({ 0.0f, 10.0f, 0.0f });
	}

	void Player::ResetState()
	{
		_state = common::packet::EntityState::IDLE;
		_actionId = 0;
		_hitCooldown = 0.0f;
		_dashCooldownTimer = 0.0f;

		// 잡기 상태 초기화
		SetGrabbedById(-1);
		SetGrabSlot(-1);

		// 물리 속성 초기화
		if (auto pc = GetComponent<PlayerControllerComponent>()) {
			pc->SetMoveVelocity({ 0, 0, 0 });
			pc->AddImpact({ 0, 0, 0 });
		}
	}

	bool Player::IsDirty()
	{
		bool isMoved = common::DistanceSq(GetPosition(), _lastSentPos) > 0.0001f;
		bool isStateChanged = (GetState() != _lastSentState);
		bool isRotated = !common::IsEqual(GetRotation(), _lastSentRot);
		bool isGrabChanged = (GetGrabbedById() != _lastSentGrabbedById) || (GetGrabSlot() != _lastSentGrabSlot);
		bool isHpChanged = (_hp != _lastSentHp); // [추가] DoT 시각화를 위해 HP 변화 감지
		bool isMpChanged = (_mp != _lastSentMp); // [추가]
		return isMoved || isStateChanged || isRotated || isGrabChanged || isHpChanged || isMpChanged;
	}

	void Player::SyncSentData()
	{
		_lastSentPos = GetPosition();
		_lastSentState = GetState();
		_lastSentRot = GetRotation();
		_lastSentGrabbedById = GetGrabbedById();
		_lastSentGrabSlot = GetGrabSlot();
		_lastSentHp = _hp; // [추가]
		_lastSentMp = _mp; // [추가]
	}

	void Player::addMaterial(common::packet::ItemId item_id, uint32_t count)
	{
		auto inventory = GetComponent<InventoryComponent>();
		if (inventory)
		{
			inventory->add_material(item_id, count);
		}
	}

	void Player::removeMaterial(common::packet::ItemId item_id, uint32_t count)
	{
		auto inventory = GetComponent<InventoryComponent>();
		if (inventory)
		{
			inventory->remove_material(item_id, count);
		}
	}

	common::packet::SC_PACKET_MOVE Player::CreateMovePacket() const
	{
		common::packet::SC_PACKET_MOVE res;
		res._type = common::packet::PacketType::S2C_P_MOVE;
		res._size = sizeof(res);
		res._id = GetId();
		res._position = GetPosition();
		res._rotation = GetRotation();
		res._state = _state;

		// [핵심 보정] 클라이언트의 예측 이동(Dead Reckoning) 오류 방지
		common::Vec3 sendVelocity = GetVelocity();

		if (_state == common::packet::EntityState::IDLE || _state == common::packet::EntityState::ACTION)
		{
			// 가만히 있거나 모션 중(공격, 스킬 등)일 때는 속도를 완벽한 0으로 강제 고정
			sendVelocity = { 0.0f, 0.0f, 0.0f };
		}
		else if (_state == common::packet::EntityState::MOVE || _state == common::packet::EntityState::RUN)
		{
			// 땅에서 이동 중일 때는 서버의 미세한 하강 속도(StickToFloor)를 제거하고 수평 속도만 전송
			// (주의: 점프/추락 상태가 따로 있다면 그 상태에서는 Y 속도를 그대로 보내야 함)
			if (sendVelocity.y < 0.0f) {
				sendVelocity.y = 0.0f;
			}
		}

		res._velocity = sendVelocity; // 정제된 속도 전송

		res._action_id = _actionId;
		res._client_tick = _lastClientTick;
		res._grabbed_by_id = _grabbedById; // [추가]
		res._grab_slot = _grabSlot;         // [추가]
		res._hp = _hp;                     // [추가] 실시간 HP 동기화
		res._mp = _mp;                     // [추가] 실시간 MP 동기화
		return res;
	}

	common::packet::QuestUpdateInfo Player::AddQuest(int32_t quest_id)
	{
		const QuestData* qData = LuaManager::Instance()->GetQuestData(quest_id);
		if (!qData) return {};

		common::packet::QuestUpdateInfo info;
		info._quest_id = quest_id;
		info._state = common::packet::QuestState::IN_PROGRESS;
		info._current_count = 0;
		info._target_count = qData->target_count;

		_quests[quest_id] = info;
		return info;
	}

	common::packet::QuestUpdateInfo Player::CompleteQuest(int32_t quest_id)
	{
		auto it = _quests.find(quest_id);
		if (it != _quests.end())
		{
			it->second._state = common::packet::QuestState::REWARDED;
			
			// 보상 지급 로직 추가 가능 (경험치 등)
			const QuestData* qData = LuaManager::Instance()->GetQuestData(quest_id);
			if (qData) {
				_exp += qData->reward_exp;
			}

			return it->second;
		}
		return {};
	}

	common::packet::QuestUpdateInfo Player::UpdateQuestProgress(int32_t quest_id, int32_t current_count)
	{
		auto it = _quests.find(quest_id);
		if (it != _quests.end())
		{
			it->second._current_count = current_count;
			if (it->second._current_count >= it->second._target_count)
			{
				it->second._current_count = it->second._target_count;
				it->second._state = common::packet::QuestState::COMPLETED;
			}

			return it->second;
		}
		return {};
	}

	common::packet::QuestUpdateInfo* Player::GetQuest(int32_t quest_id)
	{
		auto it = _quests.find(quest_id);
		if (it != _quests.end())
		{
			return &it->second;
		}
		return nullptr;
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
			_hitCooldown = 0.5; 
			return true;
		}
		return false;
	}

	void Player::Update(float deltaTime, JPH::TempAllocator* allocator)
	{
		if (_hitCooldown > 0.0f) _hitCooldown -= deltaTime;
		if (_dashCooldownTimer > 0.0f) _dashCooldownTimer -= deltaTime;

		// [추가] 마나 자동 회복 (1초에 8)
		if (_mp < _max_mp) {
			_mpRegenTimer += deltaTime;
			if (_mpRegenTimer >= 1.0f) {
				_mpRegenTimer -= 1.0f;
				_mp = std::min(_mp + 8, _max_mp);
			}
		}

		Actor::Update(deltaTime, allocator);
	}

	void Player::PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator)
	{
		GameObject::PhysicsUpdate(deltaTime, allocator);
	}
}
