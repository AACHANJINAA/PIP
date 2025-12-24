#include "stdafx.h"
#include "AnimationComponent.h"
#include "ReadGLTFMesh.h"
#include "GameObject.h"
#include "RenderComponent.h"

AnimationComponent::AnimationComponent()
{
}

AnimationComponent::~AnimationComponent()
{
}

void AnimationComponent::late_update(float deltaTime)
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
	auto renderer = game_object().get()->get_component<RenderComponent>();
	renderer->set_mesh(want_mesh);
}

void AnimationComponent::change_animation(std::string name)
{
	if(_nowAnimationName == name)
	{
		return;
	}
	_nowAnimationName = name;
	_nowAnimationTime = 0.f;
}

void AnimationComponent::change_mesh(const std::shared_ptr<Mesh>& want_mesh)
{
	if (_nowAnimationMash.get() == want_mesh.get())
	{
		return;
	}
	_nowAnimationMash = want_mesh;
	auto renderer = game_object().get()->get_component<RenderComponent>();
	renderer->set_mesh(want_mesh);
	_nowAnimationTime = 0.f;
}
