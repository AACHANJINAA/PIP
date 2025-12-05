#include "stdafx.h"
#include "MainPlayerAnimationScript.h"
#include "ReadGLTFMesh.h"
#include "ResourceManager.h"

void MainPlayerAnimationScriptScript::awake()
{
	// 메시들 불러오기
	_animations.insert({ "Walk", ResourceManager::instance()->load_mesh("Resource/Character/Gramma_Walk/Gramma_Walk.gltf", true) });
}

void MainPlayerAnimationScriptScript::update(float deltaTime)
{
	// 플레이어 조작에 따른 애니메이션 변경 처리



	// 꼭 불러주어야 함!
	GltfAnimationScript::update(deltaTime);
}

void MainPlayerAnimationScriptScript::late_update(float deltaTime)
{
}
