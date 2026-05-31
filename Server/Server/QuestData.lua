-- QuestData.lua
-- 퀘스트 정보 정의 파일

-- 퀘스트 타입
local QUEST_TYPE_KILL_MONSTER = "KILL_MONSTER"
local QUEST_TYPE_GATHER_ITEM = "GATHER_ITEM"
local QUEST_TYPE_TALK_TO_NPC = "TALK_TO_NPC"

function LoadQuestData()
    -- API_LoadQuestData(quest_id, quest_type, target_string, target_count, reward_exp)
    
    -- 예시: 1번 퀘스트 - 몬스터 "Basic" 10마리 잡기
    API_LoadQuestData(1, QUEST_TYPE_KILL_MONSTER, "Basic", 10, 500)
    
    -- 필요시 퀘스트 추가 가능
    -- API_LoadQuestData(2, QUEST_TYPE_KILL_MONSTER, "Basic", 20, 1000)
end
