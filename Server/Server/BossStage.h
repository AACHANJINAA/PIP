#pragma once
#include "Stage.h"

namespace PIP::SERVER
{
    class BossStage : public Stage
    {
    public:
        BossStage() = default;
        virtual ~BossStage() override = default;

        virtual void on_initialize(Room* room) override;
        virtual void on_enter(Room* room) override;
        virtual void update(Room* room, float dt) override;
        virtual void on_exit(Room* room) override;

        virtual std::string get_stage_name() const override { return "BossStage"; }

        const common::Vec3 get_spawn_pos() const override;
    private:
        std::vector<JPH::BodyID> _stageBodyIDs;
    };
}
