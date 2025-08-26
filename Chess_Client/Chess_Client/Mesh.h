#pragma once
#include "stdafx.h"
#include "json.hpp"
#include <assimp/Importer.hpp>      // Assimp 로더
#include <assimp/scene.h>           // Assimp scene 객체
#include <assimp/postprocess.h>     // Assimp 후처리 옵션
#include <assimp/material.h> // AI_MATKEY_TEXTURE_DIFFUSE, AI_MATKEY_COLOR_DIFFUSE 정의 포함
// nlohmann/json 헤더 // json 파싱 위해 추가
using json = nlohmann::json;

//정점을 표현하기 위한 클래스를 선언한다.
class Vertex
{
public:
	//정점의 위치 벡터이다(모든 정점은 최소한 위치 벡터를 가져야 한다).
	XMFLOAT3 m_xmf3Position;
public:
	Vertex() { m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f); }
	Vertex(XMFLOAT3 xmf3Position) { m_xmf3Position = xmf3Position; }
	~Vertex() {}
};

// Node 구조체 정의 DW : GLTF/GLB 파일에서 노드 정보를 표현하기 위해 추가
struct Node
{
	std::string name;
	int parentIndex = -1; // 부모가 없으면 -1
	std::vector<int> childrenIndices;

	// 변환 정보
	XMFLOAT3 translation = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f }; // 쿼터니언
	XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };

	int meshIndex = -1;
	int skinIndex = -1;
};

// 스키닝 정보를 포함하는 새로운 정점 구조체 -> Vertex 대신에 사용할 것임
struct SkinnedVertex
{
	XMFLOAT3 m_xmf3Position;
	XMFLOAT3 m_xmf3Normal;
	XMFLOAT2 m_xmf2TexCoord;

	// 추가된 스키닝 데이터
	XMFLOAT4 m_xmf4BoneIndices; // 영향을 주는 뼈의 인덱스 (최대 4개)
	XMFLOAT4 m_xmf4BoneWeights; // 각 뼈로부터 받는 영향(가중치)
};

struct MeshPrimitive
{
	ID3D12Resource* m_pd3dVertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_d3dVertexBufferView{};

	ID3D12Resource* m_pd3dIndexBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW m_d3dIndexBufferView{};

	UINT m_nIndices = 0;

	// 나중에 재질(Material) 인덱스도 여기에 저장할 수 있습니다.
	int m_nMaterialIndex = -1;

	ID3D12Resource* m_pTexture = nullptr; // 텍스처 리소스를 저장할 포인터
	D3D12_GPU_DESCRIPTOR_HANDLE m_d3dGpuSrvHandle{}; // SRV 핸들을 저장할 변수

	// 소멸자에서 리소스 해제
	~MeshPrimitive() {
		if (m_pd3dVertexBuffer)
			m_pd3dVertexBuffer->Release();

		if (m_pd3dIndexBuffer)
			m_pd3dIndexBuffer->Release();

		if (m_pTexture)
			m_pTexture->Release();
	}
};


// (추가) 조명 효과를 표현하기 위한 정점 클래스이다. [PONG]
class IlluminatedVertex : public Vertex
{
public:
	XMFLOAT3 m_xmf3Normal; // 법선 벡터
	XMFLOAT2 m_xmf2Texcoord; // 텍스처 좌표 (추가)
	XMFLOAT4 m_xmf4Diffuse; // CDiffusedVertex꺼 가져오기

public:
	IlluminatedVertex() { 
		m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f); 
		m_xmf3Normal = XMFLOAT3(0.0f, 0.0f, 0.0f); 
		m_xmf2Texcoord = XMFLOAT2(0.0f, 0.0f); // 추가
		m_xmf4Diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	IlluminatedVertex(XMFLOAT3 p, XMFLOAT3 n, XMFLOAT2 t, XMFLOAT4 c = RANDOM_COLOR) {
		m_xmf3Position = p;
		m_xmf3Normal = n;
		m_xmf2Texcoord = t;
		m_xmf4Diffuse = c;
	}
	~IlluminatedVertex() {}
};

class Mesh
{
public:
	Mesh() {}
	Mesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual ~Mesh();

protected:
	ID3D12Resource* m_pd3dVertexBuffer = NULL;
	ID3D12Resource* m_pd3dVertexUploadBuffer = NULL;

