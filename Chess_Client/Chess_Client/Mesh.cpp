#include "stdafx.h"
#include "Mesh.h"
#include "Scene.h"
#include <algorithm>

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

Mesh::Mesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	m_Left = 0.0f;
	m_Top = 0.0f;
	m_Right = 0.0f;
	m_Bottom = 0.0f;
	m_Front = 0.0f;
	m_Back = 0.0f;
}
Mesh::~Mesh()
{
	if (m_pd3dVertexBuffer) m_pd3dVertexBuffer->Release();
	if (m_pd3dVertexUploadBuffer) m_pd3dVertexUploadBuffer->Release();

	if (m_pd3dIndexBuffer) m_pd3dIndexBuffer->Release();
	if (m_pd3dIndexUploadBuffer) m_pd3dIndexUploadBuffer->Release();
}

void Mesh::ReleaseUploadBuffers()
{
	//정점 버퍼를 위한 업로드 버퍼를 소멸시킨다. 
	if (m_pd3dVertexUploadBuffer) m_pd3dVertexUploadBuffer->Release();
	m_pd3dVertexUploadBuffer = NULL;

	if (m_pd3dIndexUploadBuffer) m_pd3dIndexUploadBuffer->Release();
	m_pd3dIndexUploadBuffer = NULL;

};



void Mesh::UpdateVertices(ID3D12GraphicsCommandList* pd3dCommandList)
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

void Mesh::ChangeColor(ID3D12GraphicsCommandList* pd3dCommandList, float r, float g, float b, float a)
{
	for (IlluminatedVertex& vertex : m_Vertexvec)
	{
		vertex.m_xmf4Diffuse.x = r;
		vertex.m_xmf4Diffuse.y = g;
		vertex.m_xmf4Diffuse.z = b;
		vertex.m_xmf4Diffuse.w = a;
	}
	UpdateVertices(pd3dCommandList);
}


void Mesh::Render(ID3D12GraphicsCommandList* pd3dCommandList)
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

BOOL Mesh::RayIntersectionByTriangle(XMVECTOR& xmRayOrigin, XMVECTOR& xmRayDirection, XMVECTOR v0, XMVECTOR v1, XMVECTOR v2, float* pfNearHitDistance)
{
	float fHitDistance;
	BOOL bIntersected = TriangleTests::Intersects(xmRayOrigin, xmRayDirection, v0, v1, v2, fHitDistance);
	if (bIntersected && (fHitDistance < *pfNearHitDistance)) *pfNearHitDistance = fHitDistance;

	return(bIntersected);
}

