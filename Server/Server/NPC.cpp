#include "pch.h"
#include "NPC.h"

#include <algorithm>
#include "AIComponent.h"
#include "BT_Nodes.h"
#include "HitboxComponent.h"
#include "NPCControllerComponent.h"
#include "PhysicsComponent.h"
#include "LuaManager.h"

namespace PIP::GAME
{
	NPC::NPC(int64_t npc_id, NPCType npc_type, int room_id, common::Vec3 position, int32_t hp)
		:
		_npc_type{ npc_type },
		_room_id{ room_id },
		_hp{ hp },
		_maxHp{ _hp },
		_spawnPosition{ position }
	{
		SetId(npc_id);
		SetFaction(Faction::FACTION_MONSTER);
		// 1. 기본 이름 설정 (GameObject 방식)
		if (_npc_type == common::packet::NPCType::DynamicBox)
			SetName("DynamicBox_" + std::to_string(npc_id));
		else
			SetName("Monster_" + std::to_string(npc_id));

		// 2. TransformComponent 추가 및 초기화
		auto transform = AddComponent<TransformComponent>();
		transform->SetPosition(position);

		if (_npc_type == common::packet::NPCType::DynamicBox)
		{
			// 다이나믹 박스는 단순 물리 컴포넌트만 부착
			_physicsComponent = AddComponent<PhysicsComponent>();
			_transform = transform;
			
			// 무기 타격을 받기 위해 히트박스 추가
			_hitboxComponent = AddComponent<HitboxComponent>();
			JPH::Ref<JPH::Shape> boxHitShape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
			_hitboxComponent->AddHitbox("Body", boxHitShape, { 0.0f, 0.0f, 0.0f });

			_lastUpdateTime = std::chrono::steady_clock::now();
			return; // AI, 컨트롤러 부착 생략
		}

		// 3. PhysicsComponent 추가
		// (실제 Jolt 바디 생성은 CreateBody를 Room에서 물리 시스템을 인자로 넣어 호출해야 합니다)
		AddComponent<NPCControllerComponent>(Layers::NPC);

		// [추가] 히트박스 컴포넌트 추가 및 기본 히트박스 설정
		_hitboxComponent = AddComponent<HitboxComponent>();

		// NPC의 키가 1.8m라면, 그에 맞는 캡슐 히트박스를 생성해서 등록
		float height = 1.8f;
		float radius = 0.5f;
		float halfCylinderHeight = 0.5f * height - radius;
		JPH::Ref<JPH::Shape> bodyShape = new JPH::CapsuleShape(halfCylinderHeight, radius);

		// "Body"라는 이름으로 캐릭터 중심(발바닥 위 0.9m)에 히트박스 등록
		_hitboxComponent->AddHitbox("Body", bodyShape, { 0.0f, 0.9f, 0.0f });

		// 4. AIComponent 추가 및 전용 Lua 스크립트 연결
		AddComponent<AIComponent>();
		
		// 최적화를 위해 컴포넌트 포인터 캐싱
		_npcController = GetComponent<NPCControllerComponent>();
		_transform = GetComponent<TransformComponent>();
		_aiComponent = GetComponent<AIComponent>();

		NPC::SetupBT();

		_lastUpdateTime = std::chrono::steady_clock::now();
	}

	NPC::~NPC()
	{
		// GameObject가 파괴될 때 모든 컴포넌트(unique_ptr)가 자동적으로 해제됩니다.
	}

