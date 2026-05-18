#include "pch.h"
#include "CastleStage.h"
#include "Room.h"
#include "JoltSetup.h"
#include "MapDataManager.h"

namespace PIP::SERVER
{

    void CastleStage::on_initialize(Room* room)
    {
        MYLOG("[CastleStage] Initializing Physics Terrain...");

        auto* physicsSystem = room->GetPhysicsSystem();
        auto& bodyInterface = physicsSystem->GetBodyInterface();


        //TODO: 우리가 정의한 "CastleStage" 그룹에 속한 타일 포인터들만 가져옴
        //auto myTiles = MapDataManager::Instance()->GetTerrainGroup("CastleStage");

        // 1. 지형(Terrain Tiles) 로드
        const auto& terrainTiles = MapDataManager::Instance()->GetTerrainTiles();
        for (const auto& tile : terrainTiles)
        {
            if (!tile.shape) continue;
            JPH::BodyCreationSettings bodySettings(tile.shape, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                JPH::EMotionType::Static, Layers::NON_MOVING);

            JPH::Body* terrainBody = bodyInterface.CreateBody(bodySettings);
            _stageBodyIDs.push_back(terrainBody->GetID());
            bodyInterface.AddBody(terrainBody->GetID(), JPH::EActivation::DontActivate);
        }
        int terrain_num = static_cast<int>(_stageBodyIDs.size());
        MYLOG("[CastleStage] Physics terrain loaded. Count: " << terrain_num);


        // 2. 정적 메쉬(Static Mesh Collisions) 그룹 로드
        // 동일한 groupName을 사용하여 해당 타일에 속한 모든 오브젝트 메쉬를 가져옵니다.
        auto meshGroup = MapDataManager::Instance()->GetStaticMeshGroup("CastleStage");
        for (const auto* tile : meshGroup)
        {
            if (!tile->shape) continue;

            JPH::Vec3 correctBodyPos = tile->position + (tile->rotation * tile->shape->GetCenterOfMass());

            JPH::BodyCreationSettings settings(
                tile->shape,
                tile->position,
                tile->rotation,
                JPH::EMotionType::Static,
                Layers::NON_MOVING
            );

            /*if (tile->position.GetX() != correctBodyPos.GetX())
            {
                MYLOG("Actor: " << tile->meshName << " | Pivot: " << tile->position.GetX() << " | FinalBodyPos: " <<
                    correctBodyPos.GetX());
            }*/
            JPH::BodyID id = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::DontActivate);

            // 바디 생성 실패 체크 (개수 초과 등 발생 시)
            if (id.IsInvalid()) {
                MYERROR("[CastleStage] Physics Body creation FAILED for mesh: " << tile->meshName);
                continue;
            }
            /*if (tile->meshName == "SM_Rock2_mid")
            {
                MYLOG("[DEBUG] Raycasting for mesh ID: " << id.GetIndex());
                rayCheck({ correctBodyPos.GetX(), correctBodyPos.GetY() + 100, correctBodyPos.GetZ() },
                    {0,-1,0}, physicsSystem);
            }*/
            _stageBodyIDs.push_back(id);
        }

        MYLOG("[CastleStage] Physics Static Mesh objects loaded. Count: " << _stageBodyIDs.size() - terrain_num);

    }

    void CastleStage::on_enter(Room* room)
    {
        MYLOG("[CastleStage] Spawning NPCs and Boss...");

        // 1. 잡몹 500마리 무작위 배치 (CastleStage의 규칙)
        for (int i = 0; i < 1; ++i) {
            room->spawn_npc(GAME::NPCType::MagicGuard);
        }

        // 2. 보스 테이너 배치
        //room->spawn_npc(GAME::NPCType::Tainer, "Tainer the Gatekeeper");

        room->StartGame();
    }

    void CastleStage::update(Room* room, float dt)
    {
        // 보스 페이즈 체크나 스테이지 전용 기믹 업데이트
    }

    void CastleStage::on_exit(Room* room)
    {
        MYLOG("[CastleStage] Exiting Stage. Cleaning up...");

        // 1. 이 스테이지에서 만든 물리 바디 모두 제거
        auto& bodyInterface = room->GetPhysicsSystem()->GetBodyInterface();
        if (!_stageBodyIDs.empty()) {
            bodyInterface.RemoveBodies(_stageBodyIDs.data(), static_cast<int>(_stageBodyIDs.size()));
            bodyInterface.DestroyBodies(_stageBodyIDs.data(), static_cast<int>(_stageBodyIDs.size()));
            _stageBodyIDs.clear();
        }

        // 2. 스테이지 NPC들 제거
        room->ClearAllNPCs();
    }

    const common::Vec3 CastleStage::get_spawn_pos() const
    {
        return { 178.8, 10.0,-180.0 }; // 월드 중앙 등 원하는 위치로 반환
    }

}