int Mesh::CheckRayIntersection(XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection, float* pfNearHitDistance)
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
ReadObjMesh::ReadObjMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::string str)
{
	std::ifstream in{ str };
	if (!in) {
		return;
	}

	std::string Line{};
	std::string type{};
	std::string currentMtlName = ""; // mtl 파일 이름 추가

	int FaceNum{}; // 1,2,3 반복

	// 원래는 바로 넣었던 걸 수정하여, 임시로 위치, 법선을 저장할 벡터를 사용합니다.
	std::vector<XMFLOAT3> temp_positions; // 임시로 위치를 저장할 벡터
	std::vector<XMFLOAT3> temp_normals; // 임시로 법선을 저장할 벡터
	std::vector<XMFLOAT2> temp_texcoords; // 임시로 텍스처 좌표를 저장할 벡터
	std::vector<UINT> position_indices, normal_indices, texcoord_indices; // 위치 인덱스 저장용 & 법선 인덱스 저장용 & 텍스처 인덱스 저장용
	std::vector<std::string> object_names; // 오브젝트 이름 저장용


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
		else if (type == "vt") {
			XMFLOAT2 tex;
			iss >> tex.x >> tex.y;
			temp_texcoords.emplace_back(tex);
		}
		else if (type == "o") {
			std::string objName;
			iss >> objName;
			object_names.push_back(objName);
		}
		// .mtl 파일을 로드하라는 지시어(mtllib) 처리
		else if (type == "mtllib") {
		    std::string mtlFileName;
			iss >> mtlFileName; // .obj 파일 경로를 기준으로 .mtl 파일 로드 함수 호출
			LoadMtlFile(str, mtlFileName);
		} 
		// 사용할 재질을 지정하는 지시어(usemtl) 처리
			else if (type == "usemtl") {
			// 현재 사용할 재질의 이름을 저장
			 iss >> currentMtlName;
		}
		else if (type == "f")
		{
			std::string face_chunk;
			// "f" 뒤에 나오는 세 개의 "v//vn" 덩어리를 각각 처리
			for (int i = 0; i < 3; ++i)
			{
				iss >> face_chunk; // 예: "1//1"

				std::stringstream face_ss(face_chunk);
				std::string part;

				// 위치 인덱스 '/' 전까지 읽어 위치 인덱스로 저장
				std::getline(face_ss, part, '/');
				position_indices.push_back(std::stoul(part));

				// 텍스처 인덱스'/' 전까지 읽음 (비어 있을 수 있음)
				std::getline(face_ss, part, '/');
				texcoord_indices.push_back(part.empty() ? 0 : std::stoul(part));

				// 법선 인덱스 (비어있을 수 있음)
				if (std::getline(face_ss, part))
					normal_indices.push_back(part.empty() ? 0 : std::stoul(part));
				else
					normal_indices.push_back(0); // 없으면 0
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
		UINT tex_idx = texcoord_indices[i] - 1;
		if (tex_idx >= temp_texcoords.size() || tex_idx < 0)
			tex_idx = 0; // 또는 XMFLOAT2(0,0) 등 기본값 사용

		if (pos_idx >= temp_positions.size() || norm_idx >= temp_normals.size())
		{
			__debugbreak();
		}

		// 위치/법선/텍스처 좌표가 없을 때 기본값 처리
		XMFLOAT3 position = (pos_idx < temp_positions.size() && pos_idx >= 0) ? temp_positions[pos_idx] : XMFLOAT3(0, 0, 0);
		XMFLOAT3 normal = (norm_idx < temp_normals.size() && norm_idx >= 0) ? temp_normals[norm_idx] : XMFLOAT3(0, 0, 1);
		XMFLOAT2 texcoord = (tex_idx < temp_texcoords.size() && tex_idx >= 0) ? temp_texcoords[tex_idx] : XMFLOAT2(0, 0);

		// 현재 처리 중인 정점의 '(위치 인덱스, 법선 인덱스)' 조합을 Key로
		std::pair<UINT, UINT> vertex_key = { pos_idx, norm_idx };

		// map에서 이 조합을 이전에 본 적이 있는지 찾기
		auto it = vertex_map.find(vertex_key);

		// 위 find에서 end가 나왔을 경우엔?(map안에 없다는 거)
		if (it == vertex_map.end())
		{
			// 사용할 색상을 저장할 변수 (기본값은 랜덤 색상)
			XMFLOAT4 color = RANDOM_COLOR;
			// 현재 재질 이름(currentMtlName)이 재질 맵(m_mapMaterials)에 있는지 확인
			if (m_mapMaterials.count(currentMtlName))
			{
				// 재질 맵에 있다면 해당 재질의 Kd(확산광) 값을 색상으로 사용
				color = m_mapMaterials[currentMtlName].Kd;
			}

			// 임시 저장소에 있던 실제 위치와 법선 데이터로 새 정점을 생성 + color 추가
			IlluminatedVertex new_vertex(position, normal, texcoord, color);

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

	// (수정) IlluminatedVertex로 변경
	auto [min_x, max_x] =
		std::minmax_element(m_Vertexvec.begin(), m_Vertexvec.end(), 
			[](const IlluminatedVertex& a, const IlluminatedVertex& b)
			{ 
				return a.m_xmf3Position.x < b.m_xmf3Position.x; 
			});
	auto [min_y, max_y] =
		std::minmax_element(m_Vertexvec.begin(), m_Vertexvec.end(), 
			[](const IlluminatedVertex& a, const IlluminatedVertex& b)
			{ 
				return a.m_xmf3Position.y < b.m_xmf3Position.y; 
			});
	auto [min_z, max_z] =
		std::minmax_element(m_Vertexvec.begin(), m_Vertexvec.end(), 
			[](const IlluminatedVertex& a, const IlluminatedVertex& b)
			{ 
				return a.m_xmf3Position.z < b.m_xmf3Position.z; 
			});

	m_nStride = sizeof(IlluminatedVertex);
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


ReadObjMesh::~ReadObjMesh()
{

}

// .mtl 파일을 읽어 재질 정보를 파싱하고 m_mapMaterials 맵에 저장하는 함수
void ReadObjMesh::LoadMtlFile(const std::string& objFilePath, const std::string& mtlFileName)
{
	// .obj 파일의 경로를 기준으로 .mtl 파일의 전체 경로를 생성
	std::string mtlFilePath = objFilePath.substr(0, objFilePath.find_last_of("/\\")) + "/" + mtlFileName;
	std::ifstream in(mtlFilePath);
	if (!in) {
		return; // 파일 열기 실패
	}

	std::string line;
	std::string currentMtlName = ""; // 현재 파싱 중인 재질의 이름

	// .mtl 파일을 한 줄씩 읽기
	while (std::getline(in, line))
	{
		std::istringstream iss(line);
		std::string type;
		iss >> type;

		// 새로운 재질 정의 시작
		if (type == "newmtl")
		{
			iss >> currentMtlName;
			m_mapMaterials[currentMtlName] = Material();
			m_mapMaterials[currentMtlName].name = currentMtlName;
		}
		// 확산광(diffuse) 색상 값 (r, g, b)
		else if (type == "Kd")
		{
			if (!currentMtlName.empty())
			{
				iss >> m_mapMaterials[currentMtlName].Kd.x >> m_mapMaterials[currentMtlName].Kd.y >> m_mapMaterials[currentMtlName].Kd.z;
				m_mapMaterials[currentMtlName].Kd.w = 1.0f; // Alpha는 1.0으로 설정
			}
		}
		// 주변광(ambient) 색상 값
		else if (type == "Ka")
		{
			if (!currentMtlName.empty())
			{
				iss >> m_mapMaterials[currentMtlName].Ka.x >> m_mapMaterials[currentMtlName].Ka.y >> m_mapMaterials[currentMtlName].Ka.z;
				m_mapMaterials[currentMtlName].Ka.w = 1.0f;
			}
		}
		// 반사광(specular) 색상 값
		else if (type == "Ks")
		{
			if (!currentMtlName.empty())
			{
				iss >> m_mapMaterials[currentMtlName].Ks.x >> m_mapMaterials[currentMtlName].Ks.y >> m_mapMaterials[currentMtlName].Ks.z;
				m_mapMaterials[currentMtlName].Ks.w = 1.0f;
			}
		}
		// 반사광 지수(shininess)
		else if (type == "Ns")
		{
			if (!currentMtlName.empty())
			{
				iss >> m_mapMaterials[currentMtlName].Ns;
			}
		}
	}
}




// ReadGlbMesh 소멸자: 생성된 모든 Primitive와 업로드 버퍼를 정리합니다.
ReadGlbMesh::~ReadGlbMesh()
{
	m_primitives.clear(); // unique_ptr 벡터가 자동으로 각 Primitive 소멸자 호출
	for (auto& buffer : m_vUploadBuffers) {
		if (buffer) buffer->Release();
	}
	m_vUploadBuffers.clear();
}

// 오버라이드된 ReleaseUploadBuffers 함수
void ReadGlbMesh::ReleaseUploadBuffers()
{
	// ReadGlbMesh는 로딩이 끝나면 업로드 버퍼를 모두 해제합니다.
	for (auto& buffer : m_vUploadBuffers) {
		if (buffer) buffer->Release();
	}
	m_vUploadBuffers.clear();

	// 베이스 클래스의 업로드 버퍼도 혹시 모르니 호출해줄 수 있습니다.
	Mesh::ReleaseUploadBuffers();
}

D3D12_GPU_DESCRIPTOR_HANDLE ReadGlbMesh::GetSrvGpuHandle(UINT nPrimitive) const
{
	// 메시가 프리미티브를 가지고 있고, 요청된 인덱스가 유효한지 확인합니다.
	if (m_primitives.empty() || nPrimitive >= m_primitives.size())
	{
		return { 0 }; // 유효하지 않으면 비어있는 핸들 반환
	}

	// 지정된 프리미티브(기본값은 0번째)의 SRV GPU 핸들을 반환합니다.
	return m_primitives[nPrimitive]->m_d3dGpuSrvHandle;
}

D3D12_GPU_VIRTUAL_ADDRESS ReadGlbMesh::GetBoneTransformsBufferAddress() const
{
	if (m_pd3dcbBoneTransforms)
	{
		return m_pd3dcbBoneTransforms->GetGPUVirtualAddress();
	}
	return 0;
}

std::tuple<std::vector<unsigned char>, UINT, UINT> ReadGlbMesh::LoadImageFromGLB(const json& j, const std::vector<char>& binaryData, int textureIndex)
{
	if (!j.contains("textures") || textureIndex >= j["textures"].size()) return {};
	const auto& tex = j["textures"][textureIndex];

	if (!tex.contains("source")) return {};
	int imageIndex = tex["source"];

	if (!j.contains("images") || imageIndex >= j["images"].size()) return {};
	const auto& img = j["images"][imageIndex];

	if (!img.contains("bufferView")) return {};
	int bufferViewIndex = img["bufferView"];

	if (!j.contains("bufferViews") || bufferViewIndex >= j["bufferViews"].size()) return {};
	const auto& bv = j["bufferViews"][bufferViewIndex];

	size_t byteOffset = bv["byteOffset"];
	size_t byteLength = bv["byteLength"];

	const unsigned char* pImageData = reinterpret_cast<const unsigned char*>(binaryData.data() + byteOffset);

	// WIC를 사용하여 메모리상의 이미지 데이터 디코딩
	HRESULT hr;
	IWICImagingFactory* pFactory = nullptr;
	IWICStream* pStream = nullptr;
	IWICBitmapDecoder* pDecoder = nullptr;
	IWICBitmapFrameDecode* pFrame = nullptr;
	IWICFormatConverter* pConverter = nullptr;

	CoInitialize(NULL);
	hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
	if (FAILED(hr)) { CoUninitialize(); return {}; }

	hr = pFactory->CreateStream(&pStream);
	if (SUCCEEDED(hr)) hr = pStream->InitializeFromMemory(const_cast<unsigned char*>(pImageData), byteLength);
	if (SUCCEEDED(hr)) hr = pFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder);
	if (SUCCEEDED(hr)) hr = pDecoder->GetFrame(0, &pFrame);

	UINT width, height;
	if (SUCCEEDED(hr)) hr = pFrame->GetSize(&width, &height);

	if (SUCCEEDED(hr)) hr = pFactory->CreateFormatConverter(&pConverter);
	if (SUCCEEDED(hr)) hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeMedianCut);

	std::vector<unsigned char> pixels(width * height * 4);
	if (SUCCEEDED(hr)) hr = pConverter->CopyPixels(NULL, width * 4, pixels.size(), pixels.data());

	if (pConverter) pConverter->Release();
	if (pFrame) pFrame->Release();
	if (pDecoder) pDecoder->Release();
	if (pStream) pStream->Release();
	if (pFactory) pFactory->Release();
	CoUninitialize();

	if (FAILED(hr)) return {};

	return { std::move(pixels), width, height };
}

// 오버라이드된 Render 함수: ReadGlbMesh만의 렌더링 로직
void ReadGlbMesh::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
	pd3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (const auto& primitive : m_primitives)
	{
		// 이 Primitive가 텍스처를 가지고 있다면, GpuHandle가 0이 아니라면
		if (primitive->m_pTexture && primitive->m_d3dGpuSrvHandle.ptr != 0)
		{
			// 해당 텍스처의 SRV를 루트 테이블에 바인딩합니다.
			// 5번 파라미터가 텍스처 테이블
			pd3dCommandList->SetGraphicsRootDescriptorTable(5, primitive->m_d3dGpuSrvHandle);
		}

		pd3dCommandList->IASetVertexBuffers(0, 1, &primitive->m_d3dVertexBufferView);
		pd3dCommandList->IASetIndexBuffer(&primitive->m_d3dIndexBufferView);
		pd3dCommandList->DrawIndexedInstanced(primitive->m_nIndices, 1, 0, 0, 0);
	}
}

ReadGlbMesh::ReadGlbMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::string str, Scene* pScene)
{
	// --- 1단계: 파일 읽기 및 청크 분리 ---
	std::vector<char> fileData;
	std::ifstream file(str, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		std::cerr << "Error: Failed to open " << str << std::endl;
		return;
	}
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	fileData.resize(size);
	if (!file.read(fileData.data(), size)) {
		std::cerr << "Error: Failed to read file content." << std::endl;
		return;
	}
	file.close();

	std::string jsonString;
	std::vector<char> binaryData;
	char* pData = fileData.data();

	uint32_t magic = *reinterpret_cast<uint32_t*>(pData); pData += 4;
	uint32_t version = *reinterpret_cast<uint32_t*>(pData); pData += 4;
	uint32_t length = *reinterpret_cast<uint32_t*>(pData); pData += 4;

	if (magic != 0x46546C67) { // "glTF"
		std::cerr << "Error: Not a valid GLB file." << std::endl;
		return;
	}

	while (pData < fileData.data() + length) {
		uint32_t chunkLength = *reinterpret_cast<uint32_t*>(pData); pData += 4;
		uint32_t chunkType = *reinterpret_cast<uint32_t*>(pData); pData += 4;
		if (chunkType == 0x4E4F534A) { // "JSON"
			jsonString.assign(pData, chunkLength);
		}
		else if (chunkType == 0x004E4942) { // "BIN"
			binaryData.assign(pData, pData + chunkLength);
		}
		pData += chunkLength;
	}

	try
	{
		auto j = json::parse(jsonString);

		// --- 2단계: 노드 계층 구조 파싱 ---
		if (j.contains("nodes")) {
			const auto& nodes = j["nodes"];
			m_Nodes.resize(nodes.size());
			for (size_t i = 0; i < nodes.size(); ++i) {
				const auto& nodeJson = nodes[i];
				Node& currentNode = m_Nodes[i];
				if (nodeJson.contains("name")) currentNode.name = nodeJson["name"];
				if (nodeJson.contains("translation")) {
					currentNode.translation.x = nodeJson["translation"][0];
					currentNode.translation.y = nodeJson["translation"][1];
					currentNode.translation.z = nodeJson["translation"][2];

					// [좌표계 변환] Translation의 Z축 반전
					currentNode.translation.z *= -1.0f;
				}
				if (nodeJson.contains("rotation")) {
					currentNode.rotation.x = nodeJson["rotation"][0];
					currentNode.rotation.y = nodeJson["rotation"][1];
					currentNode.rotation.z = nodeJson["rotation"][2];
					currentNode.rotation.w = nodeJson["rotation"][3];

					// [좌표계 변환] Quaternion의 X, Y축 반전
					currentNode.rotation.x *= -1.0f;
					currentNode.rotation.y *= -1.0f;
				}
				if (nodeJson.contains("scale")) {
					currentNode.scale.x = nodeJson["scale"][0];
					currentNode.scale.y = nodeJson["scale"][1];
					currentNode.scale.z = nodeJson["scale"][2];
				}
				if (nodeJson.contains("children")) {
					for (const auto& childIndex : nodeJson["children"]) {
						currentNode.childrenIndices.push_back(childIndex);
					}
				}
				if (nodeJson.contains("mesh")) currentNode.meshIndex = nodeJson["mesh"];
				if (nodeJson.contains("skin")) currentNode.skinIndex = nodeJson["skin"];
			}
			for (size_t i = 0; i < m_Nodes.size(); ++i) {
				for (int childIndex : m_Nodes[i].childrenIndices) {
					if (childIndex >= 0 && childIndex < m_Nodes.size()) {
						m_Nodes[childIndex].parentIndex = i;
					}
				}
			}
		}

		// --- 3단계: 모든 메시 및 텍스처 파싱 ---
		if (!j.contains("meshes") || j["meshes"].empty()) {
			std::cout << "Warning: No meshes found in the glTF file." << std::endl;
			return;
		}

		XMFLOAT3 modelMin(FLT_MAX, FLT_MAX, FLT_MAX);
		XMFLOAT3 modelMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (const auto& mesh : j["meshes"])
		{
			for (const auto& primitiveJson : mesh["primitives"])
			{
				std::vector<SkinnedVertex> vertices;
				std::vector<UINT> indices;

				int posAccessorIndex = primitiveJson["attributes"]["POSITION"];
				int indicesAccessorIndex = primitiveJson["indices"];
				int normalAccessorIndex = primitiveJson.value("/attributes/NORMAL"_json_pointer, -1);
				int texCoordAccessorIndex = primitiveJson.value("/attributes/TEXCOORD_0"_json_pointer, -1);
				int jointAccessorIndex = primitiveJson.value("/attributes/JOINTS_0"_json_pointer, -1);
				int weightAccessorIndex = primitiveJson.value("/attributes/WEIGHTS_0"_json_pointer, -1);

				if (posAccessorIndex == -1 || indicesAccessorIndex == -1) continue;

				auto [positions, posCount] = getData<XMFLOAT3>(j, binaryData, posAccessorIndex);
				auto [normals, normCount] = (normalAccessorIndex != -1) ? getData<XMFLOAT3>(j, binaryData, normalAccessorIndex) : std::pair<XMFLOAT3*, size_t>(nullptr, 0);
				auto [texCoords, texCount] = (texCoordAccessorIndex != -1) ? getData<XMFLOAT2>(j, binaryData, texCoordAccessorIndex) : std::pair<XMFLOAT2*, size_t>(nullptr, 0);
				auto [weights, weightCount] = (weightAccessorIndex != -1) ? getData<XMFLOAT4>(j, binaryData, weightAccessorIndex) : std::pair<XMFLOAT4*, size_t>(nullptr, 0);
				struct JointType { uint16_t j[4]; };
				auto [joints, jointCount] = (jointAccessorIndex != -1) ? getData<JointType>(j, binaryData, jointAccessorIndex) : std::pair<JointType*, size_t>(nullptr, 0);

				vertices.resize(posCount);
				for (size_t i = 0; i < posCount; ++i) {
					// [좌표계 변환] 정점 위치의 Z축 반전
					vertices[i].m_xmf3Position = positions[i];
					vertices[i].m_xmf3Position.z *= -1.0f;

					// [좌표계 변환] 법선 벡터의 Z축 반전
					if (normals) {
						vertices[i].m_xmf3Normal = normals[i];
						vertices[i].m_xmf3Normal.z *= -1.0f;
					}

					if (texCoords) vertices[i].m_xmf2TexCoord = texCoords[i];
					if (joints) vertices[i].m_xmf4BoneIndices = XMFLOAT4((float)joints[i].j[0], (float)joints[i].j[1], (float)joints[i].j[2], (float)joints[i].j[3]);
					if (weights) vertices[i].m_xmf4BoneWeights = weights[i];

					// Bounding Box 계산 시에는 변환된 좌표를 사용
					XMFLOAT3 transformedPos = vertices[i].m_xmf3Position;
					modelMin.x = min(modelMin.x, transformedPos.x);
					modelMin.y = min(modelMin.y, transformedPos.y);
					modelMin.z = min(modelMin.z, transformedPos.z);
					modelMax.x = max(modelMax.x, transformedPos.x);
					modelMax.y = max(modelMax.y, transformedPos.y);
					modelMax.z = max(modelMax.z, transformedPos.z);
				}

				const auto& indexAccessor = j["accessors"][indicesAccessorIndex];
				size_t indicesCount = indexAccessor["count"];
				indices.resize(indicesCount);
				if (indexAccessor["componentType"] == 5123) { // uint16_t
					auto [indices_u16, count] = getData<uint16_t>(j, binaryData, indicesAccessorIndex);
					for (size_t i = 0; i < count; ++i) indices[i] = indices_u16[i];
				}
				else if (indexAccessor["componentType"] == 5125) { // uint32_t
					auto [indices_u32, count] = getData<uint32_t>(j, binaryData, indicesAccessorIndex);
					indices.assign(indices_u32, indices_u32 + count);
				}

				// [좌표계 변환] 인덱스 순서(Winding Order) 뒤집기
				for (size_t i = 0; i < indices.size(); i += 3) {
					std::swap(indices[i + 1], indices[i + 2]);
				}


				// --- 이하 코드는 동일 ---
				auto newPrimitive = std::make_unique<MeshPrimitive>();
				ID3D12Resource* pVertexUploadBuffer = nullptr;
				ID3D12Resource* pIndexUploadBuffer = nullptr;

				newPrimitive->m_nIndices = indices.size();

				newPrimitive->m_pd3dVertexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, vertices.data(), sizeof(SkinnedVertex) * vertices.size(), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &pVertexUploadBuffer);
				newPrimitive->m_d3dVertexBufferView.BufferLocation = newPrimitive->m_pd3dVertexBuffer->GetGPUVirtualAddress();
				newPrimitive->m_d3dVertexBufferView.StrideInBytes = sizeof(SkinnedVertex);
				newPrimitive->m_d3dVertexBufferView.SizeInBytes = sizeof(SkinnedVertex) * vertices.size();

				newPrimitive->m_pd3dIndexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, indices.data(), sizeof(UINT) * indices.size(), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &pIndexUploadBuffer);
				newPrimitive->m_d3dIndexBufferView.BufferLocation = newPrimitive->m_pd3dIndexBuffer->GetGPUVirtualAddress();
				newPrimitive->m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
				newPrimitive->m_d3dIndexBufferView.SizeInBytes = sizeof(UINT) * indices.size();

				m_vUploadBuffers.push_back(pVertexUploadBuffer);
				m_vUploadBuffers.push_back(pIndexUploadBuffer);

				if (primitiveJson.contains("material")) {
					newPrimitive->m_nMaterialIndex = primitiveJson["material"];
					const auto& mat = j["materials"][newPrimitive->m_nMaterialIndex];
					if (mat.contains("pbrMetallicRoughness") && mat["pbrMetallicRoughness"].contains("baseColorTexture")) {
						int textureIndex = mat["pbrMetallicRoughness"]["baseColorTexture"]["index"];
						auto [pixels, width, height] = LoadImageFromGLB(j, binaryData, textureIndex);

						if (!pixels.empty()) {
							D3D12_RESOURCE_DESC textureDesc = {};
							textureDesc.MipLevels = 1;
							textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
							textureDesc.Width = width;
							textureDesc.Height = height;
							textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
							textureDesc.DepthOrArraySize = 1;
							textureDesc.SampleDesc.Count = 1;
							textureDesc.SampleDesc.Quality = 0;
							textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;


							D3D12_HEAP_PROPERTIES d3dHeapProperties = {};
							d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
							d3dHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
							d3dHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
							d3dHeapProperties.CreationNodeMask = 1;
							d3dHeapProperties.VisibleNodeMask = 1;

							pd3dDevice->CreateCommittedResource(&d3dHeapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&newPrimitive->m_pTexture));

							UINT64 uploadBufferSize;
							D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
							pd3dDevice->GetCopyableFootprints(&textureDesc, 0, 1, 0, &layout, nullptr, nullptr, &uploadBufferSize);

							ID3D12Resource* pTextureUploadHeap = nullptr;
							pTextureUploadHeap = ::CreateBufferResource(pd3dDevice, pd3dCommandList, nullptr, uploadBufferSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

							D3D12_SUBRESOURCE_DATA textureData = {};
							textureData.pData = pixels.data();
							textureData.RowPitch = width * 4;
							textureData.SlicePitch = textureData.RowPitch * height;


							void* pMappedData = nullptr;
							pTextureUploadHeap->Map(0, nullptr, &pMappedData);

							BYTE* pDest = (BYTE*)pMappedData;
							BYTE* pSrc = (BYTE*)textureData.pData;
							for (UINT i = 0; i < height; ++i)
							{
								memcpy(pDest, pSrc, textureData.RowPitch);
								pDest += layout.Footprint.RowPitch;
								pSrc += textureData.RowPitch;
							}

							pTextureUploadHeap->Unmap(0, nullptr);

							D3D12_TEXTURE_COPY_LOCATION destLocation = {};
							destLocation.pResource = newPrimitive->m_pTexture;
							destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
							destLocation.SubresourceIndex = 0;

							D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
							srcLocation.pResource = pTextureUploadHeap;
							srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
							srcLocation.PlacedFootprint = layout;

							pd3dCommandList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);

							D3D12_RESOURCE_BARRIER d3dResourceBarrier = {};
							d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
							d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
							d3dResourceBarrier.Transition.pResource = newPrimitive->m_pTexture;
							d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
							d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
							d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
							pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

							m_vUploadBuffers.push_back(pTextureUploadHeap);

							D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
							srvDesc.Format = textureDesc.Format;
							srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
							srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
							srvDesc.Texture2D.MipLevels = 1;

							D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle{};
							D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle{};

							pScene->AllocateNextSrvDescriptor(CpuHandle, GpuHandle);

							pd3dDevice->CreateShaderResourceView(newPrimitive->m_pTexture, &srvDesc, CpuHandle);

							newPrimitive->m_d3dGpuSrvHandle = GpuHandle;

							++_Textures;
						}
					}
				}
				m_primitives.push_back(std::move(newPrimitive));
			}
		}
		m_xmOOBB = CreateOOBB(modelMin, modelMax);
	}
	catch (json::parse_error& e) {
		std::cerr << "JSON parse error: " << e.what() << std::endl;
		return;
	}

	// --- [추가] 뼈 변환 행렬을 위한 상수 버퍼 생성 ---
	UINT nBoneCount = 128;
	UINT nBufferSize = sizeof(XMFLOAT4X4) * nBoneCount;

	m_pd3dcbBoneTransforms = ::CreateBufferResource(pd3dDevice, pd3dCommandList, nullptr, nBufferSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

}

