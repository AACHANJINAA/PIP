#include "pch.h"
#include "BossStage.h"
#include "Room.h"
#include "JoltSetup.h"
#include "MapDataManager.h"

namespace PIP::SERVER
{
    void BossStage::on_initialize(Room* room)
    {
        MYLOG("[BossStage] Initializing Physics Terrain...");

        auto* physicsSystem = room->GetPhysicsSystem();
        auto& bodyInterface = physicsSystem->GetBodyInterface();

       

        //// 1. 지형 로드
        //const auto& terrainTiles = MapDataManager::Instance()->GetTerrainTiles();
        //for (const auto& tile : terrainTiles)
        //{
        //    if (!tile.shape) continue;
        //    JPH::BodyCreationSettings bodySettings(tile.shape, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
        //        JPH::EMotionType::Static, Layers::NON_MOVING);

        //    JPH::Body* terrainBody = bodyInterface.CreateBody(bodySettings);
        //    _stageBodyIDs.push_back(terrainBody->GetID());
        //    bodyInterface.AddBody(terrainBody->GetID(), JPH::EActivation::DontActivate);
        //}

        // 2. 정적 메쉬 그룹 로드
        auto meshGroup = MapDataManager::Instance()->GetStaticMeshGroup("BossStage");
        for (const auto& tile : meshGroup)
        {
            if (!tile->shape) continue;

            JPH::BodyCreationSettings settings(
                tile->shape,
                tile->position,
                tile->rotation,
                JPH::EMotionType::Static,
                Layers::NON_MOVING
            );

            JPH::BodyID id = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
            if (!id.IsInvalid()) {
                _stageBodyIDs.push_back(id);
            }
        }
        MYLOG("[BossStage] Physics objects loaded. Count: " << _stageBodyIDs.size());
    }

    void BossStage::on_enter(Room* room)
    {
        MYLOG("[BossStage] Spawning Elevator and Boss...");

        // 엘리베이터 생성 (16.12, -11.85, 0) -> (16.12, -1.85, 0) 10m 상승
        room->spawn_elevator(
            common::Vec3(16.12f, -11.85f, 0.0f),
            common::Vec3(16.12f, -1.85f, 0.0f),
            2.0f, 3.0f, "BossElevator"
        );

        // 보스 테이너 배치
        common::Vec3 center = get_spawn_pos();
        room->spawn_npc(GAME::NPCType::Tainer, { 0,1,0 }, "Tainer the Gatekeeper");

        room->StartGame();
    }

    void BossStage::update(Room* room, float dt)
    {
    }

    void BossStage::on_exit(Room* room)
    {
        MYLOG("[BossStage] Exiting Stage. Cleaning up...");
        auto& bodyInterface = room->GetPhysicsSystem()->GetBodyInterface();
        if (!_stageBodyIDs.empty()) {
            bodyInterface.RemoveBodies(_stageBodyIDs.data(), static_cast<int>(_stageBodyIDs.size()));
            bodyInterface.DestroyBodies(_stageBodyIDs.data(), static_cast<int>(_stageBodyIDs.size()));
            _stageBodyIDs.clear();
        }
        room->ClearAllNPCs();
    }

    const common::Vec3 BossStage::get_spawn_pos() const
    {
        return { 16.12f, -9.85f, 0.0f }; // 보스 방 스폰 위치
    }
}
