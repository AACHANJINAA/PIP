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
			spawn = { x = -232.0, y = 5.0, z = -324.0 },
			max_hp = 100,
			patrols = {},
		},
		{
			spawn = { x = -206.0, y = 6.5, z = -330.0 },
			max_hp = 100,
			patrols = {},
		},
		{
			spawn = { x = -175.0, y = 6.5, z = -341.0 },
			max_hp = 100,
			patrols = {},
		},
		{
			spawn = { x = -155.0, y = 10.0, z = -354.0 },
			max_hp = 100,
			patrols = {},
		},
		{
			spawn = { x = -232.0, y = 5.0, z = -324.0 },
			max_hp = 100,
			patrols = {},
		},
		{
			spawn = { x = -186.0, y = 8.0, z = -380.59 },
			max_hp = 100,
			patrols = {},
		},
		{
			spawn = { x = -232.0, y = 5.0, z = -324.0 },
			max_hp = 100,
			patrols = {},
		},
		{
			spawn = { x = -222.0, y = 7.0, z = -384.17 },
			max_hp = 100,
			patrols = {},
		},
		{
			spawn = { x = -234.0, y = 9.0, z = -409.0 },
			max_hp = 100,
			patrols = {},
		},
		{
			spawn = { x = -263.0, y = 6.0, z = -354.0 },
			max_hp = 100,
			patrols = {},
		},
		{
			spawn = { x = -263.0, y = 6.0, z = -359.0 },
			max_hp = 100,
			patrols = {},
		},
		{
			spawn = { x = -270.0, y = 6.0, z = -359.0 },
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