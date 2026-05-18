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
            spawn = { x = 19.1, y = 5.3, z = 178.3 },
            max_hp = 150,
            patrols = { {x=25, y=5.3, z=180}, {x=10, y=5.3, z=175} }
        }
    },
    ["Basic"] = { -- 일반 몬스터용
        {
            spawn = { x = -212.0, y = 6.43, z = -360.0 + 5.0 },
            max_hp = 100,
            patrols = { {x=30, y=5.3, z=185}, {x=15, y=5.3, z=170} },
        },
        {
            spawn = { x = 18.0, y = 5.3, z = 180.0 },
            max_hp = 100,
            patrols = { {x=25, y=5.3, z=185}, {x=10, y=5.3, z=175} },
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