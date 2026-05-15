-- NPC_Data.lua

NPC_Routes = {
    ["Tainer"] = { -- 보스 테이너용 스폰 풀
        {
            spawn = { x = 0.0, y = 1.0, z = 0.0 }, -- 기본 위치
            patrols = { {x=5, y=1, z=5}, {x=-5, y=1, z=-5} }
        },
        {
            spawn = { x = 10.0, y = 1.0, z = 10.0 }, -- 보조 위치
            patrols = { {x=15, y=1, z=15}, {x=5, y=1, z=5} }
        }
    },
    ["MagicGuard"] = { -- 일반 몬스터용
        {
            spawn = { x = 19.1, y = 5.3, z = 178.3 },
            patrols = { {x=25, y=5.3, z=180}, {x=10, y=5.3, z=175} }
        }
    }
}

function SetupNPCRandomRoute(npcType, npcPointer)
    local routes = NPC_Routes[npcType]
    if not routes or #routes == 0 then return false end

    -- 랜덤하게 하나 선택
    local selected = routes[math.random(1, #routes)]

    -- C++ 브릿지 함수 호출
    API_SetSpawnPos(npcPointer, selected.spawn.x, selected.spawn.y, selected.spawn.z)
    for _, p in ipairs(selected.patrols) do
        API_AddPatrolPoint(npcPointer, p.x, p.y, p.z)
    end
    return true
end