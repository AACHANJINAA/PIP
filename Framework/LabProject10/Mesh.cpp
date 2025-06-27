#include "Mesh.h"

// mesh.cpp
#include <array>

CMesh::CMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	::ZeroMemory(&m_d3dVertexBufferView, sizeof(D3D12_VERTEX_BUFFER_VIEW));
	::ZeroMemory(&m_d3dIndexBufferView, sizeof(D3D12_INDEX_BUFFER_VIEW));
}

CMesh::~CMesh()
{
}

void CMesh::ReleaseUploadBuffers()
{
	if (m_pd3dVertexUploadBuffer) m_pd3dVertexUploadBuffer.Reset();
	if (m_pd3dIndexUploadBuffer) m_pd3dIndexUploadBuffer.Reset();
};

void CMesh::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
	pd3dCommandList->IASetPrimitiveTopology(m_d3dPrimitiveTopology);
	pd3dCommandList->IASetVertexBuffers(m_nSlot, 1, &m_d3dVertexBufferView);
	if (m_pd3dIndexBuffer)
	{
		pd3dCommandList->IASetIndexBuffer(&m_d3dIndexBufferView);
		pd3dCommandList->DrawIndexedInstanced(m_nIndices, 1, 0, 0, 0);
	}
	else
	{
		pd3dCommandList->DrawInstanced(m_nVertices, 1, m_nOffset, 0);
	}
}

CTriangleMesh::CTriangleMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) : CMesh(pd3dDevice, pd3dCommandList)
{
	m_nVertices = 3;
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	std::array<CDiffusedVertex, 3> pVertices{ CDiffusedVertex(XMFLOAT3(0.0f, 0.5f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)),
											   CDiffusedVertex(XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)),
											   CDiffusedVertex(XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT4(Colors::Blue)) };
	
	m_pd3dVertexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, pVertices.data(),
	m_nStride* m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, & m_pd3dVertexUploadBuffer);
	
	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress();
	m_d3dVertexBufferView.StrideInBytes = m_nStride;
	m_d3dVertexBufferView.SizeInBytes = m_nStride * m_nVertices;
}

CCubeMeshDiffused::CCubeMeshDiffused(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList
	* pd3dCommandList, float fWidth, float fHeight, float fDepth) : CMesh(pd3dDevice,
		pd3dCommandList)
{
	m_nVertices = 8;
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	float fx = fWidth * 0.5f, fy = fHeight * 0.5f, fz = fDepth * 0.5f;
	std::array<CDiffusedVertex, 8> pVertices = {
		CDiffusedVertex(XMFLOAT3(-fx, +fy, -fz), RANDOM_COLOR),
		CDiffusedVertex(XMFLOAT3(+fx, +fy, -fz), RANDOM_COLOR),
		CDiffusedVertex(XMFLOAT3(+fx, +fy, +fz), RANDOM_COLOR),
		CDiffusedVertex(XMFLOAT3(-fx, +fy, +fz), RANDOM_COLOR),
		CDiffusedVertex(XMFLOAT3(-fx, -fy, -fz), RANDOM_COLOR),
		CDiffusedVertex(XMFLOAT3(+fx, -fy, -fz), RANDOM_COLOR),
		CDiffusedVertex(XMFLOAT3(+fx, -fy, +fz), RANDOM_COLOR),
		CDiffusedVertex(XMFLOAT3(-fx, -fy, +fz), RANDOM_COLOR)
	};
	m_pd3dVertexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, pVertices.data(),
		m_nStride * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dVertexUploadBuffer);
	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress(); m_d3dVertexBufferView.StrideInBytes = m_nStride;
	m_d3dVertexBufferView.SizeInBytes = m_nStride * m_nVertices;
	m_nIndices = 36;
	std::array<UINT, 36> pnIndices{
		//앞면(Front) 사각형의 위쪽 삼각형
		3, 1, 0,
		//앞면(Front) 사각형의 아래쪽 삼각형
		2, 1, 3,
		//윗면(Top) 사각형의 위쪽 삼각형
		0, 5, 4,
		//윗면(Top) 사각형의 아래쪽 삼각형
		1, 5, 0,
		//뒷면(Back) 사각형의 위쪽 삼각형
		3, 4, 7,
		//뒷면(Back) 사각형의 아래쪽 삼각형
		0, 4, 3,
		//아래면(Bottom) 사각형의 위쪽 삼각형
		1, 6, 5,
		//아래면(Bottom) 사각형의 아래쪽 삼각형
		2, 6, 1,
		//옆면(Left) 사각형의 위쪽 삼각형
		2, 7, 6,
		//옆면(Left) 사각형의 아래쪽 삼각형
		3, 7, 2,
		//옆면(Right) 사각형의 위쪽 삼각형
		6, 4, 5,
		//옆면(Right) 사각형의 아래쪽 삼각형
		7, 4, 6
	};
	m_pd3dIndexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, pnIndices.data(),
	sizeof(UINT)* m_nIndices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER,
		& m_pd3dIndexUploadBuffer);
	//인덱스 버퍼 뷰를 생성한다. 
	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = sizeof(UINT) * m_nIndices;
}

CCubeMeshDiffused::~CCubeMeshDiffused()
{
}
