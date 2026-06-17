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
		},
		{
			spawn = { x = 152.8, y = 6, z = -201 },
			max_hp = 150,
			patrols = { 
				{x = 151.8, y = 6.0, z = -203}, 
				{x = 151.8, y = 6.0, z = -187}, 
				{x = 158.6, y = 6.0, z = -153},
				{x = 153.1, y = 6.0, z = -146},
				{x = 171.1, y = 6.0, z = -148},
				{x = 202.2, y = 6.0, z = -144},
				{x = 209.2, y = 6.0, z = -101},
				{x = 184.2, y = 6.0, z = -86},
				{x = 184.2, y = 6.0, z = -86},
				{x = 151.2, y = 6.0, z = -119},
				{x = 157.2, y = 6.0, z = -152},
			}
		},
		{
			spawn = { x = 213.8, y = 6.0, z = -88 },
			max_hp = 150,
			patrols = { 
				{x = 179.8, y = 6.0, z = -179}, 
				{x = 213.8, y = 6.0, z = -35}, 
				{x = 167.38, y = 6.0, z = -33},
				{x = 161.38, y = 6.0, z = -59},
				{x = 171.38, y = 6.0, z = -90},
			}
		},
		{
			spawn = {x = 179.8, y = 6.0, z = -179}, 
			max_hp = 150,
			patrols = { 
				{x = 213.8, y = 6.0, z = -88 },
				{x = 213.8, y = 6.0, z = -35}, 
				{x = 167.38, y = 6.0, z = -33},
				{x = 161.38, y = 6.0, z = -59},
				{x = 171.38, y = 6.0, z = -90},
			}
		},
		{
			spawn = { x = 212.8, y = 10, z = -209 },
			max_hp = 150,
			patrols = { 
				{ x = 212.8, y = 10, z = -209 }, 
				{ x = 219.8, y = 6.0, z = -191 }, 
				{ x = 223.8, y = 6.0, z = -174 }, 
				{ x = 210.8, y = 6.0, z = -172 }, 
				{ x = 216.8, y = 6.0, z = -155 }, 
				{ x = 184.8, y = 6.0, z = -137 }, 
				{ x = 159.8, y = 6.0, z = -151 }, 
				{ x = 152.8, y = 6.0, z = -176 }, 
				{ x = 153.8, y = 6.0, z = -199 }, 
				{ x = 153.8, y = 6.0, z = -199 }, 
				{ x = 172.8, y = 6.0, z = -219 }, 
				{ x = 201.8, y = 6.0, z = -216 }, 
			}
		},
		{
			spawn = { x = 159.8, y = 10, z = -241 },
			max_hp = 150,
			patrols = { 
				{x = 143.8, y = 5.3, z = -262},
				{x = 121.8, y = 5.3, z = -257},
				{x = 122.8, y = 5.3, z = -234},
				{x = 142.8, y = 5.3, z = -223},
				{x = 138.8, y = 5.3, z = -210},
				{x = 154.8, y = 5.3, z = -208},
			}
		},
		{
			spawn = { x = 226.8, y = 10, z = -167 },
			max_hp = 150,
			patrols = { 
				{ x = 226.8, y = 6, z = -167 },
				{ x = 212.8, y = 6, z = -171 },
			}
		},
		{
			spawn = { x = 241.8, y = 10, z = -17.54 },
			max_hp = 150,
			patrols = { 
				{x = 241.5, y = 5.3, z = -107}, 
				{x = 186.3, y = 5.3, z = -103}, 
				{x = 155.4, y = 6.0, z = -102},
				{x = 157.0, y = 6.0, z = -23.32},
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