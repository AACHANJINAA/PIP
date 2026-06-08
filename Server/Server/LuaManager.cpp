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

	

	void LuaManager::Initialize()
	{
		if (L)
		{
            lua_close(L);
		}
        L = luaL_newstate();
        luaL_openlibs(L);
	}

	void LuaManager::Release()
	{
        if (L)
        {
            lua_close(L);
            L = nullptr;
		}
	}

	const NPCSpawnData* LuaManager::GetNPCSpawnData(common::packet::NPCType type, int index) const 
    {
        if (_npcSpawnData.contains(type) && index >= 0 && index < _npcSpawnData.at(type).size())
        {
            return &_npcSpawnData.at(type)[index];
        }
        return nullptr;
    }

	size_t LuaManager::GetNPCSpawnCount(common::packet::NPCType type) const 
	{
		if (_npcSpawnData.contains(type))
		{
			return _npcSpawnData.at(type).size();
		}
		return 0;
	}

	const QuestData* LuaManager::GetQuestData(int32_t id) const
	{
		auto it = _questData.find(id);
		if (it != _questData.end())
		{
			return &it->second;
		}
		return nullptr;
	}

	void LuaManager::LoadDataFile()
	{
        if (L)
        {
            LuaManager::Instance()->LoadNPCData();
            LuaManager::Instance()->LoadQuestData(); // [추가]
        }
	}

	void LuaManager::LoadNPCData()
	{
        lua_register(L, "API_LoadNPCData", Lua_LoadNPCData);
        int ret = luaL_dofile(L, "NPC_Data.lua");
        if (ret != LUA_OK)
        {
            const char* err = lua_tostring(L, -1);
            MYERROR("Failed to load NPC_Data.lua: " << err);
            lua_pop(L, 1);
        }
        else
        {
            // 3. Lua 스크립트에 정의된 LoadNPCData() 함수 호출
            lua_getglobal(L, "LoadNPCData");
            if (lua_isfunction(L, -1))
            {
                // 함수 실행 (인자 0개, 반환값 0개)
                if (lua_pcall(L, 0, 0, 0) != LUA_OK)
                {
                    const char* err = lua_tostring(L, -1);
                    MYERROR("Failed to call LoadNPCData in Lua: " << err);
                    lua_pop(L, 1);
                }
                else
                {
                    MYLOG("NPC Data Loaded from Lua successfully.");
                }
            }
            else
            {
                lua_pop(L, 1); // 함수가 없으면 스택에서 뺌
            }
        }
	}

	void LuaManager::LoadQuestData()
	{
		lua_register(L, "API_LoadQuestData", Lua_LoadQuestData);
		int ret = luaL_dofile(L, "QuestData.lua");
		if (ret != LUA_OK)
		{
			const char* err = lua_tostring(L, -1);
			MYERROR("Failed to load QuestData.lua: " << err);
			lua_pop(L, 1);
		}
		else
		{
			lua_getglobal(L, "LoadQuestData");
			if (lua_isfunction(L, -1))
			{
				if (lua_pcall(L, 0, 0, 0) != LUA_OK)
				{
					const char* err = lua_tostring(L, -1);
					MYERROR("Failed to call LoadQuestData in Lua: " << err);
					lua_pop(L, 1);
				}
				else
				{
					MYLOG("Quest Data Loaded from Lua successfully.");
				}
			}
			else
			{
				lua_pop(L, 1);
			}
		}
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
        auto nc = obj->GetComponent<GAME::NPCControllerComponent>();
        if (nc)
        {
            nc->SetVelocity({ x, y, z });
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
        const auto& [min_x,max_x,min_z,max_z] = MapDataManager::Instance()->GetWorldBounds();

        lua_pushnumber(L, min_x);
        lua_pushnumber(L, max_x);
        lua_pushnumber(L, min_z);
        lua_pushnumber(L, max_z);

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

    int LuaManager::Lua_LoadNPCData(lua_State* L)
	{
        // // 인자 순서: 1:type, 2:x, 3:y, 4:z, 5:patrols, 6:max_hp
        const char* type_str = lua_tostring(L, 1);
        float x = static_cast<float>(lua_tonumber(L, 2));
        float y = static_cast<float>(lua_tonumber(L, 3));
        float z = static_cast<float>(lua_tonumber(L, 4));
        int32_t max_hp = static_cast<int32_t>(lua_tointeger(L, 5));

        common::packet::NPCType type = common::packet::NPCType::error;
        std::string sType(type_str);
        if (sType == "Basic") type = common::packet::NPCType::Basic;
        else if (sType == "Tainer") type = common::packet::NPCType::Tainer;
        else if (sType == "MagicGuard") type = common::packet::NPCType::MagicGuard;
        else if (sType == "Elevator") type = common::packet::NPCType::Elevator;
        else if (sType == "QuestNPC") type = common::packet::NPCType::QuestNPC;

        NPCSpawnData data;
        data.pos = { x, y, z };
        data.max_hp = max_hp; // 기본값

        // Patrol points (table of {x,y,z})
        if (lua_istable(L, 6))
        {
            size_t len = lua_rawlen(L, 6);
            for (size_t i = 1; i <= len; ++i)
            {
                lua_rawgeti(L, 6, (int)i); // push patrols[i]
                if (lua_istable(L, -1))
                {
                    lua_getfield(L, -1, "x");
                    float px = static_cast<float>(lua_tonumber(L, -1));
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "y");
                    float py = static_cast<float>(lua_tonumber(L, -1));
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "z");
                    float pz = static_cast<float>(lua_tonumber(L, -1));
                    lua_pop(L, 1);

                    data.patrol_points.push_back({ px, py, pz });
                }
                lua_pop(L, 1);
            }
        }

        LuaManager::Instance()->_npcSpawnData[type].push_back(data);

        return 0;
	}

	int LuaManager::Lua_LoadQuestData(lua_State* L)
	{
		// 인자 순서: 1:id, 2:type_str, 3:target_name, 4:target_count, 5:reward_exp
		int32_t id = static_cast<int32_t>(lua_tointeger(L, 1));
		const char* type_str = lua_tostring(L, 2);
		const char* target_name = lua_tostring(L, 3);
		int32_t target_count = static_cast<int32_t>(lua_tointeger(L, 4));
		int32_t reward_exp = static_cast<int32_t>(lua_tointeger(L, 5));

		common::packet::QuestType type = common::packet::QuestType::KILL_MONSTER; // 기본값
		std::string sType(type_str);
		if (sType == "KILL_MONSTER") type = common::packet::QuestType::KILL_MONSTER;
		else if (sType == "GATHER_ITEM") type = common::packet::QuestType::GATHER_ITEM;
		else if (sType == "TALK_TO_NPC") type = common::packet::QuestType::TALK_TO_NPC;

		QuestData data;
		data.id = id;
		data.type = type;
		data.target_name = target_name ? target_name : "";
		data.target_count = target_count;
		data.reward_exp = reward_exp;

		LuaManager::Instance()->_questData[id] = data;

		return 0;
	}
}
