#include "stdafx.h"
#include "ScriptComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"

ScriptComponent::ScriptComponent()
{
	set_name("ScriptComponent");
}

TransformComponent* ScriptComponent::transform() const
{
    if (game_object())
    {
        return game_object()->get_component<TransformComponent>().get();
    }
    return nullptr;
}
