-- monster.lua

local map_min_x, map_max_x, map_min_z, map_max_z = 0, 0, 0, 0
local is_map_initialized = false

local target_x, target_z = 0, 0
local has_target = false

local MOVE_SPEED = 5.0 

-- 끼임 감지 변수
local stuck_timer = 0
local last_x, last_z = 0, 0
local CHECK_INTERVAL = 0.5
local acc_dt = 0

local function GetRandomTarget()
    if not is_map_initialized then return 0, 0 end
    local padding = 50.0
    -- math.random 범위 수정
    local tx = math.random() * (map_max_x - map_min_x - 2*padding) + map_min_x + padding
    local tz = math.random() * (map_max_z - map_min_z - 2*padding) + map_min_z + padding
    return tx, tz
end

function Update(dt)
    -- 1. 맵 초기화
    if not is_map_initialized then
        local min_x, max_x, min_z, max_z = GetMapBounds()
        if min_x then
            map_min_x, map_max_x, map_min_z, map_max_z = min_x, max_x, min_z, max_z
            is_map_initialized = true
        else
            return
        end
    end

    -- 2. 타겟 설정
    if not has_target then
        target_x, target_z = GetRandomTarget()
        has_target = true
        stuck_timer = 0
        acc_dt = 0
    end

    local curr_x, curr_y, curr_z = GetPosition()

    -- 3. 끼임 감지 (Stuck Check)
    acc_dt = acc_dt + dt
    if acc_dt >= CHECK_INTERVAL then
        local moved_dist = math.sqrt((curr_x - last_x)^2 + (curr_z - last_z)^2)
        local expected_dist = MOVE_SPEED * acc_dt
        
        if moved_dist < (expected_dist * 0.2) then
            stuck_timer = stuck_timer + acc_dt
        else
            stuck_timer = 0
        end

        if stuck_timer > 2.0 then
            target_x, target_z = GetRandomTarget()
            stuck_timer = 0
        end

        last_x, last_z = curr_x, curr_z
        acc_dt = 0
    end

    -- 4. 이동 (SetPosition 방식)
    local dx = target_x - curr_x
    local dz = target_z - curr_z
    local dist = math.sqrt(dx*dx + dz*dz)

    if dist < 1.0 then
        target_x, target_z = GetRandomTarget()
        has_target = true
    else
        local move_dist = MOVE_SPEED * dt
        if move_dist > dist then move_dist = dist end

        local new_x = curr_x + (dx/dist * move_dist)
        local new_z = curr_z + (dz/dist * move_dist)

        SetPosition(new_x, curr_y, new_z)
    end
end