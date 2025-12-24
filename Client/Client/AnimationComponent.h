#pragma once
#include "stdafx.h"
#include "Behaviour.h"

class Mesh;
class AnimationComponent : public Behaviour
{
public:
	AnimationComponent();
	virtual ~AnimationComponent();

public:
	virtual void late_update(float deltaTime) override;

public:
	void set_animation(std::string name);
	void set_animation_time(float time);
	void set_mesh(const std::shared_ptr<Mesh>& want_mesh);

	void change_animation(std::string name);
	void change_mesh(const std::shared_ptr<Mesh>& want_mesh);

private:
	float _nowAnimationTime{ 0.f };
	std::string _nowAnimationName{};
	std::shared_ptr<Mesh> _nowAnimationMash{};
};

