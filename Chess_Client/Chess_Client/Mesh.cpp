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
	for (CIlluminatedVertex& vertex : m_Vertexvec)
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

// (수정) 위치뿐만 아니라 빛 계산 때 필요한 법선 벡터 추가 [PONG] 
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

	// 원래는 바로 넣었던 걸 수정하여, 임시로 위치, 법선을 저장할 벡터를 사용합니다.
	std::vector<XMFLOAT3> temp_positions; // 임시로 위치를 저장할 벡터
	std::vector<XMFLOAT3> temp_normals; // 임시로 법선을 저장할 벡터
	std::vector<UINT> position_indices, normal_indices; // 위치 인덱스 저장용 & 법선 인덱스 저장용


	while (std::getline(in, Line))
	{
		std::istringstream iss(Line);

		iss >> type;

		if (type == "v") {
			// 수정 (위치벡터에 위치를 저장)
			XMFLOAT3 pos;
			iss >> pos.x >> pos.y >> pos.z;
			temp_positions.emplace_back(pos);
			
		}
		else if (type == "vn") {
			// 수정 (법선벡터에 법선을 저장)
			XMFLOAT3 normal;
			iss >> normal.x >> normal.y >> normal.z;
			temp_normals.emplace_back(normal);
		}
		else if (type == "f")
		{
			std::string face_chunk;
			// "f" 뒤에 나오는 세 개의 "v//vn" 덩어리를 각각 처리합니다.
			for (int i = 0; i < 3; ++i)
			{
				iss >> face_chunk; // 예: "1//1"

				std::stringstream face_ss(face_chunk);
				std::string part;

				// 첫 번째 '/' 전까지 읽어 위치 인덱스로 저장
				std::getline(face_ss, part, '/');
				position_indices.push_back(std::stoul(part));

				// 두 번째 '/' 전까지 읽음 (vt 인덱스, 현재는 비어있음)
				std::getline(face_ss, part, '/');

				// 나머지를 읽어 법선 인덱스로 저장
				std::getline(face_ss, part);
				normal_indices.push_back(std::stoul(part));
			}
		}
	}

	m_Vertexvec.clear();
	m_Indexvec.clear();

	// 이 코드는 (위치, 법선)의 고유한 조합을 찾아내어, 중복되는 정점 생성을 막고 메모리를 최적화
	// 법선 벡터가 고유한 solid 객체를 받아올 때는 아래코드가 있으면 좋음
	// 투명, 반투명, 구멍이 뚫려있는 물체 등을 사용할 때는 다른 형식으로 생각해봐야할듯 <- 왜냐하면 이거는 위치가 같더라도 서로다른 법선벡터가 필요할 거니까

	// Key: (위치 인덱스, 법선 인덱스)의 한 쌍
	// Value: 우리가 새로 만드는 최종 정점 목록에서의 인덱스 번호
	std::map<std::pair<UINT, UINT>, UINT> vertex_map;

	// .obj 파일의 f 라인에서 읽어온 모든 면(face)의 인덱스 정보(인덱스 3개 => 삼각형 1개)를 순회
	for (size_t i = 0; i < position_indices.size(); ++i)
	{
		// .obj 파일의 인덱스는 1부터 시작하지만, C++ 벡터의 인덱스는 0부터 시작하므로 1을 빼서 실제 배열의 인덱스로 변환
		UINT pos_idx = position_indices[i] - 1;
		UINT norm_idx = normal_indices[i] - 1;

		if (pos_idx >= temp_positions.size() || norm_idx >= temp_normals.size())
		{
			__debugbreak();
		}

		// 현재 처리 중인 정점의 '(위치 인덱스, 법선 인덱스)' 조합을 Key로
		std::pair<UINT, UINT> vertex_key = { pos_idx, norm_idx };

		// map에서 이 조합을 이전에 본 적이 있는지 찾기
		auto it = vertex_map.find(vertex_key);

		// 위 find에서 end가 나왔을 경우엔?(map안에 없다는 거)
		if (it == vertex_map.end())
		{
			// 임시 저장소에 있던 실제 위치와 법선 데이터로 새 정점을 생성
			CIlluminatedVertex new_vertex(temp_positions[pos_idx], temp_normals[norm_idx]);

			// 이 새 정점을 최종 정점 목록에 추가
			m_Vertexvec.emplace_back(new_vertex);

			// 방금 추가한 정점의 인덱스 번호를 구하고
			UINT new_index = static_cast<UINT>(m_Vertexvec.size() - 1);

			// 새로 만든 정점의 인덱스를 최종 인덱스 목록에 추가
			m_Indexvec.emplace_back(new_index);

			// map에 (위치/법선 조합, 새로 부여된 인덱스)를 기록 <- 이러면 다음에 똑같은게 나오면 중복체크가 되서 넘어가겠지 
			vertex_map[vertex_key] = new_index;
		}
		else
		{
			// 정점을 또 만들지 않고, map에서 찾은 기존 인덱스 번호를 최종 인덱스 목록에 추가하여 재사용
			m_Indexvec.emplace_back(it->second);
		}
	}

	// (수정) CIlluminatedVertex로 변경
	auto [min_x, max_x] =
		std::minmax_element(m_Vertexvec.begin(), m_Vertexvec.end(), 
			[](const CIlluminatedVertex& a, const CIlluminatedVertex& b)
			{ 
				return a.m_xmf3Position.x < b.m_xmf3Position.x; 
			});
	auto [min_y, max_y] =
		std::minmax_element(m_Vertexvec.begin(), m_Vertexvec.end(), 
			[](const CIlluminatedVertex& a, const CIlluminatedVertex& b)
			{ 
				return a.m_xmf3Position.y < b.m_xmf3Position.y; 
			});
	auto [min_z, max_z] =
		std::minmax_element(m_Vertexvec.begin(), m_Vertexvec.end(), 
			[](const CIlluminatedVertex& a, const CIlluminatedVertex& b)
			{ 
				return a.m_xmf3Position.z < b.m_xmf3Position.z; 
			});

	m_nStride = sizeof(CIlluminatedVertex);
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