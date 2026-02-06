#include "pch.h"
#include "NPC.h"

#include <algorithm>
#include "AIComponent.h"
#include "BT_Nodes.h"
#include "HitboxComponent.h"
#include "PhysicsComponent.h"


namespace PIP::GAME
{
    NPC::NPC(int npc_id, int npc_type, int room_id, common::Vec3 position, int32_t hp)
        : Actor(npc_id),
        _npc_type{ npc_type },
        _room_id{ room_id },
        _hp{ hp }
    {
        // 1. 기본 이름 설정 (GameObject 멤버)
        SetName("Monster_" + std::to_string(npc_id));

        // 2. TransformComponent 추가 및 초기화
        auto transform = AddComponent<TransformComponent>();
        transform->SetPosition(position);

        // 3. PhysicsComponent 추가
        // (실제 Jolt 바디 생성인 CreateBody는 Room에서 물리 시스템을 인자로 주어 호출해야 합니다)
        AddComponent<CharacterControllerComponent>();

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
        SetupBT();


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

        // 블랙보드 초기화 (owner 정보 주입)
        auto bb = std::make_shared<Blackboard>();
        bb->set("owner", static_cast<GameObject*>(this));
        bb->set("stuck_timer", 0.0f);
        bb->set("last_pos", GetPosition());

        BTBuilder builder;

        // 트리 설계:
        // 1. 목표가 있으면 이동한다. (이동 중 끼이면 실패하고 다음으로 넘어감)
        // 2. 목표가 없거나 이동에 실패하면 새 목표를 찾는다.
        auto root = builder
            .selector() // or
				.sequence() // and
					.leaf<Condition_HasTarget>()
					.leaf<Action_MoveToTarget>(5.0f) // 이동 속도 5.0
				.end() // end sequence
    			.leaf<Action_FindRandomTarget>()
			.end()// end selector
			.build(); // build the tree

        // 블랙보드 주입 및 등록
        root->set_blackboard(bb);
        ai->SetBehaviorTree(root);
    }

    bool NPC::ValidateHit(JPH::PhysicsSystem* physics, const JPH::Shape* attackShape,
	    const JPH::RMat44& attackTransform, uint32_t timestamp, const common::Vec3& attackerPos, int32_t damage)
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
            SetHP(GetHP() - damage);

            using namespace common::VectorHelper;
            common::Vec3 knockbackDir = Normalize(GetPosition() - attackerPos);
            knockbackDir.y = 0.0f;

            if (auto cc = GetComponent<CharacterControllerComponent>()) {
                cc->AddImpact(knockbackDir * 15.0f);
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

