#pragma once
#include "ScriptComponent.h"

class GltfAnimationTestScript : public ScriptComponent
{
public:
    GltfAnimationTestScript() = default;
    virtual ~GltfAnimationTestScript() = default;

    virtual void update(float delta_time) override;
};