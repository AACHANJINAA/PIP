#include "pch.h"
#include "NPC.h"

#include <algorithm>
#include "AIComponent.h"
#include "BT_Nodes.h"
#include "HitboxComponent.h"
#include "NPCControllerComponent.h"
#include "PhysicsComponent.h"


namespace PIP::GAME
{
	NPC::NPC(int64_t npc_id, NPCType npc_type, int room_id, common::Vec3 position, int32_t hp)
		:
		_npc_type{ npc_type },
		_room_id{ room_id },
		_hp{ hp },
		_maxHp{_hp}
	{
		SetId(npc_id);
		SetFaction(Faction::FACTION_MONSTER);
		// 1. 기본 이름 설정 (GameObject 멤버)
		SetName("Monster_" + std::to_string(npc_id));

		// 2. TransformComponent 추가 및 초기화
		auto transform = AddComponent<TransformComponent>();
		transform->SetPosition(position);

		// 3. PhysicsComponent 추가
		// (실제 Jolt 바디 생성인 CreateBody는 Room에서 물리 시스템을 인자로 주어 호출해야 합니다)
		AddComponent<NPCControllerComponent>(Layers::NPC);

		// [추가] 히트박스 컴포넌트 추가 및 기본 히트박스 설정
		auto hitbox = AddComponent<HitboxComponent>();

		// NPC의 키가 1.8m라면, 그에 맞는 캡슐 히트박스를 생성해서 붙임
		float height = 1.8f;
		float radius = 0.5f;
		float halfCylinderHeight = 0.5f * height - radius;
		JPH::Ref<JPH::Shape> bodyShape = new JPH::CapsuleShape(halfCylinderHeight, radius);

		// "Body"라는 이름으로 캐릭터 중심(발바닥 위 0.9m)에 히트박스 부착
		hitbox->AddHitbox("Body", bodyShape, { 0.0f, 0.9f, 0.0f });


		// 4. AIComponent 추가 및 기존 Lua 스크립트 설정
		/*auto ai = AddComponent<AIComponent>();
		ai->SetLuaScript("Monster.lua");*/
		AddComponent<AIComponent>();
		NPC::SetupBT();


		_lastUpdateTime = std::chrono::steady_clock::now();
	}

	NPC::~NPC()
	{
		// GameObject가 파괴될 때 모든 컴포넌트(unique_ptr)가 자동으로 안전하게 삭제됩니다.
		// 기존의 lua_close() 등은 AIComponent의 소멸자가 담당합니다.
	}

	void NPC::SetupBT()
	{
		auto ai = GetComponent<AIComponent>();
		if (!ai) return;

		// [핵심] ai가 이미 관리 중인 블랙보드를 가져옵니다.
		auto bb = ai->GetBlackboard();

		// 이미 AIComponent::Initialize()에서 owner가 세팅되었을 것이지만 확실히 하기 위해 재세팅 가능
		bb->set("owner", static_cast<GameObject*>(this));
		bb->set("stuck_timer", 0.0f);
		bb->set("last_pos", GetPosition());
		bb->set("room_id", _room_id);

		// 1. 공격 종류별 설정 (Shape, Offset, Damage, Cooldown)
		// 일반 공격: 앞 2m 반경의 구체 형태
		NPCAttackConfig normalAtk;
		normalAtk.shape = new JPH::SphereShape(1.5f);
		normalAtk.posOffset = { 0.0f, 1.0f, 1.5f }; // 전방 1.5m 지점
		normalAtk.damage = 10;
		normalAtk.cooldown = 1.2f;
		normalAtk.animationState = common::packet::OBJECT_STATE::ATTACK1; // 공격 애니메이션 상태값

		// 강력한 공격: 전방 4m 길이의 박스 형태 (범위 공격)
		NPCAttackConfig heavyAtk;
		heavyAtk.shape = new JPH::BoxShape(JPH::Vec3(1.5f, 1.0f, 2.0f)); // 가로 3m, 세로 2m, 깊이 4m
		heavyAtk.posOffset = { 0.0f, 1.0f, 2.5f };
		heavyAtk.damage = 20;
		heavyAtk.cooldown = 4.0f;
		heavyAtk.animationState = common::packet::OBJECT_STATE::ATTACK1; // 공격 애니메이션 상태값

		BTBuilder builder;
		// 트리 설계:
		// 1. 목표가 있으면 이동한다. (이동 중 끼이면 실패하고 다음으로 넘어감)
		// 2. 목표가 없거나 이동에 실패하면 새 목표를 찾는다.
		// 트리 구성
		auto root = builder
			.selector() // 전체 동작 우선순위 결정
			// --- [우선순위 1] 전투 로직 ---
				.sequence()
					.leaf<Condition_HasEnemy>() // 타겟(공격자)이 있는가?
					.selector()
					// 1-1. 강력한 공격 시도 (사거리 4.5m)
						.sequence()
							.leaf<Condition_IsEnemyInRange>(4.5f)
							.leaf<Action_AttackEnemy>(heavyAtk) // 위에서 정의한 Config 전달
						.end()
					// 1-2. 일반 공격 시도 (사거리 2.5m)
					.sequence()
						.leaf<Condition_IsEnemyInRange>(2.5f)
						.leaf<Action_AttackEnemy>(normalAtk)
					.end()
					// 1-3. 공격 사거리 밖이면 추격
					.leaf<Action_ChaseEnemy>(6.0f, 1.5f) // 추격 속도 6.0
				.end()
			.end()
			// --- [우선순위 2] 배회/정찰 로직 (전투 중이 아닐 때) ---
			.sequence()
				.leaf<Condition_HasTarget>() // 이동 목적지가 있는가?
				.leaf<Action_MoveToTarget>(3.0f) // 정찰 속도 3.0
			.end()
			// --- [우선순위 3] 할 일 없으면 새로운 목적지 찾기 ---
			.leaf<Action_FindRandomTarget>()
			.end()
		.build();

		// 블랙보드 주입 및 등록
		root->set_blackboard(bb);
		ai->SetBehaviorTree(root);
	}

