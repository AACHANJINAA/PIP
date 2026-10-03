#pragma once
#include "stdafx.h"
#include "Behavior.h"
#include "RenderComponent.h"
#include "ReadGLTFMesh.h"

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

	// 특정 진행도(0.0 ~ 1.0)까지만 재생하고 해당 프레임에서 멈춤(속도 0)
	void play_until_progress(const std::string& name, float targetProgress, float speed = 1.0f);

	// 멈춘 시점부터 다시 재생 (속도를 복구)
	void resume(float speed = 1.0f) { _animationSpeed = speed; }

	// 현재 재생 중인 애니메이션이 특정 진행도에 도달하면 멈추도록 설정
	void set_pause_at_progress(float targetProgress);

	// 해당 별칭의 애니메이션이 등록되어 있는지 여부
	bool has_animation(const std::string& name) const { return _animResources.contains(name); }

	// 현재 재생 중인 애니메이션 별칭 반환
	const std::string& get_current_name() const { return _currentName; }


	float get_anim_time() const { return _nowAnimationTime; }
	float get_anim_duration() const;
	void set_progress(float progress) { _nowAnimationTime = get_anim_duration() * std::clamp(progress, 0.0f, 1.0f); }
	bool is_anim_finished() const { return _isFinished; }
	void set_anim_speed(float wantSpeed) { _animationSpeed = std::max(wantSpeed, 0.0f); }

	// --- 캐릭터별 뼈 자세 조회 ---
	// late_update에서 계산된 이 캐릭터의 뼈 행렬 (공용 메쉬의 _nodes와 달리 다른 캐릭터가 덮어쓰지 않음)
	// late_update 이후부터 다음 프레임 late_update 전까지 유효. 업데이트 단계에서 읽으면 직전 프레임 자세
	bool has_pose() const { return !_jointModelMatrices.empty(); }
	// 모델 공간 뼈 행렬. 자세가 없거나 뼈가 없으면 false
	bool try_get_bone_model_matrix(const std::string& bone_name, XMFLOAT4X4& out) const;
	// 모델 공간 뼈 행렬 × 오브젝트 월드 행렬
	bool try_get_bone_world_matrix(const std::string& bone_name, XMFLOAT4X4& out) const;


	//// DW설명 : 뼈대 변환 행렬 버퍼 얻기 -> 작동 안함
	//const ComPtr<ID3D12Resource>& get_bone_palette_buffer() const { return _bone_palette_buffer; }

	// DW설명 : 뼈대 변환 행렬 버퍼의 GPU 가상 주소 얻기 (렌더링 시 셰이더에 전달하기 위해)
	D3D12_GPU_VIRTUAL_ADDRESS get_bone_gpu_virtual_address() const { return _currentBoneGPUAddr; }

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

	bool _isPauseTargetSet = false; // 특정 진행도에서 멈출지 여부
	float _pauseTargetProgress = 0.0f; // 멈출 목표 진행도 (0.0 ~ 1.0)

	std::shared_ptr<Mesh> _currentMesh{};
	std::shared_ptr<Mesh> _bufferedMesh = nullptr; // 현재 버퍼가 어떤 메쉬를 기준으로 생성된 뼈 팔레트 행렬 상수 버퍼인지 확인용
	
	
	//// DW설명 -> 이것 작동 안함
	//// 최종 뼈대 변환 행렬을 담을 GPU 상수 버퍼 -> 그냥 이걸 넘긴다
	//// 뼈 행렬까지 각자 가지고 있을 필요는 없다 -> 상태 비의존적으로 제작하였기 때문
	//ComPtr<ID3D12Resource> _bone_palette_buffer;
	//UINT8* _mapped_bone_data = nullptr; // 매핑된 GPU 버퍼에 직접 접근하기 위한 포인터 (CPU 메모리)


	// DW설명 : GPU 가상 주소는 버퍼가 GPU에 업로드된 후에 얻을 수 있음 -> 버퍼가 생성되고 데이터가 업로드된 후에 이 주소를 얻어서 저장해둬야 함
	// 즉 매 프레임 할당기에서 빌려올 주소와 크기를 기억할 변수들임
	D3D12_GPU_VIRTUAL_ADDRESS _currentBoneGPUAddr = 0;
	size_t _bonePaletteSize = 0;
	


	// 인스턴싱을 위한 노드 정보와, 뼈대 행렬 들고있기
	std::vector<NodeInfo> _nodes; // 노드 정보 리스트 (glTF node index와 1:1 매칭)
	std::vector<DirectX::XMFLOAT4X4> _boneTransforms; // 뼈대 행렬 팔레트 (CPU 메모리) -> 뼈대 행렬들을 애니메이션 컴포넌트가 관리하도록 변경

	// 캐릭터별 뼈 자세 (조인트 순서, 모델 공간, 전치하지 않음)
	std::vector<DirectX::XMFLOAT4X4> _jointModelMatrices;
	const ReadGLTFMesh* _poseMesh = nullptr; // _jointModelMatrices를 계산한 메쉬
	mutable std::unordered_map<std::string, int> _jointIndexCache; // 뼈 이름 -> 조인트 인덱스
	mutable const ReadGLTFMesh* _jointIndexCacheMesh = nullptr;      // 캐시를 만든 메쉬
};

