-- monster.lua

-- 맵 경계 (초기화 시 설정됨)
local map_min_x, map_max_x, map_min_z, map_max_z = 0, 0, 0, 0
local is_map_initialized = false

-- 현재 NPC의 목표 지점 (로컬 변수)
local target_x, target_z = 0, 0
local has_target = false

local MOVE_SPEED = 5.0 -- 초당 이동 속도

-- 랜덤 타겟 생성
local function GetRandomTarget()
    if not is_map_initialized then return 0, 0 end

    local padding = 50.0
    local min_x = map_min_x + padding
    local max_x = map_max_x - padding
    local min_z = map_min_z + padding
    local max_z = map_max_z - padding
    -- Log(string.format("Random Target Range: X[%f~%f], Z[%f~%f]", min_x, max_x, min_z, max_z))
    local tx = math.random() * (max_x - min_x) + min_x
    local tz = math.random() * (max_z - min_z) + min_z
    return tx, tz
end

-- Update 함수 (인자: deltaTime)
function Update(dt)
    -- Log(string.format("NPC Update Called! dt: %f", dt))
    -- 1. 맵 정보 초기화 (최초 1회)
    if not is_map_initialized then
        -- Log("Attempting to get Map Bounds...")
        local min_x, max_x, min_z, max_z = GetMapBounds()
        if min_x then
            map_min_x, map_max_x, map_min_z, map_max_z = min_x, max_x, min_z, max_z
            is_map_initialized = true
            -- Log(string.format("Lua Map Bounds Initialized: X[%f~%f], Z[%f~%f]", min_x, max_x, min_z, max_z))
        else
            -- Log("ERROR: Failed to get Map Bounds from C++! min_x was nil.")
            return -- 맵 정보 없으면 대기
        end
    end

    -- 2. 목표 설정 (없으면 생성)
    if not has_target then
        -- Log("NPC has no target. Setting new target...")
        target_x, target_z = GetRandomTarget()
        has_target = true
        -- Log(string.format("New target set: X=%f, Z=%f", target_x, target_z))
    end

    -- 3. 이동 로직
    local curr_x, curr_y, curr_z = GetPosition() -- 인자 없이 호출 (this 포인터는 내부 처리)
    -- Log(string.format("Current Position: X=%f, Y=%f, Z=%f", curr_x, curr_y, curr_z))
    local dx = target_x - curr_x
    local dz = target_z - curr_z
    local dist = math.sqrt(dx*dx + dz*dz)

    -- Log(string.format("Target: X=%f, Z=%f, Distance: %f", target_x, target_z, dist))
    if dist < 1.0 then
        -- 도착! 새 목표
        -- Log("NPC arrived at target. Setting new target...")
        target_x, target_z = GetRandomTarget()
        -- Log(string.format("New target set: X=%f, Z=%f", target_x, target_z))
    else
        -- 이동
        local move_dist = MOVE_SPEED * dt
        if move_dist > dist then move_dist = dist end

        local new_x = curr_x + (dx/dist * move_dist)
        local new_z = curr_z + (dz/dist * move_dist)

        SetPosition(new_x, curr_y, new_z)
        -- Log(string.format("Moving to: X=%f, Y=%f, Z=%f", new_x, curr_y, new_z))
    end
end