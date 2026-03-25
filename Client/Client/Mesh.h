#pragma once
#include "stdafx.h"
#include "Object.h"

// nlohmann/json 헤더 // json 파싱 위해 추가
using json = nlohmann::json;

//정점을 표현하기 위한 클래스를 선언한다.


// Node 구조체 정의 DW : GLTF/GLB 파일에서 노드 정보를 표현하기 위해 추가
//struct Node
//{
//	std::string name;
//	int parentIndex = -1; // 부모가 없으면 -1
//	std::vector<int> childrenIndices;
//
//	// 변환 정보
//	XMFLOAT3 translation = { 0.0f, 0.0f, 0.0f };
//	XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f }; // 쿼터니언
//	XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
//
//	int meshIndex = -1;
//	int skinIndex = -1;
//};

//// 스키닝 정보를 포함하는 새로운 정점 구조체 -> Vertex 대신에 사용할 것임
//struct SkinnedVertex
//{
//	XMFLOAT3 m_xmf3Position;
//	XMFLOAT3 m_xmf3Normal;
//	XMFLOAT2 m_xmf2TexCoord;
//
//	// 추가된 스키닝 데이터
//	XMFLOAT4 m_xmf4BoneIndices; // 영향을 주는 뼈의 인덱스 (최대 4개)
//	XMFLOAT4 m_xmf4BoneWeights; // 각 뼈로부터 받는 영향(가중치)
//};

struct MeshPrimitive
{
	ID3D12Resource* _d3dVertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _d3dVertexBufferView{};

	ID3D12Resource* _d3dIndexBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW _d3dIndexBufferView{};

	UINT _indices = 0;

	// 나중에 재질(Material) 인덱스도 여기에 저장할 수 있습니다.
	int _materialIndex = -1;

	ID3D12Resource* _texture = nullptr; // 텍스처 리소스를 저장할 포인터
	D3D12_GPU_DESCRIPTOR_HANDLE _gpuSrvHandle{}; // SRV 핸들을 저장할 변수

	// 소멸자에서 리소스 해제
	~MeshPrimitive() {
		if (_d3dVertexBuffer)
			_d3dVertexBuffer->Release();

		if (_d3dIndexBuffer)
			_d3dIndexBuffer->Release();

		if (_texture)
			_texture->Release();
	}
};


//struct GltfVertex // GLTF에서 사용하는 정점 구조체 -> DW계획 : 추후 필요없는 구조체는 삭제할 예정
//{
//	XMFLOAT3 _position;
//	XMFLOAT3 _normal;
//	XMFLOAT4 _tangent;
//	XMFLOAT2 _texCoord;
//};
//
//
//struct GltfPrimitiveData
//{
//	ComPtr<ID3D12Resource> _vertexBuffer;
//	ComPtr<ID3D12Resource> _indexBuffer;
//
//	D3D12_VERTEX_BUFFER_VIEW _vertexBufferView;
//	D3D12_INDEX_BUFFER_VIEW _indexBufferView;
//
//	UINT _indexCount = 0;
//	int _materialIndex = -1; // 이 프리미티브가 사용할 m_textures 벡터의 인덱스
//};

struct Vertex
{
public:
	//정점의 위치 벡터이다(모든 정점은 최소한 위치 벡터를 가져야 한다).
	XMFLOAT3 _position;
public:
	Vertex() { _position = XMFLOAT3(0.0f, 0.0f, 0.0f); }
	Vertex(XMFLOAT3 xmf3Position) { _position = xmf3Position; }
};
// (추가) 조명 효과를 표현하기 위한 정점 클래스이다. [PONG]
struct IlluminatedVertex : public Vertex
{
public:
	XMFLOAT3 _normal; // 법선 벡터
	XMFLOAT2 _texCoord; // 텍스처 좌표 (추가)
	XMFLOAT3 _tangent;

public:
	IlluminatedVertex() { 
		_position = XMFLOAT3(0.0f, 0.0f, 0.0f); 
		_normal = XMFLOAT3(0.0f, 0.0f, 0.0f); 
		_texCoord = XMFLOAT2(0.0f, 0.0f); // 추가
		_tangent = XMFLOAT3(0.0f, 0.0f, 0.0f);
	}
	IlluminatedVertex(XMFLOAT3 p, XMFLOAT3 n, XMFLOAT2 t, XMFLOAT3 tan) {
		_position = p;
		_normal = n;
		_texCoord = t;
		_tangent = tan;
	}
};

