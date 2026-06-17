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

	struct QuestData
	{
		int32_t id;
		common::packet::QuestType type;
		std::string target_name; // e.g., "Tainer"
		int32_t target_count;
		int32_t reward_exp;
	};

	struct LeverSpawnData
	{
		int id;             // 레버 고유 ID (0-based)
		common::Vec3 pos;   // 레버 월드 좌표
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
		size_t GetNPCSpawnCount(common::packet::NPCType type) const; // [추가] 타입별 스폰 데이터 개수 반환
		const QuestData* GetQuestData(int32_t id) const; // [추가]
		const std::vector<LeverSpawnData>& GetLeverData() const;
		void LoadLeverData();
		
	private:
		void LoadNPCData();
		void LoadQuestData(); // [추가]

		static GAME::GameObject* GetOwner(lua_State* L);

		// Lua API Wrappers
		static int Lua_GetPosition(lua_State* L);
		static int Lua_SetPosition(lua_State* L);
		static int Lua_SetVelocity(lua_State* L); // [추가]
		static int Lua_GetMapBounds(lua_State* L);
		static int Lua_Log(lua_State* L);

		static int Lua_LoadNPCData(lua_State* L);
		static int Lua_LoadQuestData(lua_State* L); // [추가]
		static int Lua_LoadLeverData(lua_State* L);

	public:
		lua_State* L = nullptr; // Lua 상태를 저장하는 멤버 변수
		std::unordered_map<common::packet::NPCType, std::vector<NPCSpawnData>> _npcSpawnData; // NPC 유형별 스폰 데이터 맵
		std::unordered_map<int32_t, QuestData> _questData; // [추가] 퀘스트 원본 데이터
		std::vector<LeverSpawnData> _leverData;
	};
}