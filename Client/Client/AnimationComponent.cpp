#include "stdafx.h"
#include "AnimationComponent.h"
#include "ReadGLTFMesh.h"

AnimationComponent::AnimationComponent()
{
}

AnimationComponent::~AnimationComponent()
{
}

void AnimationComponent::update(float deltaTime)
{
	_nowAnimationTime += deltaTime;
	if(_nowAnimationMash)
	{
		static_cast<ReadGLTFMesh*>(_nowAnimationMash.get())->update_animation(_nowAnimationTime, _nowAnimationName);
	}
}

void AnimationComponent::set_animation(std::string name)
{
	_nowAnimationName = name;
}

void AnimationComponent::set_animation_time(float time)
{
	_nowAnimationTime = time;
}

void AnimationComponent::set_mesh(const std::shared_ptr<Mesh>& want_mesh)
{
	_nowAnimationMash = want_mesh;
}
