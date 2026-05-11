#pragma once
#include "Stage.h"

namespace PIP::SERVER
{
    class MainStage : public Stage
    {
    public:
        MainStage() = default;
        virtual ~MainStage() override = default;

        // [핵심 1] 물리 지형 및 충돌체 설정
        virtual void on_initialize(Room* room) override;

        // [핵심 2] 플레이어 로딩 완료 후 NPC 및 보스 배치
        virtual void on_enter(Room* room) override;

        // [핵심 3] 스테이지 특화 업데이트 (10초 후 씬 전환용 타이머 추가)
        virtual void update(Room* room, float dt) override;

        // [핵심 4] 스테이지를 떠날 때 정리
        virtual void on_exit(Room* room) override;

        virtual std::string get_stage_name() const override { return "MainStage"; }

        const common::Vec3 get_spawn_pos() const override;
    private:
        // 이 스테이지에서 생성한 물리 바디 ID 보관 (정리용)
        std::vector<JPH::BodyID> _stageBodyIDs;

        // [추가] 보스 씬 전환용 타이머
        float _bossSceneTimer = 0.0f;
        bool _isTransitioning = false;
    };
}
