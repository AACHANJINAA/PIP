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
		class NPC;
		class GameObject;
	}
	struct NPCSpawnData 
	{
		common::Vec3 pos;
		int32_t max_hp;
		std::vector<common::Vec3> patrol_points;
	};

	class LuaManager : public Singleton<LuaManager>
	{
		friend class Singleton<LuaManager>;
		LuaManager() = default;
		~LuaManager() = default;

	public:
		void RegisterFunctions(lua_State* L);


		void Initialize() override;
		void Release() override;

		void LoadDataFile();

		const NPCSpawnData* GetNPCSpawnData(common::packet::NPCType type, int index) const;
		
	private:
		void LoadNPCData();

		static GAME::GameObject* GetOwner(lua_State* L);

		// Lua API Wrappers
		static int Lua_GetPosition(lua_State* L);
		static int Lua_SetPosition(lua_State* L);
		static int Lua_SetVelocity(lua_State* L); // [추가]
		static int Lua_GetMapBounds(lua_State* L);
		static int Lua_Log(lua_State* L);

		static int Lua_LoadNPCData(lua_State* L);

	public:
		lua_State* L = nullptr; // Lua 상태를 저장하는 멤버 변수
		std::unordered_map<common::packet::NPCType, std::vector<NPCSpawnData>> _npcSpawnData; // NPC 유형별 스폰 데이터 맵
	};
}