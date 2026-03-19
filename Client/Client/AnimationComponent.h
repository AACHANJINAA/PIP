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

	// 애니메이션 리소스 등록 (예: "Walk", walkMesh, "run_anim")
	void add_animation(const std::string& want_name, const std::shared_ptr<Mesh>& mesh, const std::string& actualAnimName = "");

	// 애니메이션 재생 (이름이 같으면 무시, 다르면 교체)
	void play(const std::string& name, bool isLoop = true, float speed = 1.0f);

	// 현재 재생 중인 애니메이션 별칭 반환
	const std::string& get_current_name() const { return _currentName; }


	float get_anim_time() const { return _nowAnimationTime; }
	float get_anim_duration() const;
	bool is_anim_finished() const { return _isFinished; }
	void set_anim_speed(float wantSpeed) { _animationSpeed = std::max(wantSpeed, 0.0f); }


	// DW설명 : 뼈대 변환 행렬 버퍼 얻기
	const ComPtr<ID3D12Resource>& get_bone_palette_buffer() const { return _bone_palette_buffer; }
private:
	void change_mesh(const std::shared_ptr<Mesh>& want_mesh);
	void create_bone_palette_buffer(const std::shared_ptr<Mesh>& want_mesh);

private:
	struct AnimResource {
		std::shared_ptr<Mesh> mesh;
		std::string actualName; // GLTF 내부의 실제 애니메이션 이름
	};
	std::unordered_map<std::string, AnimResource> _animResources;

	std::string _currentName;		 // 현재 재생 중인 별칭 ("Walk", "Attack" 등)
	std::string _nowAnimationName{}; // 현재 재생 중인 실제 GLTF 애니메이션 이름

	bool _isLoop = true; // 애니메이션 루프 설정 -> 기본적으로 루프하도록 설정
	bool _isFinished = false; // 애니메이션이 끝났는지 설정!
	float _nowAnimationTime{ 0.f };
	float _animationSpeed{ 1.f }; // 애니메이션 속도 추가 1.0이 기본임

	std::shared_ptr<Mesh> _currentMesh{};
	std::shared_ptr<Mesh> _bufferedMesh = nullptr; // 현재 버퍼가 어떤 메쉬를 기준으로 생성된 뼈 팔레트 행렬 상수 버퍼인지 확인용
	// DW설명
	// 최종 뼈대 변환 행렬을 담을 GPU 상수 버퍼 -> 그냥 이걸 넘긴다
	// 뼈 행렬까지 각자 가지고 있을 필요는 없다 -> 상태 비의존적으로 제작하였기 때문
	ComPtr<ID3D12Resource> _bone_palette_buffer;
};

