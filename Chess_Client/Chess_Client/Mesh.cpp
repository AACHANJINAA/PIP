#include "stdafx.h"
#include "Mesh.h"

BoundingOrientedBox CreateOOBB(XMFLOAT3 min, XMFLOAT3 max) {
	// 중심점 계산 (min과 max의 중간값)
	XMFLOAT3 center(
		(min.x + max.x) * 0.5f,
		(min.y + max.y) * 0.5f,
		(min.z + max.z) * 0.5f
	);

	// 크기 계산 (max - min 값의 절반)
	XMFLOAT3 extents(
		(max.x - min.x) * 0.5f,
		(max.y - min.y) * 0.5f,
		(max.z - min.z) * 0.5f
	);

	// 기본 방향 (회전 없음)
	XMFLOAT4 orientation(0.0f, 0.0f, 0.0f, 1.0f);

	// OOBB 생성
	BoundingOrientedBox oobb(center, extents, orientation);
	return oobb;
}

CMesh::CMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{

}
CMesh::~CMesh()
{
	if (m_pd3dVertexBuffer) m_pd3dVertexBuffer->Release();
	if (m_pd3dVertexUploadBuffer) m_pd3dVertexUploadBuffer->Release();

	if (m_pd3dIndexBuffer) m_pd3dIndexBuffer->Release();
	if (m_pd3dIndexUploadBuffer) m_pd3dIndexUploadBuffer->Release();
}

void CMesh::ReleaseUploadBuffers()
{
	//정점 버퍼를 위한 업로드 버퍼를 소멸시킨다. 
	if (m_pd3dVertexUploadBuffer) m_pd3dVertexUploadBuffer->Release();
	m_pd3dVertexUploadBuffer = NULL;

	if (m_pd3dIndexUploadBuffer) m_pd3dIndexUploadBuffer->Release();
	m_pd3dIndexUploadBuffer = NULL;

};



void CMesh::UpdateVertices(ID3D12GraphicsCommandList* pd3dCommandList)
{

	// 1. m_pd3dVertexUploadBufferForUpdate (UPLOAD 힙)에 새로운 정점 데이터 매핑 및 복사
	void* pMappedData;
	D3D12_RANGE readRange = { 0, 0 }; // CPU는 읽지 않으므로 0,0

	// map을 사용해서 데이터를 업로드 힙에 쓰는 것은 업로드 힙의 고유한 특성이므로 상태변경이 필요없다.
	m_pd3dUploadVertexBuffer->Map(0, &readRange, &pMappedData);
	memcpy(pMappedData, m_Vertexvec.data(), m_nStride * m_nVertices); // m_Vertexvec의 현재 데이터 복사
	m_pd3dUploadVertexBuffer->Unmap(0, nullptr); // 매핑 해제

	// 2. Resource Barrier (상태 전환) - DEFAULT 힙 버퍼를 COPY_DEST로 전환

	D3D12_RESOURCE_BARRIER barrierToCopyDest;
	ZeroMemory(&barrierToCopyDest, sizeof(barrierToCopyDest)); // 구조체 초기화

	barrierToCopyDest.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierToCopyDest.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrierToCopyDest.Transition.pResource = m_pd3dVertexBuffer; // 전환할 DEFAULT 힙 버퍼
	barrierToCopyDest.Transition.StateBefore = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; // 현재 상태
	barrierToCopyDest.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;               // 목표 상태
	barrierToCopyDest.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; // 모든 서브리소스에 적용

	pd3dCommandList->ResourceBarrier(1, &barrierToCopyDest); // 명령 리스트에 추가


	// 3. UPLOAD 힙 버퍼 -> DEFAULT 힙 버퍼로 데이터 복사 (GPU 작업)

	pd3dCommandList->CopyBufferRegion(m_pd3dVertexBuffer, 0,
		m_pd3dUploadVertexBuffer, 0,
		m_nStride * m_nVertices);


	// 4. Resource Barrier (상태 전환) - DEFAULT 힙 버퍼를 VERTEX_AND_CONSTANT_BUFFER로 전환

	D3D12_RESOURCE_BARRIER barrierToVertexAndConstantBuffer;
	ZeroMemory(&barrierToVertexAndConstantBuffer, sizeof(barrierToVertexAndConstantBuffer)); // 구조체 초기화

	barrierToVertexAndConstantBuffer.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierToVertexAndConstantBuffer.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrierToVertexAndConstantBuffer.Transition.pResource = m_pd3dVertexBuffer; // 전환할 DEFAULT 힙 버퍼
	barrierToVertexAndConstantBuffer.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;                 // 현재 상태
	barrierToVertexAndConstantBuffer.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; // 목표 상태
	barrierToVertexAndConstantBuffer.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; // 모든 서브리소스에 적용

	pd3dCommandList->ResourceBarrier(1, &barrierToVertexAndConstantBuffer); // 명령 리스트에 추가
}

