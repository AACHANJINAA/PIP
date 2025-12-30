#include "stdafx.h"
#include "AnimationComponent.h"
#include "ReadGLTFMesh.h"
#include "GameObject.h"
#include "RenderComponent.h"

AnimationComponent::AnimationComponent()
{
}

void AnimationComponent::late_update(float deltaTime)
{
	_nowAnimationTime += deltaTime;

	// 현재 렌더링 중인 메쉬를 가져와서 애니메이션 업데이트
	auto renderComp = game_object()->get_component<RenderComponent>();
	if (!renderComp) return;

	//auto mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(renderComp->mesh());
	//if (mesh && !_nowAnimationName.empty())
	//{
	//	mesh->update_animation(_nowAnimationTime, _nowAnimationName);
	//}

	auto mesh = _stateMeshMap.find(_currentState);
	auto anim = _stateAnimMap.find(_currentState);
	std::dynamic_pointer_cast<ReadGLTFMesh>(mesh->second)->update_animation(_nowAnimationTime, anim->second);
}

void AnimationComponent::set_state(common::packet::OBJECT_STATE state)
{
	if (_currentState == state) return;
	_currentState = state;

	// 1. 메쉬 교체 (등록된 메쉬가 있을 경우만)
	auto mIt = _stateMeshMap.find(state);
	if (mIt != _stateMeshMap.end() && mIt->second) {
		change_mesh(mIt->second);
	}

	// 2. 애니메이션 교체
	auto aIt = _stateAnimMap.find(state);
	if (aIt != _stateAnimMap.end()) {
		change_animation(aIt->second);
	}
}

void AnimationComponent::add_state_mapping(common::packet::OBJECT_STATE state, const std::string& animName,
	std::shared_ptr<Mesh> mesh)
{
	_stateAnimMap[state] = animName;
	if (mesh) _stateMeshMap[state] = mesh;
}

void AnimationComponent::change_animation(std::string name)
{
	if (_nowAnimationName == name)
	{
		return;
	}
	_nowAnimationName = name;
	_nowAnimationTime = 0.f;
}

void AnimationComponent::change_mesh(const std::shared_ptr<Mesh>& want_mesh)
{
	if (_currentMesh == want_mesh) return;
	_currentMesh = want_mesh;
	if (auto renderer = game_object()->get_component<RenderComponent>()) {
		renderer->set_mesh(want_mesh);
	}
	_nowAnimationTime = 0.f;
}

/*void AnimationComponent::set_animation(std::string name)
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
}*/
