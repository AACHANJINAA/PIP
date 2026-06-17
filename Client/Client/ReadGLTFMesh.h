#pragma once
#include "stdafx.h"
#include "Mesh.h"

enum class AnimationInterpolation {
	Linear,
	Step,
	CubicSpline
};

struct GltfVertex : public Vertex
{
public:
	XMFLOAT3 _normal;      // 법선 벡터
	XMFLOAT2 _texCoord;    // 텍스처 좌표 (UV)
	XMFLOAT4 _tangent;     // 탄젠트 벡터 (w 요소는 handedness를 나타냄)

	// 향후 애니메이션 확장 영역
	// 주석 처리된 이 부분에 스키닝 데이터를 추가할 수 있습니다.
	// XMFLOAT4 _boneIndices; // 영향을 주는 뼈(Bone)의 인덱스 (최대 4개)
	// XMFLOAT4 _boneWeights; // 각 뼈로부터 받는 가중치

public:
	GltfVertex() {
		_position = XMFLOAT3(0.0f, 0.0f, 0.0f);
		_normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		_texCoord = XMFLOAT2(0.0f, 0.0f);
		_tangent = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	GltfVertex(XMFLOAT3 p, XMFLOAT3 n, XMFLOAT2 t, XMFLOAT4 tan) {
		_position = p;
		_normal = n;
		_texCoord = t;
		_tangent = tan;
	}
};

struct GltfSkinnedVertex : public GltfVertex
{
	UINT   _boneIndices[4]; // BONEINDICES (JOINTS_0)
	float  _boneWeights[4]; // BONEWEIGHTS (WEIGHTS_0)
};

struct GltfPrimitive
{
	// CPU
	std::vector<GltfVertex> _vertices; // 기존 애니메이션 없는 메쉬용
	std::vector<GltfSkinnedVertex> _skinned_vertices; // 애니메이션 메쉬용

	std::vector<UINT> _indices;

	// GPU
	ComPtr<ID3D12Resource> _vertexBuffer;
	ComPtr<ID3D12Resource> _indexBuffer;

	D3D12_VERTEX_BUFFER_VIEW _vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW _indexBufferView;

	UINT _vertexCount = 0;
	UINT _indexCount = 0;
	int _materialIndex = -1;

	BoundingOrientedBox _orientedBoundingBox;

	// 임시 업로드 버퍼 로딩이 끝나면 해제
	ComPtr<ID3D12Resource> _vertexUploadBuffer;
	ComPtr<ID3D12Resource> _indexUploadBuffer;
};



// ===================================DW설명 : gltf 애니메이션을 위한 구조체들===================================

// glTF의 'skins' 배열에서 파싱한 개별 뼈대(Bone) 정보를 저장
// glTF의 'nodes' 계층 구조와 'inverseBindMatrices'를 결합
 
struct BoneInfo
{
	DirectX::XMFLOAT4X4 _inverse_bind_matrix;

	int _parent_index;

	int _node_index;

	// 뼈대 이름 (애니메이션 여러가지 할 때 쓰임)
	std::string _name;
};


// glTF의 'sampler'에서 읽어온 단일 키프레임 (시간-값 쌍)
struct Keyframe {
	float _time;
	DirectX::XMFLOAT4 _value;      // 기본 값 (Linear/Step/Spline Vertex)에서 모두 사용
	DirectX::XMFLOAT4 _in_tangent;
	DirectX::XMFLOAT4 _out_tangent;
};


// glTF animation.channel과 animation.sampler를 결합한
// 단일 애니메이션 트랙 (예: 61번 뼈대의 회전)
struct AnimationChannel
{
	// 이 채널이 제어할 glTF 'nodes' 배열의 인덱스
	int _node_index;

	// 변경할 속성 "translation", "rotation", "scale"
	std::string _path;

	// 보간 방식 "LINEAR", "STEP"
	AnimationInterpolation _interpolation; // 매번 문자열 비교하는 것을 피하기 위해 enum으로 수정

	// 이 채널(트랙)의 모든 키프레임 목록
	std::vector<Keyframe> _keyframes;
};

// glTF 'animations' 배열의 한 항목에 해당하는 애니메이션 클립
// 예 : Walk, Run 등등
struct AnimationClip
{
	// 애니메이션 클립 이름 (디버깅용)
	std::string _name;

	// 이 클립의 전체 재생 시간 (초)
	float _duration;

	// 이 클립을 구성하는 모든 애니메이션 트랙(채널) 목록입니다.
	std::vector<AnimationChannel> _channels;
};

// glTF 'nodes' 배열의 상태를 관리하기 위한 구조체
struct NodeInfo
{
	int _parent_index = -1;
	std::vector<int> _children;

