#pragma once
#include "ScriptComponent.h"

class GltfAnimationScript : public ScriptComponent
{
public:
    GltfAnimationScript() = default;
    virtual ~GltfAnimationScript() = default;

    virtual void update(float delta_time) override;

private:
	float _animationTime = 0.0f;
};