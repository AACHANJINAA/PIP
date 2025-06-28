#pragma once
#include "stdafx.h"
//정점을 표현하기 위한 클래스를 선언한다.
class CVertex
{
public:
	//정점의 위치 벡터이다(모든 정점은 최소한 위치 벡터를 가져야 한다).
	XMFLOAT3 m_xmf3Position;
public:
	CVertex() { m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f); }
	CVertex(XMFLOAT3 xmf3Position) { m_xmf3Position = xmf3Position; }
	~CVertex() {}
};

class CDiffusedVertex : public CVertex
{
public:
	//정점의 색상이다. 
	XMFLOAT4 m_xmf4Diffuse;
public:
	CDiffusedVertex() {
		m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f); m_xmf4Diffuse =
			XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	CDiffusedVertex(float x, float y, float z, XMFLOAT4 xmf4Diffuse) {
		m_xmf3Position =
			XMFLOAT3(x, y, z); m_xmf4Diffuse = xmf4Diffuse;
	}
	CDiffusedVertex(XMFLOAT3 xmf3Position, XMFLOAT4 xmf4Diffuse) {
		m_xmf3Position =
			xmf3Position; m_xmf4Diffuse = xmf4Diffuse;
	}
	~CDiffusedVertex() {}
};

class CMesh
{
public:
	CMesh() {}
	CMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual ~CMesh();

private:
	int m_nReferences = 0;

public:
	void AddRef() { m_nReferences++; }
	void Release() { if (--m_nReferences <= 0) delete this; }
	void ReleaseUploadBuffers();

	virtual void UpdateVertices(ID3D12GraphicsCommandList* pd3dCommandList);

	void ChangeColor(ID3D12GraphicsCommandList* pd3dCommandList,float r, float g, float b, float a = 1.f);

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

protected:
	ID3D12Resource* m_pd3dIndexBuffer = NULL;

	ID3D12Resource* m_pd3dIndexUploadBuffer = NULL;

	std::vector<UINT> m_Indexvec; // 인덱스 버퍼를 저장하기 위한 벡터(인덱스 버퍼는 변함이 없음)

	std::vector<CDiffusedVertex> m_Vertexvec; // 버텍스 버퍼를 저장하기 위한 벡터

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
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList);

	BoundingOrientedBox			m_xmOOBB = BoundingOrientedBox();

	BOOL RayIntersectionByTriangle(XMVECTOR& xmRayOrigin, XMVECTOR& xmRayDirection, XMVECTOR v0, XMVECTOR v1, XMVECTOR v2, float* pfNearHitDistance);
	int CheckRayIntersection(XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection, float* pfNearHitDistance); // 모델좌표계에서의 레이 좌표와 방향을 넣어준다.
	// DW설명 : 모델좌표계에서 충돌체크를 할 것이기 때문에 메쉬클래스에서 가지고 있는 것 같다.

	float m_Left{};
	float m_Top{};
	float m_Right{};
	float m_Bottom{};
	float m_Front{};
	float m_Back{};


};

class CReadObjMesh : public CMesh
{
public:
	CReadObjMesh() {};
	CReadObjMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::string str);

	//void ChangeColor(float r, float g, float b, float a);

	//virtual void UpdateVertices(ID3D12GraphicsCommandList* pd3dCommandList);


	virtual ~CReadObjMesh();
};