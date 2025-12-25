#pragma once
#include "stdafx.h"
#include "Behaviour.h"

class Mesh;
class AnimationComponent : public Behaviour
{
public:
	AnimationComponent();
	~AnimationComponent() override = default;

public:
	void late_update(float deltaTime) override;

public:
	/*void set_animation(std::string name);
	void set_animation_time(float time);
	void set_mesh(const std::shared_ptr<Mesh>& want_mesh);
	*/

	void set_state(common::packet::OBJECT_STATE state);
	common::packet::OBJECT_STATE get_state() const { return _currentState; }
	// 상태와 애니메이션 이름 매핑 (예: IDLE -> "Armature|Idle")
	void add_state_mapping(common::packet::OBJECT_STATE state, const std::string& animName, 
		std::shared_ptr<Mesh> mesh =nullptr);
private:
	void change_animation(std::string name);
	void change_mesh(const std::shared_ptr<Mesh>& want_mesh);
private:
	float _nowAnimationTime{ 0.f };
	std::string _nowAnimationName{};
	//std::shared_ptr<Mesh> _nowAnimationMash{}; //KJ가 DW -> Mesh 아님? ㅋㅋㅋㅋㅋㅋㅋㅋㅋㅋㅋ

	std::shared_ptr<Mesh> _currentMesh{};
	common::packet::OBJECT_STATE _currentState{ common::packet::OBJECT_STATE::IDLE };

	std::unordered_map<common::packet::OBJECT_STATE, std::string> _stateAnimMap;
	std::unordered_map<common::packet::OBJECT_STATE, std::shared_ptr<Mesh>> _stateMeshMap;
};

