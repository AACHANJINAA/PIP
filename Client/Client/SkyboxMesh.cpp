#include "stdafx.h"
#include "SkyboxMesh.h"

SkyboxMesh::SkyboxMesh(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	// 위치만 필요하기에 Vertex 구조체 사용
	std::vector<Vertex> vertices = {
		Vertex(XMFLOAT3(-1.0f, +1.0f, -1.0f)), // 0
	    Vertex(XMFLOAT3(+1.0f, +1.0f, -1.0f)), // 1
	    Vertex(XMFLOAT3(+1.0f, +1.0f, +1.0f)), // 2
	    Vertex(XMFLOAT3(-1.0f, +1.0f, +1.0f)), // 3
	    Vertex(XMFLOAT3(-1.0f, -1.0f, -1.0f)), // 4 
	    Vertex(XMFLOAT3(+1.0f, -1.0f, -1.0f)), // 5
	    Vertex(XMFLOAT3(+1.0f, -1.0f, +1.0f)), // 6
	    Vertex(XMFLOAT3(-1.0f, -1.0f, +1.0f))  // 7
	};

	// 큐브의 6면에 대한 12개 삼각형을 정의하는 인덱스 데이터입니다.
	std::vector<UINT> indices = {
		// Front
		0, 1, 2, 
		0, 2, 3,
		// Back
		4, 5, 6, 
		4, 6, 7,
		// Left
		4, 7, 3,
		4, 3, 0,
		// Right
		1, 5, 6,
		1, 6, 2,
		// Top
		3, 2, 6,
		3, 6, 7,
		// Bottom
		0, 4, 5,
		0, 5, 1
	};
	// 부모 클래스(Mesh)의 함수를 사용하여 정점 데이터를 설정합니다.
    set_vertex_data_buffer(vertices);
	// 인덱스 데이터를 설정합니다.
    _indices = indices;
	// 프리미티브 토폴로지를 삼각형 리스트로 설정합니다.
    _primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	
}