ReadFbxMesh::ReadFbxMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::string str)
{
	// Assimp Importer 객체 생성
	Assimp::Importer importer;

	// 파일을 읽어 Assimp의 scene 객체로 변환
	// aiProcess_Triangulate: 모든 면을 삼각형으로 분할
	// aiProcess_FlipUVs: UV(텍스처 좌표)의 y축을 뒤집기
	// aiProcess_CalcTangentSpace: 탄젠트와 바이탄젠트 계산
	const aiScene* pScene = importer.ReadFile(str, aiProcess_Triangulate | aiProcess_FlipUVs |
		aiProcess_CalcTangentSpace);

	// 파일 읽기 실패 시 처리
	if (!pScene || pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !pScene->mRootNode)
	{
		// 에러 로그 출력
		OutputDebugStringA(importer.GetErrorString());
		return;
	}

	// 루트 노드부터 시작하여 모든 노드를 재귀적으로 처리
	ProcessNode(pScene->mRootNode, pScene, pd3dDevice, pd3dCommandList);

	auto [min_x, max_x] = std::minmax_element(m_Vertexvec.begin(), m_Vertexvec.end(),
		[](const IlluminatedVertex& a, const IlluminatedVertex& b) {
			return a.m_xmf3Position.x < b.m_xmf3Position.x;
		});

	auto [min_y, max_y] = std::minmax_element(m_Vertexvec.begin(), m_Vertexvec.end(),
		[](const IlluminatedVertex& a, const IlluminatedVertex& b) {
			return a.m_xmf3Position.y < b.m_xmf3Position.y;
		});
	
	auto [min_z, max_z] = std::minmax_element(m_Vertexvec.begin(), m_Vertexvec.end(),
		[](const IlluminatedVertex& a, const IlluminatedVertex& b) {
			return a.m_xmf3Position.z < b.m_xmf3Position.z;
		});

	XMFLOAT3 Min(min_x->m_xmf3Position.x, min_y->m_xmf3Position.y, min_z->m_xmf3Position.z);
	XMFLOAT3 Max(max_x->m_xmf3Position.x, max_y->m_xmf3Position.y, max_z->m_xmf3Position.z);

	m_xmOOBB = CreateOOBB(Min, Max);

	// --- 모든 메쉬 데이터 처리가 끝난 후, 최종 버퍼 생성 ---

	m_nStride = sizeof(IlluminatedVertex);
	m_nVertices = m_Vertexvec.size();
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// 정점 버퍼 생성
	m_pd3dVertexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_Vertexvec.data(),
		m_nStride * m_nVertices, D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&m_pd3dVertexUploadBuffer);

	// 인덱스 버퍼 생성
	m_nIndices = m_Indexvec.size();
	m_pd3dIndexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_Indexvec.data(),
		sizeof(UINT) * m_nIndices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER,
		&m_pd3dIndexUploadBuffer);

	// 정점 버퍼 뷰 설정
	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress();
	m_d3dVertexBufferView.StrideInBytes = m_nStride;
	m_d3dVertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	// 인덱스 버퍼 뷰 설정
	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = sizeof(UINT) * m_nIndices;
}

