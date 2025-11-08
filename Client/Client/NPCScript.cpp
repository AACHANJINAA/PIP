#include "stdafx.h"
#include "NPCScript.h"

#include "GameObject.h"

void NPCScript::set_position(const f3& position)
{
	game_object().get()->get_component<TransformComponent>()->set_local_position(position);
}

const f3& NPCScript::position() const
{
	return game_object().get()->get_component<TransformComponent>()->local_position();
}

void NPCScript::awake()
{
	transform()->set_local_rotation(-90.f, 0.f, 0.f);
	transform()->set_local_scale({ 200.0f, 200.0f, 200.0f });
}

void NPCScript::update(float deltaTime)
{
	ScriptComponent::update(deltaTime);
}