//struct GltfMeshData
//{
//	std::vector<IlluminatedVertex> _vertices;
//	std::vector<uint32_t> _indices; // 인덱스 타입은 accessor에 따라 달라질 수 있음 -> 이거 경고 막고싶은디...
//};

class Mesh : public Object
{
public:
	// [변경] 생성자는 이제 파일 경로만 받아서 CPU 메모리에 데이터를 로드하는 역할만 합니다.
	// 구체적인 파일 파싱은 파생 클래스(ReadObjMesh 등)에서 구현합니다.
	Mesh();
	virtual ~Mesh();

	// [추가] CPU 메모리에 로드된 데이터를 기반으로 실제 GPU 버퍼를 생성하는 함수입니다.
	// Renderer가 렌더링 직전에 호출해줍니다.
	
	virtual void upload_to_gpu(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, UINT64 targetFenceValue);

	virtual void release_upload_buffers();
	
	// [추가] GPU에 업로드되었는지 확인하는 플래그
	bool is_uploaded() const { return _isUploaded; }

	// 렌더링 함수는 VBV/IBV를 설정하고 DrawInstanced를 호출합니다.
	virtual void render(ID3D12GraphicsCommandList* commandList);
	virtual void render_instance(ID3D12GraphicsCommandList* commandList, size_t want_instance_count = 1) {};

	// CSM 렌더링을 위한 별도의 렌더링 함수
	virtual void render_CascadeShadowMap(ID3D12GraphicsCommandList* commandList);

	virtual const BoundingOrientedBox& bounding_box() const { return _orientedBoundingBox; }
protected:
	static BoundingOrientedBox CreateOOBB(XMFLOAT3 min, XMFLOAT3 max);
	virtual void upload_to_gpu_internal(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, UINT64 targetFenceValue);

	// [추가] 템플릿 함수로 다양한 정점 타입을 지원합니다.
	template<typename VertexType>
	void set_vertex_data_buffer(const std::vector<VertexType>& temp_vertices)
	{
		// 2. 메타데이터 설정
		_vertexCount = static_cast<UINT>(temp_vertices.size());
		_vertexStride = sizeof(VertexType);

		// 3. 원시 바이트 데이터를 부모 클래스의 버퍼로 복사
		_vertexDataBuffer.resize(_vertexStride * _vertexCount);
		memcpy(_vertexDataBuffer.data(), temp_vertices.data(), _vertexStride * _vertexCount);
	}


protected:
	bool _isUploaded = false;
	// [변경] 생성 시점에는 이 변수들에 정점/인덱스 데이터가 채워집니다.
	// upload_to_gpu가 호출될 때 실제 GPU 버퍼가 생성됩니다.
	std::vector<std::byte> _vertexDataBuffer; // 바이트 단위의 버퍼
	UINT _vertexCount = 0; // 정점 개수
	UINT _vertexStride = 0; // 정점 하나의 크기(바이트 단위)
	std::vector<UINT> _indices;


	D3D_PRIMITIVE_TOPOLOGY _primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; // 선으로 렌더링

	// [변경] 이 GPU 리소스들은 upload_to_gpu가 호출될 때 생성됩니다.
	ComPtr<ID3D12Resource> _vertexBuffer;
	ComPtr<ID3D12Resource> _indexBuffer;
	ComPtr<ID3D12Resource> _vertexUploadBuffer;
	ComPtr<ID3D12Resource> _indexUploadBuffer;

	D3D12_VERTEX_BUFFER_VIEW _vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW _indexBufferView;

	BoundingOrientedBox _orientedBoundingBox = BoundingOrientedBox();

};

struct CollisionPrimitive
{
	// 충돌 계산 전용의 최소화된 정점 데이터를 사용
	std::vector<Vertex> _vertices;

	// 정점을 연결하여 삼각형을 만드는 인덱스 데이터
	std::vector<uint32_t> _indices;

	// 광역 단계에서 사용할 AABB
	BoundingBox aabb;

	// (추가)협역 단계 및 시각화용 OBB
	DirectX::BoundingOrientedBox obb;

	// 월드 변환 행렬
	XMFLOAT4X4 worldTransform;
	CollisionPrimitive() = default;
};


class DebugCollisionBox : public Mesh
{
public:
	DebugCollisionBox(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, XMFLOAT4 color);
	virtual ~DebugCollisionBox();
};

// 정점/인덱스 데이터를 받아 와이어프레임으로 그리는 범용 디버그 메시 클래스
class DebugWireframeMesh : public Mesh
{
public:
	// 생성자에서 정점과 인덱스 목록을 직접 받습니다.
	DebugWireframeMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, XMFLOAT4 color);
	virtual ~DebugWireframeMesh();
};