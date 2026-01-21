#pragma once

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace PIP
{
	namespace GAME
	{
		class GameObject;
	}

	class LuaManager : public Singleton<LuaManager>
	{
		friend class Singleton<LuaManager>;
	private:
		LuaManager() = default;
		~LuaManager() = default;

	public:
		void RegisterFunctions(lua_State* L);

	private:
		static GAME::GameObject* GetOwner(lua_State* L);

		// Lua API Wrappers
		static int Lua_GetPosition(lua_State* L);
		static int Lua_SetPosition(lua_State* L);
		static int Lua_SetVelocity(lua_State* L); // [추가]
		static int Lua_GetMapBounds(lua_State* L);
		static int Lua_Log(lua_State* L);
	};
}