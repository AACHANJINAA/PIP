#pragma once
#include "stdafx.h"
#include "Behavior.h"
#include "RenderComponent.h"

class Mesh;
class AnimationComponent : public Behavior
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

	// DW설명 : 뼈대 변환 행렬 버퍼 얻기
	const ComPtr<ID3D12Resource>& get_bone_palette_buffer() const { return _bone_palette_buffer; }


	float get_anim_time() const { return _nowAnimationTime; }
	float get_anim_duration() const;
	bool is_anim_finished() const;
	void set_anim_speed(float wantSpeed);
private:
	void change_animation(std::string name);
	void change_mesh(const std::shared_ptr<Mesh>& want_mesh);
	void create_bone_palette_buffer(const std::shared_ptr<Mesh>& want_mesh);
private:
	bool _isFinished = false;
	// DW설명
	// 최종 뼈대 변환 행렬을 담을 GPU 상수 버퍼 -> 그냥 이걸 넘긴다
	// 뼈 행렬까지 각자 가지고 있을 필요는 없다 -> 상태 비의존적으로 제작하였기 때문
	ComPtr<ID3D12Resource> _bone_palette_buffer;

	// 현재 버퍼가 어떤 메쉬를 기준으로 생성된 뼈 팔레트 행렬 상수 버퍼인지 확인용
	std::shared_ptr<Mesh> _bufferedMesh = nullptr;

	float _nowAnimationTime{ 0.f };
	float _animationSpeed{1.f}; // 애니메이션 속도 추가 1.0이 기본임
	std::string _nowAnimationName{};
	//std::shared_ptr<Mesh> _nowAnimationMash{}; //KJ가 DW -> Mesh 아님? ㅋㅋㅋㅋㅋㅋㅋㅋㅋㅋㅋ

	std::shared_ptr<Mesh> _currentMesh{};
	common::packet::OBJECT_STATE _currentState{ common::packet::OBJECT_STATE::IDLE };

	std::unordered_map<common::packet::OBJECT_STATE, std::string> _stateAnimMap;
	std::unordered_map<common::packet::OBJECT_STATE, std::shared_ptr<Mesh>> _stateMeshMap;
};

