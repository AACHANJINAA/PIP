#include "stdafx.h"
#include "GltfAnimationScript.h"
#include "GameObject.h"
#include "RenderComponent.h"
#include "ReadGLTFMesh.h"

void GltfAnimationScript::update(float delta_time)
{
    auto renderComp = game_object()->get_component<RenderComponent>();
    if (!renderComp) return;

    // 메쉬를 ReadGLTFMesh로 캐스팅
    auto mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(renderComp->mesh());

    if (mesh)
    {
        // 0번 애니메이션 클립 재생
		_animationTime += delta_time;
        mesh->update_animation(_animationTime, "Armature|mixamo.com|Layer0");
        mesh->update_animation(_animationTime, "Armature|mixamo.com|Layer0");
    }
}