	// 현재 애니메이션에 의해 변경되는 로컬 변환 값 (T, R, S)
	DirectX::XMFLOAT3 _translation = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 _rotation = { 0.0f, 0.0f, 0.0f, 1.0f }; // Quaternion
	DirectX::XMFLOAT3 _scale = { 1.0f, 1.0f, 1.0f };

	// 계층 구조가 반영된 최종 전역 행렬 (World Transform)
	DirectX::XMFLOAT4X4 _global_transform;
};

//====================================================================================================


class ReadGLTFMesh : public Mesh
{
public:
	ReadGLTFMesh(const std::string& filePath, bool is_animated = false, std::string animation_name = "null_name");
	~ReadGLTFMesh() override;

	void upload_to_gpu_internal(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, UINT64 targetFenceValue) override;

	//virtual void render(ID3D12GraphicsCommandList* commandList) override;
	void render(ID3D12GraphicsCommandList* commandList) override;
	void render_instance(ID3D12GraphicsCommandList* commandList, size_t want_instance_count = 1) override; // 인스턴싱으로 렌더링
	void render_instance_CascadeShadowMap(ID3D12GraphicsCommandList* commandList, size_t want_instance_count = 1) override;
	void release_upload_buffers() override;

	virtual void render_CascadeShadowMap(ID3D12GraphicsCommandList* commandList) override;

	
	// DW설명 : 애니메이션 관련 함수들
	// void set_bone_palette_buffer_from_animation_component(ComPtr<ID3D12Resource> bone_palette_buffer) { _bone_palette_buffer_from_animation_component = bone_palette_buffer; }

	// 추후 인스턴싱을 위해서 확장하고 있는 함수
	void update_animation(float& delta_time, std::string animation_name, std::vector<DirectX::XMFLOAT4X4>& bone_transforms, bool _isLoop = true);

	// 현재 사용중인 것
	void update_animation(float& delta_time, const std::string& animation_name, UINT8* mapped_buffer, bool _isLoop = true);
	
	void render_skinned(ID3D12GraphicsCommandList* commandList);
	void render_instance_skinned(ID3D12GraphicsCommandList* commandList); // 스키닝 인스턴싱으로 렌더링
	size_t get_joint_count() const { return _joints.size(); }

	// 애니메이션만 있는 glTF 파일 로더 추가
	void load_animation_only(const std::string& file_path, const std::string& want_name = "null_name");

	float get_animation_duration(const std::string& name) const;

	bool has_animation(const std::string& name) const;

	std::vector<std::string> get_animation_names() const;

	std::string get_parent_bone_name(const std::string& child_name) const;

public: // DW설명 : 소켓기능 관련 함수들
	int get_bone_index_by_name(const std::string& name) const; // 뼈대 이름으로 인덱스 찾기
	// DW주의 : get_socket_transform 이 함수의 순서는 update_animation 함수 호출 직후에 호출되어야 한다 그래야 방금 갱신된 위치를 가져올 수 있음 // TickGroup
	XMFLOAT4X4 get_socket_transform(std::string& bone_name) const; // 뼈대 이름으로 소켓 변환 행렬 얻기
	std::vector<std::string> get_bone_names() const;

	void set_shader_for_all_materials(const std::string& shader_name);

public: // DW설명 : 인스턴싱 관련 함수들
	void nodes_inout_set(_Inout_ std::vector<NodeInfo>& nodes); // 모든 gltf 노드의 정보를 설정해주는 함수

	// DW설명 : 레이와 메쉬의 교차 여부를 계산하는 함수 rayStart는 레이의 시작점, rayDir는 레이의 방향 벡터 worldMatrix는 메쉬의 월드 변환 행렬 outHitDist는 레이와 메쉬가 교차할 때의 거리를 반환하는 출력 매개변수
	bool intersects_ray(const XMVECTOR& rayStart, const XMVECTOR& rayDir, const XMMATRIX& worldMatrix, float& outHitDist) const;


public: // DW설명 : 파티클 기반 스킬 구현을 위한 내용들
	// 지정된 개수(particleCount)만큼 메쉬 표면에서 균일하게 랜덤한 점들을 추출
	std::vector<DirectX::XMFLOAT3> extract_particle_targets(UINT particleCount) const;

private:

	void read_static_mesh(const std::string& filePath);
	
	bool load_gltf_file(const std::string& filename, json& outJson, std::vector<char>& outBinBuffer);
	void process_node(const json& gltfJson, const std::vector<char>& binaryBuffer, int nodeIndex, const DirectX::XMFLOAT4X4& parentTransform);
	void process_mesh(const json& gltfJson, const std::vector<char>& binaryBuffer, const json& mesh, const DirectX::XMFLOAT4X4& transform);