ReadFbxMesh::~ReadFbxMesh()
{
	// 소멸자
}

void ReadFbxMesh::ProcessNode(aiNode* node, const aiScene* scene, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	// 현재 노드에 포함된 모든 메쉬를 처리
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		ProcessMesh(mesh, scene, pd3dDevice, pd3dCommandList);
	}

	// 현재 노드의 모든 자식 노드에 대해 재귀적으로 이 함수를 호출
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		ProcessNode(node->mChildren[i], scene, pd3dDevice, pd3dCommandList);
	}
}

void ReadFbxMesh::ProcessMesh(aiMesh* mesh, const aiScene* scene, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	std::string meshNameStr = mesh->mName.C_Str();

	if (meshNameStr.rfind("UCX_", 0) == 0)
	{
		// --- Collision Mesh 처리 ---
		CollisionPrimitive primitive;

		// 제안 2: UCX_ 메시의 정점/인덱스 데이터 저장
		for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
		{
			// Assimp로부터 데이터를 읽은 직후, 값이 유효한지 확인합니다.
			const aiVector3D& vtx = mesh->mVertices[i];
			if (std::isnan(vtx.x) || std::isnan(vtx.y) || std::isnan(vtx.z) ||
				std::isinf(vtx.x) || std::isinf(vtx.y) || std::isinf(vtx.z))
			{
				char buffer[256];
				sprintf_s(buffer, "!!! CRITICAL ERROR: Invalid vertex data loaded from mesh: %s at index %u\n", meshNameStr.c_str(), i);
				OutputDebugStringA(buffer);
				continue; // 이 비정상적인 정점은 건너뜁니다.
			}

			Vertex v;
			v.m_xmf3Position.x = mesh->mVertices[i].x;
			v.m_xmf3Position.y = mesh->mVertices[i].y;
			v.m_xmf3Position.z = mesh->mVertices[i].z;
			primitive.vertices.push_back(v);
		}

		for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
		{
			aiFace face = mesh->mFaces[i];

			for (unsigned int j = 0; j < face.mNumIndices; ++j)
			{
				primitive.indices.push_back(face.mIndices[j]);
			}
		}

		// AABB 계산 
		DirectX::BoundingBox::CreateFromPoints(primitive.aabb, primitive.vertices.size(), &primitive.vertices[0].m_xmf3Position, sizeof(Vertex));
		// OBB 계산
		DirectX::BoundingOrientedBox::CreateFromPoints(primitive.oobb, primitive.vertices.size(), &primitive.vertices[0].m_xmf3Position, sizeof(Vertex));

		XMVECTOR quat = XMLoadFloat4(&primitive.oobb.Orientation);

		XMVECTOR lengthSq = XMVector4LengthSq(quat);

		float fLengthSq;
		XMStoreFloat(&fLengthSq, lengthSq);

		_collisionPrimitives.push_back(primitive);
	}
	else {
		// 현재 메쉬의 정점 정보를 임시로 담을 벡터
		std::vector<IlluminatedVertex> vertices;
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			// Render Mesh
			IlluminatedVertex vertex;

			// 위치 (Position)
			vertex.m_xmf3Position.x = mesh->mVertices[i].x;
			vertex.m_xmf3Position.y = mesh->mVertices[i].y;
			vertex.m_xmf3Position.z = mesh->mVertices[i].z;

			// 법선 (Normal)
			if (mesh->HasNormals())
			{
				vertex.m_xmf3Normal.x = mesh->mNormals[i].x;
				vertex.m_xmf3Normal.y = mesh->mNormals[i].y;
				vertex.m_xmf3Normal.z = mesh->mNormals[i].z;
			}

			// 텍스처 좌표 (Texture Coordinate)
			if (mesh->mTextureCoords[0]) // 텍스처 좌표 채널이 존재하는지 확인
			{
				vertex.m_xmf2Texcoord.x = mesh->mTextureCoords[0][i].x;
				vertex.m_xmf2Texcoord.y = mesh->mTextureCoords[0][i].y;
			}
			else
			{
				vertex.m_xmf2Texcoord = XMFLOAT2(0.0f, 0.0f);
			}

			// 재질 정보 처리
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			aiColor4D diffuseColor;
			aiString texturePath;

			// 확산 색상 가져오기 (없으면 흰색 기본값)
			if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor))
			{
				vertex.m_xmf4Diffuse = XMFLOAT4(diffuseColor.r, diffuseColor.g, diffuseColor.b, diffuseColor.a);
			}
			else
			{
				vertex.m_xmf4Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f); // 기본 흰색
			}

			// 확산 텍스처 경로 가져오기 (첫 번째 텍스처 채널)
			if (AI_SUCCESS == material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath))
			{
				// 텍스처 경로가 내장 텍스처를 가리키는지 확인 (예: "*0", "*1" 등)
				if (texturePath.C_Str()[0] == '*')
				{
					// 내장 텍스처인 경우: m_texturePath에 내장 텍스처 인덱스/이름 저장
					m_texturePath = texturePath.C_Str();
				}
				else
				{
					// 외부 텍스처인 경우: m_texturePath에 외부 파일 경로 저장
					m_texturePath = texturePath.C_Str();
				}
			}

			vertices.push_back(vertex);
		}

		// 현재 메쉬의 인덱스 정보를 임시로 담을 벡터
		// FBX의 모든 면(face)을 순회하며 인덱스를 가져옴
		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; j++)
			{
				// 전체 인덱스 벡터에 현재 메쉬의 인덱스를 추가
				// 이 때, 이미 추가된 정점 수를 더해줘서 전체 정점 배열에 맞는 인덱스가 되도록 함
				m_Indexvec.push_back(face.mIndices[j] + m_Vertexvec.size());
			}
		}

		// 임시 정점 벡터를 클래스의 전체 정점 벡터(m_Vertexvec)에 합침
		m_Vertexvec.insert(m_Vertexvec.end(), vertices.begin(), vertices.end());
	}
}

