#pragma once
#include "ScriptComponent.h"
#include "GltfAnimationScript.h"


class Mesh;
class MainPlayerAnimationScriptScript : public GltfAnimationScript
{
public:
	MainPlayerAnimationScriptScript() = default;
	virtual ~MainPlayerAnimationScriptScript() = default;


public:
	virtual void awake() override;
	virtual void update(float deltaTime) override;
	virtual void late_update(float deltaTime) override;


private:
	std::map<std::string, std::shared_ptr<Mesh>> _animations;
};