	ID3D12Resource* m_pd3dUploadVertexBuffer = NULL; // 매 틱마다 업데이트 해주기 위함

	D3D12_VERTEX_BUFFER_VIEW m_d3dVertexBufferView;
	D3D12_PRIMITIVE_TOPOLOGY m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	UINT m_nSlot = 0;
	UINT m_nVertices = 0;
	UINT m_nStride = 0;
	UINT m_nOffset = 0;

	ID3D12Resource* m_pd3dIndexBuffer = NULL;

	ID3D12Resource* m_pd3dIndexUploadBuffer = NULL;

	std::vector<UINT> m_Indexvec; // 인덱스 버퍼를 저장하기 위한 벡터(인덱스 버퍼는 변함이 없음)

	std::vector<IlluminatedVertex> m_Vertexvec; // 버텍스 버퍼를 저장하기 위한 벡터 -> (수정) IlluminatedVertex class로 변경 [PONG]

	/*인덱스 버퍼(인덱스의 배열)와 인덱스 버퍼를 위한 업로드 버퍼에 대한 인터페이스 포인터이다. 인덱스 버퍼는 정점
	버퍼(배열)에 대한 인덱스를 가진다.*/
	D3D12_INDEX_BUFFER_VIEW m_d3dIndexBufferView;
	UINT m_nIndices = 0;
	//인덱스 버퍼에 포함되는 인덱스의 개수이다. 
	UINT m_nStartIndex = 0;
	//인덱스 버퍼에서 메쉬를 그리기 위해 사용되는 시작 인덱스이다. 
	int m_nBaseVertex = 0;
	//인덱스 버퍼의 인덱스에 더해질 인덱스이다. 

public:
	BoundingOrientedBox	m_xmOOBB = BoundingOrientedBox();

	float m_Left;
	float m_Top;
	float m_Right;
	float m_Bottom;
	float m_Front;
	float m_Back;

private:
	int m_nReferences = 0;

public:
	void AddRef() { m_nReferences++; }
	void Release() { if (--m_nReferences <= 0) delete this; }
	virtual void ReleaseUploadBuffers();

	virtual void UpdateVertices(ID3D12GraphicsCommandList* pd3dCommandList);

	void ChangeColor(ID3D12GraphicsCommandList* pd3dCommandList,float r, float g, float b, float a = 1.f);

	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList);

	BOOL RayIntersectionByTriangle(XMVECTOR& xmRayOrigin, XMVECTOR& xmRayDirection, XMVECTOR v0, XMVECTOR v1, XMVECTOR v2, float* pfNearHitDistance);
	int CheckRayIntersection(XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection, float* pfNearHitDistance); // 모델좌표계에서의 레이 좌표와 방향을 넣어준다.
	// DW설명 : 모델좌표계에서 충돌체크를 할 것이기 때문에 메쉬클래스에서 가지고 있는 것 같다.

	BoundingOrientedBox GetBoundingBox() const { return m_xmOOBB; }
};

struct Material                                                                                                                                          
{                                                                                                                                              
	std::string name;
	XMFLOAT4 Ka; // Ambient
	XMFLOAT4 Kd; // Diffuse
	XMFLOAT4 Ks; // Specular
	float Ns;    // Specular Exponent 
};

class ReadObjMesh : public Mesh
{
public:
	ReadObjMesh() {};
	ReadObjMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::string str);

	//void ChangeColor(float r, float g, float b, float a);

	//virtual void UpdateVertices(ID3D12GraphicsCommandList* pd3dCommandList);


	virtual ~ReadObjMesh();

private:
	std::map<std::string, Material> m_mapMaterials;
	void LoadMtlFile(const std::string& objFilePath, const std::string& mtlFileName);
};

class ReadGlbMesh : public Mesh
{
public:
	ReadGlbMesh() {}
	virtual ~ReadGlbMesh();

	ReadGlbMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::string str, class Scene* pScene);


	// Mesh의 Render 함수를 오버라이드하여 GLB/glTF 모델만의 렌더링 로직을 구현
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList) override;
	// 업로드 버퍼 해제도 오버라이드
	void ReleaseUploadBuffers() override;

	UINT GetTextureCount() { return _Textures; } // 로드된 텍스처 개수를 반환하는 함수