DebugCollisionBox::DebugCollisionBox(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, XMFLOAT4 color)
{
	// 정점 8개의 위치는 고정, 색상은 인자로 받은 color를 사용합니다.
	m_Vertexvec.resize(8);
	m_Vertexvec[0] = IlluminatedVertex(XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0, 0, 0), XMFLOAT2(0, 0), color);
	m_Vertexvec[1] = IlluminatedVertex(XMFLOAT3(-0.5f, 0.5f, -0.5f), XMFLOAT3(0, 0, 0), XMFLOAT2(0, 0), color);
	m_Vertexvec[2] = IlluminatedVertex(XMFLOAT3(0.5f, 0.5f, -0.5f), XMFLOAT3(0, 0, 0), XMFLOAT2(0, 0), color);
	m_Vertexvec[3] = IlluminatedVertex(XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(0, 0, 0), XMFLOAT2(0, 0), color);
	m_Vertexvec[4] = IlluminatedVertex(XMFLOAT3(-0.5f, -0.5f, 0.5f), XMFLOAT3(0, 0, 0), XMFLOAT2(0, 0), color);
	m_Vertexvec[5] = IlluminatedVertex(XMFLOAT3(-0.5f, 0.5f, 0.5f), XMFLOAT3(0, 0, 0), XMFLOAT2(0, 0), color);
	m_Vertexvec[6] = IlluminatedVertex(XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(0, 0, 0), XMFLOAT2(0, 0), color);
	m_Vertexvec[7] = IlluminatedVertex(XMFLOAT3(0.5f, -0.5f, 0.5f), XMFLOAT3(0, 0, 0), XMFLOAT2(0, 0), color);

	// 인덱스 데이터 (12개의 선)
	m_Indexvec.resize(24);
	m_Indexvec[0] = 0; m_Indexvec[1] = 1; m_Indexvec[2] = 1; m_Indexvec[3] = 2;
	m_Indexvec[4] = 2; m_Indexvec[5] = 3; m_Indexvec[6] = 3; m_Indexvec[7] = 0;
	m_Indexvec[8] = 4; m_Indexvec[9] = 5; m_Indexvec[10] = 5; m_Indexvec[11] = 6;
	m_Indexvec[12] = 6; m_Indexvec[13] = 7; m_Indexvec[14] = 7; m_Indexvec[15] = 4;
	m_Indexvec[16] = 0; m_Indexvec[17] = 4; m_Indexvec[18] = 1; m_Indexvec[19] = 5;
	m_Indexvec[20] = 2; m_Indexvec[21] = 6; m_Indexvec[22] = 3; m_Indexvec[23] = 7;

	// D3D 리소스 생성 (나머지 부분은 DebugWireframeMesh와 거의 동일)
	m_nStride = sizeof(IlluminatedVertex);
	m_nVertices = m_Vertexvec.size();
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;

	m_pd3dVertexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_Vertexvec.data(), m_nStride * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dVertexUploadBuffer);
	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress();
	m_d3dVertexBufferView.StrideInBytes = m_nStride;
	m_d3dVertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	m_nIndices = m_Indexvec.size();
	m_pd3dIndexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_Indexvec.data(), sizeof(UINT) * m_nIndices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_pd3dIndexUploadBuffer);
	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = sizeof(UINT) * m_nIndices;
}