	bool NPC::IsDirty() const
	{
		// 1. 상태(애니메이션) 변화 체크 (최우선)
		// IDLE <-> WALK 등 상태가 바뀌면 즉시 패킷을 보내야 합니다.
		if (_state != _lastSentState)
		{
			return true;
		}

		// 2. 위치 변화 체크
		common::Vec3 currentPos = GetPosition();
		float distSq = (currentPos.x - _lastSentPos.x) * (currentPos.x - _lastSentPos.x) +
			(currentPos.y - _lastSentPos.y) * (currentPos.y - _lastSentPos.y) +
			(currentPos.z - _lastSentPos.z) * (currentPos.z - _lastSentPos.z);

		// 상태에 따른 임계값(Threshold) 차등 적용
		// 움직이는 중일 때는 5cm, 가만히 있을 때는 10cm 이상 변해야Dirty로 간주
		float thresholdSq = (_state == common::packet::OBJECT_STATE::IDLE) ? 0.01f : 0.0025f;

		if (distSq > thresholdSq)
		{
			return true;
		}

		// 3. 회전 변화 체크
		common::Vec4 currentRot = GetRotation();
		// 쿼터니언 차이 계산 (단순 거리 비교보다 안정적이지만, 여기선 가벼운 연산을 위해 거리로 유지)
		float rotDiff = (currentRot.x - _lastSentRot.x) * (currentRot.x - _lastSentRot.x) +
			(currentRot.y - _lastSentRot.y) * (currentRot.y - _lastSentRot.y) +
			(currentRot.z - _lastSentRot.z) * (currentRot.z - _lastSentRot.z) +
			(currentRot.w - _lastSentRot.w) * (currentRot.w - _lastSentRot.w);

		// 회전은 약 5~10도 이상 변했을 때만 전송 (너무 민감하면 지터링 발생)
		if (rotDiff > 0.05f)
		{
			return true;
		}

		// 4. 하트비트 (Heartbeat)
		// 아무 변화가 없더라도 1초에 한 번은 위치를 강제 동기화하여 누적 오차 방지
		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration<float>(now - _lastSentTime).count() > 1.0f) {
			return true;
		}

		return false;
	}

	bool NPC::ValidateHit(JPH::PhysicsSystem* physics, const JPH::Shape* attackShape,
	                      const JPH::RMat44& attackTransform, uint32_t timestamp, GameObject* attacker, int32_t damage)
	{
		if (_hitCooldown > 0) return false;
		// 1. 부모(Actor)의 히스토리에서 과거 데이터 가져오기
		auto snapshot = GetSnapshotAt(timestamp);

		// 2. 히트박스 컴포넌트 검증
		auto hitboxComp = GetComponent<HitboxComponent>();
		if (!hitboxComp) return false;

		std::string hitPart;
		if (hitboxComp->CheckCollision(physics, attackShape, attackTransform, snapshot, hitPart)) {

			// 3. 데미지 및 넉백 (Kinematic 방식)
			
			int32_t current_hp = GetHP();
			// 현재 HP보다 데미지가 크면 0, 아니면 차이만큼 차감
			int32_t new_hp = (current_hp > damage) ? (current_hp - damage) : 0;
			SetHP(new_hp);

			using namespace common::VectorHelper;
			common::Vec3 knockbackDir = common::Normalize(GetPosition() - dynamic_cast<Actor*>(attacker)->GetPosition());
			knockbackDir.y = 0.0f;

			if (auto cc = GetComponent<CharacterControllerComponent>()) {
				cc->AddImpact(knockbackDir * 15.0f);
			}

			// [반격 로직] 공격자를 타겟으로 설정
			auto ai = GetComponent<AIComponent>();
			if (ai) {
				ai->GetBlackboard()->set("target_enemy", attacker->GetId());
				//MYLOG("[HIT] " << GetName() << " part: " << hitPart << " set: target_enemy");
			}

			//MYLOG("[HIT] " << GetName() << " part: " << hitPart << " HP: " << GetHP());
			_hitCooldown = 0.2f;
			return true;
		}
		return false;
	}

	void NPC::Update(float deltaTime, JPH::TempAllocator* allocator)
	{
		// 1. 피격 쿨타임 감쇄 로직
		if (_hitCooldown > 0.0f) {
			_hitCooldown -= deltaTime;
			_hitCooldown = std::max(_hitCooldown, 0.0f);
		}
		Actor::Update(deltaTime, allocator);
	}

}

