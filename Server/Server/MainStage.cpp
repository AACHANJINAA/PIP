#include "pch.h"
#include "MainStage.h"
#include "Room.h"
#include "JoltSetup.h"
#include "MapDataManager.h"

namespace PIP::SERVER
{
    void MainStage::on_initialize(Room* room)
    {
        MYLOG("[MainStage] Initializing Physics Terrain...");

        auto* physicsSystem = room->GetPhysicsSystem();
        auto& bodyInterface = physicsSystem->GetBodyInterface();

        
        //TODO: 우리가 정의한 "MainStage" 그룹에 속한 타일 포인터들만 가져옴
        //auto myTiles = MapDataManager::Instance()->GetTerrainGroup("MainStage");

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
        MYLOG("[MainStage] Physics terrain loaded. Count: " << terrain_num);


        // 2. 고정 메쉬(Static Mesh Collisions) 로드
        const auto& shared_shapes = MapDataManager::Instance()->GetStaticMeshTiles();
        for (const auto& tile : shared_shapes)
        {
            JPH::BodyCreationSettings settings(tile.shape, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                JPH::EMotionType::Static, Layers::NON_MOVING);
            JPH::BodyID id = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
            _stageBodyIDs.push_back(id);
        }

        MYLOG("[MainStage] Physics objects loaded. Count: " << _stageBodyIDs.size() - terrain_num);
    }

    void MainStage::on_enter(Room* room)
    {
        MYLOG("[MainStage] Spawning NPCs and Boss...");

        // 1. 기존 Room에 있던 SpawnInitialNPCs 로직 수행 (500마리 등)
        room->SpawnInitialNPCs();

        // 2. 보스 스폰 (Tainer)
        room->SpawnBoss(); // 기존 함수를 유지하거나 이리로 옮김
        room->StartGame(); // 게임 상태 PLAYING으로 전환
    }

    void MainStage::update(Room* room, float dt)
    {
        // 보스 페이즈 체크나 스테이지 전용 기믹 업데이트
    }

    void MainStage::on_exit(Room* room)
    {
        MYLOG("[MainStage] Exiting Stage. Cleaning up...");

        // 1. 이 스테이지에서 만든 물리 바디 모두 제거
        auto& bodyInterface = room->GetPhysicsSystem()->GetBodyInterface();
        if (!_stageBodyIDs.empty()) {
            bodyInterface.RemoveBodies(_stageBodyIDs.data(), (int)_stageBodyIDs.size());
            bodyInterface.DestroyBodies(_stageBodyIDs.data(), (int)_stageBodyIDs.size());
            _stageBodyIDs.clear();
        }

        // 2. 스테이지 NPC들 제거
        room->ClearAllNPCs();
    }

    const common::Vec3 MainStage::get_spawn_pos() const
    {
		return { 0.0f, 0.0f, 0.0f }; // 월드 중앙 등 원하는 위치로 반환
    }
}
