#include "pch.h"
#include "NPC.h"
#include "AIComponent.h"
#include "PhysicsComponent.h"


namespace PIP
{
    NPC::NPC(int npc_id, int npc_type, int room_id, common::Vec3 position, int32_t hp)
        : GameObject(npc_id),
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
        AddComponent<PhysicsComponent>();

        // 4. AIComponent 추가 및 기존 Lua 스크립트 설정
        auto ai = AddComponent<AIComponent>();
        ai->SetLuaScript("Monster.lua");

        _lastUpdateTime = std::chrono::steady_clock::now();
    }

    NPC::~NPC()
    {
        // GameObject가 파괴될 때 모든 컴포넌트(unique_ptr)가 자동으로 안전하게 삭제됩니다.
		// 기존의 lua_close() 등은 AIComponent의 소멸자가 담당합니다.
    }
}

