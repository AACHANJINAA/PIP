#pragma once
#include "ScriptComponent.h"
#include "RenderComponent.h"
#include "AnimationComponent.h"

class QuestNPCScript : public ScriptComponent
{
public:
    using required_components = std::tuple<RenderComponent, AnimationComponent>;

    QuestNPCScript() = default;
    virtual ~QuestNPCScript() = default;

    virtual void awake() override;
    virtual void update(float deltaTime) override;
};
