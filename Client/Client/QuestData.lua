-- QuestData.lua
-- 퀘스트 정보 정의 파일

-- 퀘스트 타입
local QUEST_TYPE_KILL_MONSTER = "KILL_MONSTER"
local QUEST_TYPE_GATHER_ITEM = "GATHER_ITEM"
local QUEST_TYPE_TALK_TO_NPC = "TALK_TO_NPC"

function LoadQuestData()
    -- API_LoadQuestData(quest_id, quest_type, target_string, target_count, reward_exp)
    
    -- 예시: 1번 퀘스트 - 몬스터 "Tainer" 10마리 잡기
    API_LoadQuestData(1, QUEST_TYPE_KILL_MONSTER, "Tainer", 10, 500)
    
end
