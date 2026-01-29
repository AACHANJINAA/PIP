#include "stdafx.h"
#include "WeaponScript.h"
#include "GameObject.h"
void WeaponScript::on_collision_enter(std::shared_ptr<GameObject> other)
{
	CLOG("[Weapon Hit] Target: " << other->name() << std::endl);
}
