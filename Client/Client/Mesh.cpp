#include "stdafx.h"
#include "Mesh.h"
#include "Scene.h"
#include <algorithm>

#include "ResourceManager.h"


// --- Mesh Base Class ---

Mesh::Mesh() : _vertexStride{ 0 }
{
	set_name("Mesh");
}

Mesh::~Mesh()
{
	if (_vertexBuffer)
	{
		_vertexBuffer.Reset();
	}
	if (_vertexUploadBuffer)
	{
		_vertexUploadBuffer.Reset();
	}
	if (_indexBuffer)
	{
		_indexBuffer.Reset();
	}
	if (_indexUploadBuffer)
	{
		_indexUploadBuffer.Reset();
	}
}

void Mesh::upload_to_gpu_internal(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, UINT64 targetFenceValue)
{
	// 이미 업로드되었다면 중복 실행 방지
	/*if (_isUploaded || _verticesDataBuffer.empty() ) return;
	if ( 0 == _vertexStride)
	{
		CERROR("stride 설정 안됨")
	}*/
	// --- 기존 생성자에 있던 GPU 버퍼 생성 로직이 여기로 이전 ---

	// 정점 버퍼 생성
	_vertexBuffer = ::CreateBufferResource(device, commandList, _vertexDataBuffer.data(), static_cast<UINT>(_vertexDataBuffer.size()),
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &_vertexUploadBuffer);

	// 인덱스 버퍼 생성 (인덱스가 있는 경우)
	if (!_indices.empty())
	{
		_indexBuffer = ::CreateBufferResource(device, commandList, _indices.data(), 
			sizeof(UINT) * static_cast<UINT>(_indices.size()),
			D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &_indexUploadBuffer);
	}

	// 버퍼 뷰 설정
	_vertexBufferView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
	_vertexBufferView.StrideInBytes = _vertexStride;
	_vertexBufferView.SizeInBytes = static_cast<UINT>(_vertexDataBuffer.size());

	if (!_indices.empty())
	{
		_indexBufferView.BufferLocation = _indexBuffer->GetGPUVirtualAddress();
		_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		_indexBufferView.SizeInBytes = sizeof(UINT) * static_cast<UINT>(_indices.size());
	}

	// 이거 중요!
	/*_isUploaded = true;*/
}

void Mesh::	upload_to_gpu(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, UINT64 targetFenceValue)
{
	// 1. 기존 내부 로직(버퍼 생성 및 복사 명령 기록) 실행
	upload_to_gpu_internal(device, commandList, targetFenceValue);

	// 2. [핵심] 명령 기록 직후, 리소스 매니저의 삭제 대기열에 등록!
	auto rm = ResourceManager::instance();
	if (_vertexUploadBuffer) {
		rm->register_upload_buffer(_vertexUploadBuffer, targetFenceValue);
		_vertexUploadBuffer.Reset(); // Mesh는 이제 소유권을 포기함 (큐가 관리)
	}
	if (_indexUploadBuffer) {
		rm->register_upload_buffer(_indexUploadBuffer, targetFenceValue);
		_indexUploadBuffer.Reset();
	}

	_isUploaded = true;
}

void Mesh::release_upload_buffers()
{
	if (_vertexUploadBuffer) _vertexUploadBuffer.Reset();
	if (_indexUploadBuffer) _indexUploadBuffer.Reset();
}


void Mesh::render(ID3D12GraphicsCommandList* commandList)
{
	if (!_isUploaded) return;

	commandList->IASetPrimitiveTopology(_primitiveTopology);
	commandList->IASetVertexBuffers(0, 1, &_vertexBufferView);

	if (!_indices.empty())
	{
		commandList->IASetIndexBuffer(&_indexBufferView);
		commandList->DrawIndexedInstanced(static_cast<UINT>(_indices.size()), 1, 0, 0, 0);
	}
	else
	{
		// [수정] 바이트 크기가 아닌, 정점 개수를 사용합니다.
		commandList->DrawInstanced(_vertexCount, 1, 0, 0);
	}
}