private:
	UINT _Textures = 0; // 로드된 텍스처 개수를 저장할 변수

	// 렌더링에 필요한 데이터는 ReadGlbMesh가 직접 관리
	std::vector<std::unique_ptr<MeshPrimitive>> m_primitives;

	// 로딩 과정에서만 사용할 멤버 변수들
	std::vector<Node> m_Nodes;
	std::vector<ID3D12Resource*> m_vUploadBuffers;

	// 반환값: {픽셀 데이터, 가로 크기, 세로 크기}
	std::tuple<std::vector<unsigned char>, UINT, UINT> LoadImageFromGLB(const json& j, const std::vector<char>& binaryData, int textureIndex);

	template<typename T>
	std::pair<T*, size_t> getData(const json& j, const std::vector<char>& binaryData, int accessorIndex)
	{
		// --- 방어 코드 1: accessorIndex가 유효한 범위 내에 있는지 확인 ---
		if (accessorIndex < 0 || !j.contains("accessors") || accessorIndex >= j["accessors"].size()) {
			std::cerr << "Error: Invalid accessorIndex provided: " << accessorIndex << std::endl;
			return { nullptr, 0 };
		}
		const auto& accessor = j["accessors"][accessorIndex];

		// --- 방어 코드 2: accessor가 "bufferView" 키를 가지고 있는지 확인 ---
		if (!accessor.contains("bufferView")) {
			std::cerr << "Error: Accessor " << accessorIndex << " does not contain a bufferView." << std::endl;
			return { nullptr, 0 };
		}
		int bufferViewIndex = accessor["bufferView"];

		// --- 방어 코드 3: bufferViewIndex가 유효한 범위 내에 있는지 확인 ---
		if (bufferViewIndex < 0 || !j.contains("bufferViews") || bufferViewIndex >= j["bufferViews"].size()) {
			std::cerr << "Error: Invalid bufferViewIndex found in accessor " << accessorIndex << ": " << bufferViewIndex << std::endl;
			return { nullptr, 0 };
		}
		const auto& bufferView = j["bufferViews"][bufferViewIndex];

		// 기존 방어 코드 (byteOffset 처리)
		size_t totalOffset = 0;
		if (bufferView.contains("byteOffset")) {
			totalOffset += bufferView["byteOffset"].get<size_t>();
		}
		if (accessor.contains("byteOffset")) {
			totalOffset += accessor["byteOffset"].get<size_t>();
		}

		const char* dataStart = binaryData.data() + totalOffset;
		size_t count = accessor["count"];

		// 기존 방어 코드 (메모리 범위 초과 방지)
		size_t dataSizeInBytes = count * sizeof(T);
		if ((totalOffset + dataSizeInBytes) > binaryData.size()) {
			std::cerr << "Error: Data access is out of bounds for the binary buffer." << std::endl;
			return { nullptr, 0 };
		}
		return { reinterpret_cast<T*>(const_cast<char*>(dataStart)), count };
	}
};

struct CollisionPrimitive
{
	// 충돌 계산 전용의 최소화된 정점 데이터를 사용
	std::vector<Vertex> vertices;

	// 정점을 연결하여 삼각형을 만드는 인덱스 데이터
	std::vector<uint32_t> indices;

	// 광역 단계에서 사용할 AABB
	BoundingBox aabb;

	// (추가)협역 단계 및 시각화용 OBB
	DirectX::BoundingOrientedBox oobb;

	// 월드 변환 행렬
	XMFLOAT4X4 worldTransform;
	CollisionPrimitive() = default;
};

class ReadFbxMesh : public Mesh
{
public:
	ReadFbxMesh() {};
	ReadFbxMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::string str);
	virtual ~ReadFbxMesh();

	const std::vector<CollisionPrimitive>& GetCollisionPrimitives() const { return _collisionPrimitives; }

private:
	// Assimp Scene의 노드를 재귀적으로 처리하는 함수
	void ProcessNode(aiNode* node, const aiScene* scene, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	// Assimp Mesh를 처리하여 정점/인덱스 데이터를 추출하는 함수
	void ProcessMesh(aiMesh* mesh, const aiScene* scene, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

private:
	std::string m_texturePath; // 로드된 텍스처 파일 경로 (단순화를 위해 하나만 저장)
	std::vector<CollisionPrimitive> _collisionPrimitives;
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