void CMesh::ChangeColor(ID3D12GraphicsCommandList* pd3dCommandList, float r, float g, float b, float a)
{
	for (CDiffusedVertex& vertex : m_Vertexvec)
	{
		vertex.m_xmf4Diffuse.x = r;
		vertex.m_xmf4Diffuse.y = g;
		vertex.m_xmf4Diffuse.z = b;
		vertex.m_xmf4Diffuse.w = a;
	}
	UpdateVertices(pd3dCommandList);
}


void CMesh::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
	pd3dCommandList->IASetPrimitiveTopology(m_d3dPrimitiveTopology);
	pd3dCommandList->IASetVertexBuffers(m_nSlot, 1, &m_d3dVertexBufferView);
	if (m_pd3dIndexBuffer)
	{
		pd3dCommandList->IASetIndexBuffer(&m_d3dIndexBufferView);
		pd3dCommandList->DrawIndexedInstanced(m_nIndices, 1, 0, 0, 0);
		//인덱스 버퍼가 있으면 인덱스 버퍼를 파이프라인(IA: 입력 조립기)에 연결하고 인덱스를 사용하여 렌더링한다. 
	}
	else
	{
		pd3dCommandList->DrawInstanced(m_nVertices, 1, m_nOffset, 0);
	}
}

BOOL CMesh::RayIntersectionByTriangle(XMVECTOR& xmRayOrigin, XMVECTOR& xmRayDirection, XMVECTOR v0, XMVECTOR v1, XMVECTOR v2, float* pfNearHitDistance)
{
	float fHitDistance;
	BOOL bIntersected = TriangleTests::Intersects(xmRayOrigin, xmRayDirection, v0, v1, v2, fHitDistance);
	if (bIntersected && (fHitDistance < *pfNearHitDistance)) *pfNearHitDistance = fHitDistance;

	return(bIntersected);
}

int CMesh::CheckRayIntersection(XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection, float* pfNearHitDistance)
{
	int nIntersections = 0;
	bool bIntersected = m_xmOOBB.Intersects(xmvPickRayOrigin, xmvPickRayDirection, *pfNearHitDistance);

	if (bIntersected)
	{
		if (m_pd3dIndexBuffer)
		{
			for (UINT i = 0; i < m_nIndices; i)
			{
				XMVECTOR v0 = XMLoadFloat3(&m_Vertexvec[m_Indexvec[i++]].m_xmf3Position);
				XMVECTOR v1 = XMLoadFloat3(&m_Vertexvec[m_Indexvec[i++]].m_xmf3Position);
				XMVECTOR v2 = XMLoadFloat3(&m_Vertexvec[m_Indexvec[i++]].m_xmf3Position);
				BOOL bIntersected = RayIntersectionByTriangle(xmvPickRayOrigin, xmvPickRayDirection, v0, v1, v2, pfNearHitDistance);
				if (bIntersected) nIntersections++;
				break;
			}
		}
		else
		{
			__debugbreak();
		}
	}

	return(nIntersections);
}