BoundingOrientedBox Mesh::CreateOOBB(XMFLOAT3 min, XMFLOAT3 max)
{
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

DebugCollisionBox::DebugCollisionBox(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, XMFLOAT4 color)
{
	// 정점 8개의 위치를 설정, 디버그용으로 일반 노멀/탄젠트/텍스처 좌표를 사용합니다.
	std::vector<IlluminatedVertex> temp_vertices;
	// For a debug box, we use placeholder normals, texCoords, and tangents.
	XMFLOAT3 default_normal = XMFLOAT3(0.0f, 1.0f, 0.0f); // Up direction
	XMFLOAT2 default_texCoord = XMFLOAT2(0.0f, 0.0f);
	XMFLOAT3 default_tangent = XMFLOAT3(1.0f, 0.0f, 0.0f); // X-axis direction

	temp_vertices.push_back(IlluminatedVertex(XMFLOAT3(-0.5f, -0.5f, -0.5f), default_normal, default_texCoord, default_tangent));
	temp_vertices.push_back(IlluminatedVertex(XMFLOAT3(-0.5f, 0.5f, -0.5f), default_normal, default_texCoord, default_tangent));
	temp_vertices.push_back(IlluminatedVertex(XMFLOAT3(0.5f, 0.5f, -0.5f), default_normal, default_texCoord, default_tangent));
	temp_vertices.push_back(IlluminatedVertex(XMFLOAT3(0.5f, -0.5f, -0.5f), default_normal, default_texCoord, default_tangent));
	temp_vertices.push_back(IlluminatedVertex(XMFLOAT3(-0.5f, -0.5f, 0.5f), default_normal, default_texCoord, default_tangent));
	temp_vertices.push_back(IlluminatedVertex(XMFLOAT3(-0.5f, 0.5f, 0.5f), default_normal, default_texCoord, default_tangent));
	temp_vertices.push_back(IlluminatedVertex(XMFLOAT3(0.5f, 0.5f, 0.5f), default_normal, default_texCoord, default_tangent));
	temp_vertices.push_back(IlluminatedVertex(XMFLOAT3(0.5f, -0.5f, 0.5f), default_normal, default_texCoord, default_tangent));

	set_vertex_data_buffer(temp_vertices);

	// 인덱스 데이터 생성
	_indices.resize(24);
	_indices[0] = 0; _indices[1] = 1; _indices[2] = 1; _indices[3] = 2;
	_indices[4] = 2; _indices[5] = 3; _indices[6] = 3; _indices[7] = 0;
	_indices[8] = 4; _indices[9] = 5; _indices[10] = 5; _indices[11] = 6;
	_indices[12] = 6; _indices[13] = 7; _indices[14] = 7; _indices[15] = 4;
	_indices[16] = 0; _indices[17] = 4; _indices[18] = 1; _indices[19] = 5;
	_indices[20] = 2; _indices[21] = 6; _indices[22] = 3; _indices[23] = 7;

	_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
}

DebugCollisionBox::~DebugCollisionBox()
{
}

DebugWireframeMesh::DebugWireframeMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, XMFLOAT4 color)
{
	// 1. 입력받은 Vertex 데이터를 IlluminatedVertex 데이터로 변환하여 임시 저장
	std::vector<IlluminatedVertex> temp_vertices;
	temp_vertices.reserve(vertices.size());
	XMFLOAT3 default_normal = XMFLOAT3(0.0f, 1.0f, 0.0f); // Default normal
	XMFLOAT2 default_texCoord = XMFLOAT2(0.0f, 0.0f);   // Default texCoord
	XMFLOAT3 default_tangent = XMFLOAT3(1.0f, 0.0f, 0.0f); // Default tangent

	for (const auto& v : vertices)
	{
		temp_vertices.emplace_back(IlluminatedVertex(v._position, default_normal, default_texCoord, default_tangent));
	}

	set_vertex_data_buffer(temp_vertices);

	// 인덱스 데이터 변환 (삼각형 -> 라인 리스트)
	_indices.reserve(indices.size() * 2);
	for (size_t i = 0; i < indices.size(); i += 3)
	{
		UINT i0 = indices[i];
		UINT i1 = indices[i + 1];
		UINT i2 = indices[i + 2];

		_indices.push_back(i0); _indices.push_back(i1);
		_indices.push_back(i1); _indices.push_back(i2);
		_indices.push_back(i2); _indices.push_back(i0);
	}

	_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
}

DebugWireframeMesh::~DebugWireframeMesh() {}
