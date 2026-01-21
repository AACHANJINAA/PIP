#include "pch.h"
#include "LuaManager.h"

#include "MapDataManager.h"
#include "NPC.h"
#include "TransformComponent.h"
#include "GameObject.h"
#include "CharacterControllerComponent.h"

namespace PIP
{
	
	void LuaManager::RegisterFunctions(lua_State* L)
	{
		luaL_openlibs(L);

		lua_register(L, "GetPosition", Lua_GetPosition);
		lua_register(L, "SetPosition", Lua_SetPosition);
		lua_register(L, "SetVelocity", Lua_SetVelocity);
		lua_register(L, "GetMapBounds", Lua_GetMapBounds);
		lua_register(L, "Log", Lua_Log);
	}

	GAME::GameObject* LuaManager::GetOwner(lua_State* L)
	{
        lua_getglobal(L, "__gameObject"); 
        if (!lua_islightuserdata(L, -1))
        {
            lua_pop(L, 1);
            return nullptr;
        }
        GAME::GameObject* obj = static_cast<GAME::GameObject*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        return obj;
	}

	int LuaManager::Lua_GetPosition(lua_State* L)
    {
        GAME::GameObject* obj = GetOwner(L);
        if (!obj) return 0;

        auto transform = obj->GetComponent<GAME::TransformComponent>();
        if (!transform) return 0;

        common::Vec3 pos = transform->GetPosition();
        lua_pushnumber(L, pos.x);
        lua_pushnumber(L, pos.y);
        lua_pushnumber(L, pos.z);
        return 3;
    }

    int LuaManager::Lua_SetPosition(lua_State* L)
    {
	    GAME::GameObject* obj = GetOwner(L);
        if (!obj) return 0;

        float x = static_cast<float>(lua_tonumber(L, 1));
        float y = static_cast<float>(lua_tonumber(L, 2));
        float z = static_cast<float>(lua_tonumber(L, 3));

		auto transform = obj->GetComponent<GAME::TransformComponent>();
        if (!transform)
        {
            return 0;
        }
		transform->SetPosition(common::Vec3{ x, y, z });
        return 0;
    }

    int LuaManager::Lua_SetVelocity(lua_State* L)
    {
        GAME::GameObject* obj = GetOwner(L);
        if (!obj) return 0;

        float x = static_cast<float>(lua_tonumber(L, 1));
        float y = static_cast<float>(lua_tonumber(L, 2));
        float z = static_cast<float>(lua_tonumber(L, 3));

        // [수정] 컴포넌트 직접 조회 방식으로 변경하여 안정성 확보
        auto controller = obj->GetComponent<GAME::CharacterControllerComponent>();
        if (controller)
        {
            controller->SetVelocity({ x, y, z });
        }
        else
        {
            // 혹시 모르니 NPC 캐스팅도 남겨두거나 에러 로그 출력
             if (auto npc = dynamic_cast<GAME::NPC*>(obj))
             {
                 npc->SetVelocity({ x, y, z });
             }
             else
             {
                 // MYERROR("Lua SetVelocity Failed: Component Not Found");
             }
        }

        return 0;
    }

    int LuaManager::Lua_GetMapBounds(lua_State* L)
    {
        const auto& info = MapDataManager::Instance()->GetTerrainData().GetInfo();

        lua_pushnumber(L, info.min_x);
        lua_pushnumber(L, info.max_x);
        lua_pushnumber(L, info.min_z);
        lua_pushnumber(L, info.max_z);

        return 4;
    }

    int LuaManager::Lua_Log(lua_State* L)
    {
	    if (const char* msg = lua_tostring(L, 1))
        {
            MYLOG("[LuaLog] " << msg << std::endl);
        }
        return 0;
    }
}
