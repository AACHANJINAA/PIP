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

	// 디버깅용 뼈대 이름
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

//====================================================================================================


class ReadGLTFMesh : public Mesh
{
public:
	ReadGLTFMesh(const std::string& filePath, bool ishave_animate = false);
	~ReadGLTFMesh() override;

	void upload_to_gpu_internal(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;

	//virtual void render(ID3D12GraphicsCommandList* commandList) override;
	virtual void render(ID3D12GraphicsCommandList* commandList) override;
	void release_upload_buffers() override;


	// DW설명 : 애니메이션 관련 함수들
	void update_animation(float& delta_time, int clip_index);
	void render_skinned(ID3D12GraphicsCommandList* commandList);

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
	void process_skinned_mesh(const json& gltf_json, const std::vector<char>& binary_buffer, const json& mesh);

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
		size_t byteOffset = bufferView.value("byteOffset", 0) + accessor.value("byteOffset", 0);
		size_t elementSize = sizeof(T);
		size_t byteStride = bufferView.value("byteStride", elementSize);

		std::vector<T> data(count);
		const char* bufferStart = binaryBuffer.data() + byteOffset;

		for (size_t i = 0; i < count; ++i) {
			memcpy(&data[i], bufferStart + i * byteStride, elementSize);
		}

		return data;
	}

private:
	std::vector<std::unique_ptr<GltfPrimitive>> _primitives;
	std::vector<std::string> _material_names;

private: // DW설명 : 애니메이션 관련 멤버 변수들
	bool _is_animated; // 애니메이션 하는 건지?
	std::vector<BoneInfo> _skeleton;

	// (GPU 행렬 팔레트 순서와 일치) 뼈대 인덱스 목록
	std::vector<int> _joints;

	std::vector<AnimationClip> _animations;

	// 스킨이 이미 로드되었는지 확인하는 플래그
	bool _is_skin_loaded;

	// GPU로 업로드 될 최종 뼈대 변환 행렬 팔레트
	// (매 프레임 UpdateAnimation()에서 계산됨)
	std::vector<DirectX::XMFLOAT4X4> _final_bone_transforms;

	// 최종 뼈대 변환 행렬을 담을 GPU 상수 버퍼
	ComPtr<ID3D12Resource> _bone_palette_buffer;


private: // 애니메이션을 위해 필요한 멤버들
	
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

	// 모든 노드의 리스트 (glTF node index와 1:1 매칭)
	std::vector<NodeInfo> _nodes;

	// 현재 재생 중인 애니메이션 시간
	// DW설명 : GltfAnimationTestScript 에서 관리하도록 변경함
	//			그래야 각 오브젝트별로 다른 애니메이션 타임을 가질 수 있음
	float _current_animation_time = 0.0f;

	// 헬퍼 함수: 초기 노드 계층 구조 및 TRS 값 설정
	void load_nodes(const json& gltf_json);

	// 헬퍼 함수: 노드 계층 구조를 순회하며 전역 행렬 갱신
	void update_node_hierarchy(int node_index, const DirectX::XMMATRIX& parent_transform);

};