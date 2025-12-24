#include "pch.h"
#include "NPC.h"

namespace PIP
{
    NPC::NPC(int npc_id, int npc_type, int room_id, common::Vec3 position, int32_t hp)
        :   _npc_id{ npc_id },
			_npc_type{ npc_type },
	        _room_id{ room_id },
	        _hp{ hp },
	        _position{ position },
	        _velocity{ 0.0f, 0.0f, 0.0f },
	        _rotation{ 0.0f, 0.0f, 0.0f, 1.0f }
    {
        _name = "Monster_" + std::to_string(npc_id);

        // [추가] Lua State 초기화
        _L = luaL_newstate();
        if (_L)
        {
            // 함수 등록
            LuaManager::Instance()->RegisterFunctions(_L);

            // "this" 포인터를 전역 변수로 저장 (LuaManager에서 사용)
            lua_pushlightuserdata(_L, this);
            lua_setglobal(_L, "__npc_ptr");

            // 스크립트 로드 및 실행 (한 번 실행해서 초기화)
            if (luaL_dofile(_L, "Monster.lua") != LUA_OK)
            {
                const char* err = lua_tostring(_L, -1);
                MYERROR("[NPC " << npc_id << "] Lua Error: " << err << std::endl;);
                lua_pop(_L, 1);
            }
        }
    }

    NPC::~NPC()
    {
        // [추가] Lua State 정리
        if (_L)
        {
            lua_close(_L);
            _L = nullptr;
        }
    }

    void NPC::UpdateAI(float deltaTime)
    {
        if (!_L)
        {
            MYERROR("[NPC " << _npc_id << "] UpdateAI Error: Lua state is null!" << std::endl;)
	        return;
        }

        // Lua의 "Update" 함수 호출
        lua_getglobal(_L, "Update");

        if (lua_isfunction(_L, -1))
        {
            lua_pushnumber(_L, deltaTime); // 인자: deltaTime

            if (lua_pcall(_L, 1, 0, 0) != LUA_OK)
            {
                const char* err = lua_tostring(_L, -1);
                MYERROR("[NPC " << _npc_id << "] Lua Update Error: " << err << std::endl;);
                lua_pop(_L, 1);
            }
        }
        else
        {
            MYERROR("[NPC " << _npc_id << "] Error: 'Update' function not found in Lua script." << std::endl;)
            lua_pop(_L, 1); // 함수가 아니면 제거
        }
    }
}

