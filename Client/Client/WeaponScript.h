#pragma once
#include "ScriptComponent.h"

class WeaponScript : public ScriptComponent
{
public:
	void on_collision_enter(std::shared_ptr<GameObject> other) override;
	
};
