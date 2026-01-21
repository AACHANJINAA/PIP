#pragma once
namespace PIP
{
	namespace GAME
	{
		class GameObject;
	}
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
		static GAME::GameObject* GetOwner(lua_State* L);

		// Static C functions that will be exposed to Lua
		static int Lua_GetPosition(lua_State* L);
		static int Lua_SetPosition(lua_State* L);
		static int Lua_GetMapBounds(lua_State* L);
		static int Lua_Log(lua_State* L);
	};
}