	// 전체 모델의 바운딩 박스를 모든 프리미티브의 바운딩 박스를 병합하여 계산
	// DW설명 : 모든 프리미티브의 OBB를 병합하여 메쉬 전체의 OBB를 계산함
	void bounding_box_merge();

	// 스키닝 로더 함수
	void read_skinned_animation_mesh(const std::string& filePath);
	void load_skins(const json& gltf_json, const std::vector<char>& binary_buffer);
	void load_animations(const json& gltf_json, const std::vector<char>& binary_buffer);

	// 스키닝 메쉬 처리 함수
	void process_skinned_mesh(const json& gltf_json, const std::vector<char>& binary_buffer, const json& mesh, int skin_index);
	
	// DW설명 : 이름을 통해 행렬 팔레트에서 해당 뼈대의 인덱스를 반환하는 함수
	int get_palette_index_by_name(const std::string& name) const;

	// enum 으로 바꾸는 헬퍼 함수
	AnimationInterpolation string_to_interpolation(const std::string& str);

	// glTF accessor에서 속성 데이터를 추출하는 템플릿 함수
	//  T 타입의 데이터를 벡터로 반환
	template<typename T>
	std::vector<T> get_attribute_data(const json& gltfJson, const std::vector<char>& binaryBuffer, int accessorIndex)
	{
		if (accessorIndex < 0) return {};

		const json& accessor = gltfJson["accessors"][accessorIndex];
		const json& bufferView = gltfJson["bufferViews"][accessor["bufferView"].get<size_t>()];

		size_t count = accessor["count"];
		
		std::string type = accessor.value("type", "SCALAR");
		size_t type_count = 1;
		if (type == "VEC2") type_count = 2;
		else if (type == "VEC3") type_count = 3;
		else if (type == "VEC4") type_count = 4;
		else if (type == "MAT4") type_count = 16;

		size_t component_size = 4; // float or uint32
		if (accessor.contains("componentType")) {
			int ctype = accessor["componentType"];
			if (ctype == 5120 || ctype == 5121) component_size = 1;
			else if (ctype == 5122 || ctype == 5123) component_size = 2;
		}

		size_t elementSize = type_count * component_size;
		size_t byteOffset = bufferView.value("byteOffset", 0) + accessor.value("byteOffset", 0);
		size_t byteStride = bufferView.value("byteStride", elementSize);

		size_t items_per_element = elementSize / sizeof(T);
		if (items_per_element == 0) items_per_element = 1;

		std::vector<T> data(count * items_per_element);
		const char* bufferStart = binaryBuffer.data() + byteOffset;

		for (size_t i = 0; i < count; ++i) {
			memcpy(&data[i * items_per_element], bufferStart + i * byteStride, elementSize);
		}

		return data;
	}

private:
	std::vector<std::unique_ptr<GltfPrimitive>> _primitives;
	std::vector<std::string> _material_names;

private: // DW설명 : 애니메이션 관련 멤버 변수들
	bool _is_animated; // 애니메이션 하는 건지?
	std::string _include_animation_name; // with_skin으로 뽑은 애니메이션의 경우 지정해준 애니메이션 이름
	std::vector<BoneInfo> _skeleton;

	// (GPU 행렬 팔레트 순서와 일치) 뼈대 인덱스 목록
	std::vector<int> _joints;

	std::map<std::string, AnimationClip> _animations;

	// 스킨이 이미 로드되었는지 확인하는 플래그
	bool _is_skin_loaded;

	// GPU로 업로드 될 최종 뼈대 변환 행렬 팔레트
	// (매 프레임 UpdateAnimation()에서 계산됨)
	std::vector<DirectX::XMFLOAT4X4> _final_bone_transforms;

	// 최종 뼈대 변환 행렬을 담을 GPU 상수 버퍼
	// ComPtr<ID3D12Resource> _bone_palette_buffer;

	// 애니메이션 컴포넌트로부터 받는 뼈대 행렬 버퍼
	// ComPtr<ID3D12Resource> _bone_palette_buffer_from_animation_component = nullptr;

private: // 애니메이션을 위해 필요한 멤버들
	
	// 모든 노드의 리스트 (glTF node index와 1:1 매칭)
	std::vector<NodeInfo> _nodes;

	// 현재 재생 중인 애니메이션 시간
	// DW설명 : GltfAnimationScript 에서 관리하도록 변경함
	//			그래야 각 오브젝트별로 다른 애니메이션 타임을 가질 수 있음
	float _current_animation_time = 0.0f;

	// 헬퍼 함수: 초기 노드 계층 구조 및 TRS 값 설정
	void load_nodes(const json& gltf_json);

	// 헬퍼 함수: 노드 계층 구조를 순회하며 전역 행렬 갱신
	void update_node_hierarchy(int node_index, const DirectX::XMMATRIX& parent_transform);

};