DebugCollisionBox::~DebugCollisionBox()
{
}

DebugWireframeMesh::DebugWireframeMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, XMFLOAT4 color)
{
	// 렌더링에 사용할 정점 목록을 채움.
	// CollisionPrimitive의 Vertex는 위치만 있으므로, 색상 정보를 포함하는 IlluminatedVertex로 변환
	m_Vertexvec.reserve(vertices.size());
	for (const auto& v : vertices)
	{
		m_Vertexvec.emplace_back(v.m_xmf3Position, XMFLOAT3(0, 0, 0), XMFLOAT2(0, 0), color);
	}

	// 삼각형 인덱스를 라인 리스트 인덱스로 변환
	// 삼각형 인덱스 {0, 1, 2} -> 라인 인덱스 {0,1, 1,2, 2,0}
	m_Indexvec.reserve(indices.size() * 2);
	for (size_t i = 0; i < indices.size(); i += 3)
	{
		UINT i0 = indices[i];
		UINT i1 = indices[i + 1];
		UINT i2 = indices[i + 2];

		m_Indexvec.push_back(i0); m_Indexvec.push_back(i1); // 0-1 라인
		m_Indexvec.push_back(i1); m_Indexvec.push_back(i2); // 1-2 라인
		m_Indexvec.push_back(i2); m_Indexvec.push_back(i0); // 2-0 라인
	}

	// D3D12 리소스 및 뷰를 생성
	m_nStride = sizeof(IlluminatedVertex);
	m_nVertices = m_Vertexvec.size();
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; // 선으로 렌더링

	// 정점 버퍼 생성
	m_pd3dVertexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_Vertexvec.data(), m_nStride * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dVertexUploadBuffer);
	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress();
	m_d3dVertexBufferView.StrideInBytes = m_nStride;
	m_d3dVertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	// 인덱스 버퍼 생성
	m_nIndices = m_Indexvec.size();
	m_pd3dIndexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_Indexvec.data(), sizeof(UINT) * m_nIndices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_pd3dIndexUploadBuffer);
	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = sizeof(UINT) * m_nIndices;
}

DebugWireframeMesh::~DebugWireframeMesh() {}