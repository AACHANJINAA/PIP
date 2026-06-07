-- NPC_Data.lua

NPC_Routes = {
	["Tainer"] = { -- 보스 테이너용 스폰 풀
		{
			spawn = { x = 0.0, y = 1.0, z = 0.0 }, -- 기본 위치
			max_hp = 500,
			patrols = {}
		}
	},
	["MagicGuard"] = { -- 일반 몬스터용
		{
			spawn = { x = 178.8, y = 10, z = -180 },
			max_hp = 150,
			patrols = { 
				{x = 179.8, y = 5.3, z = -179}, 
				{x = 185.3, y = 5.3, z = -173}, 
				{x = 198.4, y = 6.0, z = -176},
				{x = 193.0, y = 6.0, z = -190},
			}
		}
	},
	["Basic"] = { -- 일반 몬스터용
		{
			spawn = { x = -215.27, y = 10.59, z = -366.41 },
			max_hp = 100,
			patrols = {},
		},
		{
			spawn = { x = 18.0, y = 5.3, z = 180.0 },
			max_hp = 100,
			patrols = {},
		}
	},
	["QuestNPC"] = { -- 퀘스트 제공 NPC
		{
			spawn = { x = -214.0, y = 6.6, z = -366.42 },
			max_hp = 1000,
			patrols = {}
		}
	}
}

-- NPC Data를 C++ API로 로드 해주는 함수
function LoadNPCData()
	for npcType, routes in pairs(NPC_Routes) do
		for _, route in ipairs(routes) do
			local spawn = route.spawn
			local patrols = route.patrols
			local max_hp = route.max_hp or 100
			-- C++ API 호출 (예시)
			API_LoadNPCData(npcType, spawn.x, spawn.y, spawn.z, max_hp, patrols)
		end
	end
end