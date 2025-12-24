#pragma once
// Lua header path updated to match the new location inside Server/Server
extern "C" {
#include "lua-5.4.2_Win64_dll17_lib/include/lua.h"
#include "lua-5.4.2_Win64_dll17_lib/include/lualib.h"
#include "lua-5.4.2_Win64_dll17_lib/include/lauxlib.h"
}

namespace PIP
{
	class NPC;

	class LuaManager : public Singleton<LuaManager>
	{
		friend class Singleton<LuaManager>;
		LuaManager() = default;
		~LuaManager() = default;
	public:
		// 특정 Lua State에 C++ API 함수들을 등록해주는 함수
		void RegisterFunctions(lua_State* L);
	private:
		static NPC* GetOwnerNPC(lua_State* L);

		// Static C functions that will be exposed to Lua
		static int Lua_GetPosition(lua_State* L);
		static int Lua_SetPosition(lua_State* L);
		static int Lua_GetMapBounds(lua_State* L);
		static int Lua_Log(lua_State* L);
	};
}