	void NPC::SetupBT()
	{
		if (!_aiComponent) return;

		_aiComponent->Initialize();

		// [다시] ai에 이미 설정된 블랙보드를 가져옵니다.
		auto bb = _aiComponent->GetBlackboard();

		// 이미 AIComponent::Initialize()에서 owner가 설정되었겠지만 명확히 하기 위해 재설정
		bb->set("owner", static_cast<GameObject*>(this));
		bb->set("owner_npc", static_cast<NPC*>(this)); // [수정] 명시적 캐스팅
		bb->set("stuck_timer", 0.0f);
		bb->set("last_pos", GetPosition());
		bb->set("room_id", _room_id);

		// 1. 공격 설정 정의 (Shape, Offset, Damage, Cooldown)
		// 일반 공격: 앞 1.5m 반경의 구체 형태
		AttackConfig normalAtk;
		normalAtk.shape = new JPH::SphereShape(0.8f);
		normalAtk.posOffset = { 0.0f, 1.0f, 1.0f }; // 전방 1.0m 지점
		normalAtk.damage = 10;
		normalAtk.cooldown = 1.2f;
		normalAtk.animationDuration = 0.8f; // [추가] 일반 공격 애니메이션 길이
		normalAtk.attackTiming = 0.4f;
		normalAtk.entityState = common::packet::EntityState::ACTION; // 공격 애니메이션 상태로
		normalAtk.actionId = 1; // 일반 공격 행동 ID

		// 강력한 공격: 전방 3m 범위의 박스 형태 (강한 일격)
		AttackConfig heavyAtk;
		heavyAtk.shape = new JPH::BoxShape(JPH::Vec3(1.0f, 1.0f, 0.8f));
		heavyAtk.posOffset = { 0.0f, 1.0f, 1.0f };
		heavyAtk.damage = 20;
		heavyAtk.cooldown = 4.0f;
		heavyAtk.animationDuration = 1.2f; // [추가] 강공격 애니메이션 길이
		heavyAtk.attackTiming = 0.6f;
		heavyAtk.entityState = common::packet::EntityState::ACTION; // 공격 애니메이션 상태로
		heavyAtk.actionId = 1; // [수정] 강공격 행동 ID 분리

		BTBuilder builder;
		auto root = builder
			.sequence()
			.leaf<Condition_IsAlive>()
			.leaf<Condition_IsHitted>(DecoratorType::Inverter)
			.selector() // 전체 로직 우선순위 결정
				// --- [우선순위 1] 전투 로직 ---
				.sequence()
					.leaf<Condition_HasEnemy>() // 타겟(적군)이 있는가?
					.leaf<Condition_IsTargetInLeashRange>(25.0f) // 25m 내에 있으면 추격 유지
					.selector()
					// 1-1. 강력한 공격 시도 (사거리 2.5m)
						.sequence()
							.leaf<Condition_IsEnemyInRange>(1.5f)
							.leaf<Action_AttackEnemy>(heavyAtk)
						.end()
					// 1-2. 일반 공격 시도 (사거리 1.5m)
					.sequence()
						.leaf<Condition_IsEnemyInRange>(1.5f)
						.leaf<Action_AttackEnemy>(normalAtk)
					.end()
					// 1-3. 타겟 사거리 밖이면 네비메쉬 추격
					.sequence()
						.leaf<Action_UpdateEnemyPosToTarget>()
						.leaf<Action_FindPath>("MainStage_NavMesh")
						.leaf<Action_FollowPath>(6.0f) // 추격 속도 6.0
					.end()
				.end()
			.end()
			// --- [우선순위 2] 배회/이동 로직 (전투 중이 아닐 때) ---
			.sequence()
				.selector()
					.leaf<Condition_HasTarget>() // 이미 배회 목적지가 있는가?
					.leaf<Action_SetRandomTargetAroundSpawn>(10.0f) // 없다면 스폰 위치 기준 10m 이내 랜덤 세팅
				.end()
				.leaf<Action_FindPath>("MainStage_NavMesh") // 목적지로 가는 네비메쉬 길찾기
				.leaf<Action_FollowPath>(3.0f) // 배회 속도 3.0으로 이동
			.end()
		.end()
		.build();

		// 블랙보드 연결 및 루트 설정
		root->set_blackboard(bb);
		_aiComponent->SetBehaviorTree(root);
	}



	bool NPC::IsDirty() const
	{
		// 1. 상태(애니메이션) 변화 체크 (최우선)
		if (_state != _lastSentState) return true;
		if (_actionId != _lastSentActionId) return true;
		if (GetGrabbedById() != _lastSentGrabbedById) return true;
		if (GetGrabSlot() != _lastSentGrabSlot) return true;

		// 2. 위치 변화 체크 (캐싱된 포인터 사용)
		common::Vec3 currentPos = GetPosition();
		float dx = currentPos.x - _lastSentPos.x;
		float dy = currentPos.y - _lastSentPos.y;
		float dz = currentPos.z - _lastSentPos.z;
		float distSq = dx * dx + dy * dy + dz * dz;

		// 상태에 따른 임계값(Threshold) 적용
		float thresholdSq = (_state == common::packet::EntityState::IDLE) ? 0.01f : 0.0025f;

		if (distSq > thresholdSq) return true;

		// 3. 회전 변화 체크
		common::Quat currentRot = GetRotation();
		float rx = currentRot.x - _lastSentRot.x;
		float ry = currentRot.y - _lastSentRot.y;
		float rz = currentRot.z - _lastSentRot.z;
		float rw = currentRot.w - _lastSentRot.w;
		float rotDiffSq = rx * rx + ry * ry + rz * rz + rw * rw;

		if (rotDiffSq > 0.0001f) return true;

		return false;
	}