CReadObjMesh::CReadObjMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::string str)
{
	std::ifstream in{ str };
	if (!in) {
		return;
	}

	std::string Line{};

	std::string type{};

	int FaceNum{}; // 1,2,3 반복

	float x, y, z;

	UINT a, b, c;

	//std::vector<CDiffusedVertex> m_Vertexvec{};
	//std::vector<UINT> AllIndex{};


	while (std::getline(in, Line))
	{
		std::istringstream iss(Line);

		iss >> type;

		if (type == "v") {
			iss >> x >> y >> z;
			m_Vertexvec.emplace_back(x, y, z, RANDOM_COLOR);

			
		}
		if (type == "s")
		{
			iss >> type;
			if (type == "1")
			{
				//break;
			}
		}
		if (type == "f")
		{
			iss >> a >> b >> c;

			m_Indexvec.emplace_back(a - 1);
			m_Indexvec.emplace_back(b - 1);
			m_Indexvec.emplace_back(c - 1);

		}
	}
	auto [min_x, max_x] = 
		std::minmax_element(m_Vertexvec.begin(), m_Vertexvec.end(),[](const CDiffusedVertex& a, const CDiffusedVertex& b)
		{
			return a.m_xmf3Position.x < b.m_xmf3Position.x;
		});
	auto [min_y, max_y] =
		std::minmax_element(m_Vertexvec.begin(), m_Vertexvec.end(), [](const CDiffusedVertex& a, const CDiffusedVertex& b)
		{
			return a.m_xmf3Position.y < b.m_xmf3Position.y;
		});
	auto [min_z, max_z] =
		std::minmax_element(m_Vertexvec.begin(), m_Vertexvec.end(), [](const CDiffusedVertex& a, const CDiffusedVertex& b)
		{
			return a.m_xmf3Position.z < b.m_xmf3Position.z;
		});

	m_nStride = sizeof(CDiffusedVertex);
	m_nVertices = m_Vertexvec.size();
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	m_pd3dVertexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_Vertexvec.data(),
		m_nStride * m_nVertices, D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&m_pd3dVertexUploadBuffer);

	// D3D12_HEAP_TYPE_UPLOAD 버퍼 (CPU 업데이트용)
// 이 버퍼에 CPU가 직접 데이터를 맵핑해서 업데이트합니다.
	m_pd3dUploadVertexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, nullptr, // 초기 데이터는 맵핑 후 쓸 것이므로 nullptr
		m_nStride * m_nVertices, D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ, // 업로드 힙이기 때문에 GENERIC_READ로 설정
		nullptr); // UPLOAD 버퍼는 업로드 버퍼가 필요 없음


	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress();
	m_d3dVertexBufferView.StrideInBytes = m_nStride;
	m_d3dVertexBufferView.SizeInBytes = m_nStride * m_nVertices;


	m_nIndices = m_Indexvec.size();
	//인덱스 버퍼를 생성한다. 인덱스 버퍼는 변경 안될것임
	m_pd3dIndexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_Indexvec.data(),
		sizeof(UINT) * m_nIndices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER,
		&m_pd3dIndexUploadBuffer);

	//인덱스 버퍼 뷰를 생성한다.
	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = sizeof(UINT) * m_nIndices;

	m_Bottom = min_y->m_xmf3Position.y;
	m_Top = max_y->m_xmf3Position.y;

	m_Right = max_x->m_xmf3Position.x;
	m_Left = min_x->m_xmf3Position.x;

	m_Front = max_z->m_xmf3Position.z;
	m_Back = min_z->m_xmf3Position.z;

	m_xmOOBB = CreateOOBB(XMFLOAT3(min_x->m_xmf3Position.x, min_y->m_xmf3Position.y, min_z->m_xmf3Position.z),
						  XMFLOAT3(max_x->m_xmf3Position.x, max_y->m_xmf3Position.y, max_z->m_xmf3Position.z));
}


CReadObjMesh::~CReadObjMesh()
{

}