	bool NPC::ValidateHit(JPH::PhysicsSystem* physics, const JPH::Shape* attackShape, const JPH::RMat44& attackTransform, uint32_t timestamp, GameObject* attacker, int32_t damage)
	{
		if (_hitCooldown > 0) return false;
		// 1. 부모(Actor)의 히스토리에서 해당 시점의 트랜스폼 가져오기
		auto snapshot = GetSnapshotAt(timestamp);

		// 2. 히트박스 컴포넌트 호출
		if (_hitboxComponent) {
			std::string hitPart;
			if (_hitboxComponent->CheckCollision(physics, attackShape, attackTransform, snapshot, hitPart)) {
				// 피격 성공!
				using namespace common::VectorHelper;
				common::Vec3 knockbackDir = common::Normalize(GetPosition() - dynamic_cast<Actor*>(attacker)->GetPosition());
				knockbackDir.y = 0.0f;

				if (_npc_type == common::packet::NPCType::DynamicBox) {
					// 다이나믹 박스는 체력이 닳지 않으며, 물리적 힘(속도)만 받음
					if (auto pc = GetComponent<PhysicsComponent>()) {
						pc->SetVelocity(knockbackDir * 20.0f + common::Vec3(0, 5.0f, 0)); // 위로 살짝 띄우며 날리기
					}
					return true;
				}

				_hp -= damage;
				_hp = std::max(_hp, 0);

				_hitCooldown = 0.5f; // 0.5초 피격 쿨다운
				SetState(common::packet::EntityState::HITTED);

				if (auto cc = GetComponent<CharacterControllerComponent>()) {
					cc->AddImpact(knockbackDir * 15.0f);
					if (auto nc = dynamic_cast<NPCControllerComponent*>(cc)) {
						nc->SetVelocity({ 0, 0, 0 }); // [추가] AI 이동 관성 제거하여 넉백 저항 없애기
					}
				}
				// 3. AI 타겟 설정 (나를 때린 놈을 타겟으로)
				if (auto ai = GetComponent<AIComponent>()) {
					auto bb = ai->GetBlackboard();
					if (attacker) {
						bb->set("target_enemy", attacker->GetId());
						// [수정] 피격 시 즉각 추격을 위해 기존 경로 및 길찾기 쿨타임 초기화
						bb->set("path_search_cooldown", 0.0f);
						bb->set("path", std::vector<common::Vec3>{});
					}
				}

				return true;
			}
		}
		return false;
	}

	void NPC::ResetForRespawn() 
	{
		_hitCooldown = 0.0f;
		_actionId = 0;
		_state = common::packet::EntityState::IDLE;
		// dirty 필드를 엉뚱한 값으로 세팅 → 부활 직후 반드시 패킷 발송
		_lastSentPos = { 9999.f, 9999.f, 9999.f };
		_lastSentRot = { 0.f, 0.f, 0.f, -1.f };
		_lastSentState = common::packet::EntityState::DEAD;
		_lastSentActionId = -1;

		// [수정] 부활 시 AI 블랙보드 초기화 (이전 타겟이나 경로가 남아있어 배회를 안하는 버그 수정)
		if (_aiComponent) {
			auto bb = _aiComponent->GetBlackboard();
			if (bb) {
				bb->set("target_enemy", std::any());
				bb->set("target_pos", std::any());
				bb->set("path", std::vector<common::Vec3>{});
				bb->set("path_search_cooldown", 0.0f);
				bb->set("heal_timer", 0.0f);
				// actual_spawn_pos는 유지 (스폰 위치 기준 방황을 위해)
			}
		}
	}

	void NPC::ApplySpawnData(const NPCSpawnData& data)
	{
		_hp = data.max_hp;
		_maxHp = data.max_hp;
		_spawnPosition = data.pos;
		_patrolPoints = data.patrol_points;

		SetPosition(data.pos);

		// AI 블랙보드에도 순찰 지점 데이터 동기화
		if (_aiComponent)
		{
			auto bb = _aiComponent->GetBlackboard();
			bb->set("patrol_points", _patrolPoints);
			bb->set("patrol_index", 0);
		}
	}

	void NPC::Update(float deltaTime, JPH::TempAllocator* allocator)
	{
		// 1. 피격 쿨다운 처리
		if (_hitCooldown > 0.0f) {
			_hitCooldown -= deltaTime;
			if (_hitCooldown < 0.0f) _hitCooldown = 0.0f;
		}

		// 2. 피격 상태 해제
		if (_state == common::packet::EntityState::HITTED && _hitCooldown <= 0) {
			SetState(common::packet::EntityState::IDLE);
		}

		// 3. 비전투 시 체력 회복 및 글로벌 공격 쿨타임 로직
		if (_aiComponent) {
			auto bb = _aiComponent->GetBlackboard();
			if (bb) {
				// 글로벌 공격 쿨타임 감소 (항상 진행)
				float atkCD = bb->has("global_attack_cooldown") ? bb->get<float>("global_attack_cooldown") : 0.0f;
				if (atkCD > 0.0f) {
					bb->set("global_attack_cooldown", atkCD - deltaTime);
				}

				if (!bb->has("target_enemy")) {
					float healTimer = bb->has("heal_timer") ? bb->get<float>("heal_timer") : 0.0f;
					healTimer += deltaTime;
					if (healTimer >= 5.0f) {
						healTimer -= 5.0f;
						if (_hp < _maxHp) {
							_hp = std::min(_hp + static_cast<int32_t>(_maxHp * 0.1f), _maxHp);
						}
					}
					bb->set("heal_timer", healTimer);
				} else {
					bb->set("heal_timer", 0.0f);
				}
			}

			_aiComponent->Update(deltaTime, allocator);
		}
		
		// NPC 전용 로직 업데이트
	}

}
