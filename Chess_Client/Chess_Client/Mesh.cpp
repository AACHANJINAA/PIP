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
			CIlluminatedVertex new_vertex(position, normal, texcoord, color);

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

// .mtl 파일을 읽어 재질 정보를 파싱하고 m_mapMaterials 맵에 저장하는 함수
void CReadObjMesh::LoadMtlFile(const std::string& objFilePath, const std::string& mtlFileName)
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




CReadGlbMesh::CReadGlbMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::string str)
{
	// 파일 전체를 읽어올 벡터
	std::vector<char> fileData;

	std::ifstream file("brute_jump.glb", std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		std::cerr << "Error: Failed to open brute_jump.glb" << std::endl;
		return; // 또는 다른 에러 처리
	}

	// 파일 크기를 알아내고 그만큼 벡터 크기 조절
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	fileData.resize(size);

	// 파일 내용을 벡터에 한번에 읽기
	if (!file.read(fileData.data(), size)) {
		std::cerr << "Error: Failed to read file content." << std::endl;
		return;
	}

	file.close();

	std::cout << "Successfully read " << fileData.size() << " bytes from GLB file." << std::endl;


	// GLB 파일 구조에 따라 JSON과 BIN 청크를 읽어오는 과정=========================================

	// JSON과 BIN 데이터를 저장할 변수
	std::string jsonString;
	std::vector<char> binaryData;

	// 포인터처럼 사용할 현재 위치 변수
	char* pData = fileData.data();

	// 1. GLB 헤더 읽기 (12바이트)
	uint32_t magic = *reinterpret_cast<uint32_t*>(pData);
	pData += 4;
	uint32_t version = *reinterpret_cast<uint32_t*>(pData);
	pData += 4;
	uint32_t length = *reinterpret_cast<uint32_t*>(pData);
	pData += 4;

	if (magic != 0x46546C67) { // "glTF"
		std::cerr << "Error: Not a valid GLB file." << std::endl;
		return;
	}

	// 2. 청크 순회 (JSON -> BIN)
	while (pData < fileData.data() + length) {
		// 각 청크의 길이와 타입 읽기
		uint32_t chunkLength = *reinterpret_cast<uint32_t*>(pData);
		pData += 4;
		uint32_t chunkType = *reinterpret_cast<uint32_t*>(pData);
		pData += 4;

		if (chunkType == 0x4E4F534A) { // "JSON"
			// JSON 데이터를 문자열로 복사
			jsonString.assign(pData, chunkLength);
			std::cout << "Found JSON chunk: " << chunkLength << " bytes." << std::endl;
		}
		else if (chunkType == 0x004E4942) { // "BIN"
			// BIN 데이터를 벡터로 복사
			binaryData.assign(pData, pData + chunkLength);
			std::cout << "Found BIN chunk: " << chunkLength << " bytes." << std::endl;
		}

		// 다음 청크로 이동
		pData += chunkLength;
	}

	// 최종 확인
	std::cout << "\nJSON Data:\n" << jsonString.substr(0, 200) << "...\n" << std::endl;
	std::cout << "Binary data size: " << binaryData.size() << " bytes." << std::endl;



	// --- JSON 파싱 시작 ---
	try
	{
		// 한 줄 코드로 JSON 문자열 전체를 파싱!
		auto j = json::parse(jsonString);

		// 이제부터 j 변수를 통해 JSON 데이터에 쉽게 접근할 수 있습니다.
		std::string version = j["asset"]["version"];
		std::cout << "JSON Parsed! glTF Version: " << version << std::endl;

		// "scene" 키의 값을 정수로 읽기
		int scene_id = j["scene"];
		std::cout << "Default Scene Index: " << scene_id << std::endl;

		// "nodes" 배열 가져오기
		const auto& nodes = j["nodes"];
		std::cout << "Number of nodes: " << nodes.size() << std::endl;

		// TODO: 여기서 nodes 배열을 순회하며 데이터를 읽어야 합니다.
		// 1. 모든 노드 정보를 담을 벡터를 선언합니다.
		std::vector<Node> nodeVec;
		nodeVec.resize(nodes.size());

		// 2. nodes 배열을 순회하며 각 노드의 정보를 읽어옵니다.
		for (size_t i = 0; i < nodes.size(); ++i)
		{
			const auto& nodeJson = nodes[i];
			Node& currentNode = nodeVec[i];

			// 이름 (있을 수도 있고 없을 수도 있음)
			if (nodeJson.contains("name")) {
				currentNode.name = nodeJson["name"];
			}

			// 변환 정보 (있을 경우에만 읽기)
			if (nodeJson.contains("translation")) {
				currentNode.translation.x = nodeJson["translation"][0];
				currentNode.translation.y = nodeJson["translation"][1];
				currentNode.translation.z = nodeJson["translation"][2];
			}
			if (nodeJson.contains("rotation")) {
				currentNode.rotation.x = nodeJson["rotation"][0];
				currentNode.rotation.y = nodeJson["rotation"][1];
				currentNode.rotation.z = nodeJson["rotation"][2];
				currentNode.rotation.w = nodeJson["rotation"][3];
			}
			if (nodeJson.contains("scale")) {
				currentNode.scale.x = nodeJson["scale"][0];
				currentNode.scale.y = nodeJson["scale"][1];
				currentNode.scale.z = nodeJson["scale"][2];
			}

			// 자식 노드 인덱스 목록
			if (nodeJson.contains("children")) {
				for (const auto& childIndex : nodeJson["children"]) {
					currentNode.childrenIndices.push_back(childIndex);
				}
			}

			// 메시 및 스킨 인덱스
			if (nodeJson.contains("mesh")) {
				currentNode.meshIndex = nodeJson["mesh"];
			}
			if (nodeJson.contains("skin")) {
				currentNode.skinIndex = nodeJson["skin"];
			}
		}

		// 3. 부모-자식 관계 설정 (부모 인덱스 채우기)
		for (size_t i = 0; i < nodeVec.size(); ++i)
		{
			for (int childIndex : nodeVec[i].childrenIndices)
			{
				if (childIndex >= 0 && childIndex < nodeVec.size()) {
					nodeVec[childIndex].parentIndex = i;
				}
			}
		}

		// 이제 nodeVec 안에 모든 노드의 정보와 계층 구조가 완성되었습니다!
		// 테스트: 루트 노드 중 하나의 이름을 출력해봅시다.
		// brute_jump.glb 파일의 경우 scene[0].nodes[0] 은 0번 노드입니다.
		int rootNodeIndex = j["scenes"][0]["nodes"][0];
		std::cout << "Root Node Name: " << nodeVec[rootNodeIndex].name << std::endl;

		// m_Nodes 멤버 변수에 최종 결과 저장 (클래스 멤버 변수로 선언 필요)
		// m_Nodes = std::move(nodeVec);
	}
	catch (json::parse_error& e)
	{
		std::cerr << "JSON parse error: " << e.what() << std::endl;
		return;
	}

}

CReadGlbMesh::~CReadGlbMesh()
{

}

CReadFbxMesh::CReadFbxMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::string str)
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

	// --- 모든 메쉬 데이터 처리가 끝난 후, 최종 버퍼 생성 ---

	m_nStride = sizeof(CIlluminatedVertex);
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

CReadFbxMesh::~CReadFbxMesh()
{
	// 소멸자
}

void CReadFbxMesh::ProcessNode(aiNode* node, const aiScene* scene, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
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

void CReadFbxMesh::ProcessMesh(aiMesh* mesh, const aiScene* scene, ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList)
{
	// 현재 메쉬의 정점 정보를 임시로 담을 벡터
	std::vector<CIlluminatedVertex> vertices;
	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		CIlluminatedVertex vertex;

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
		// GetTexture 함수는 aiTextureType_DIFFUSE와 같은 aiTextureType 값을 첫 번째 인자로 받습니다.
		if (AI_SUCCESS == material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath)) // 두 번째 인자는 텍스처 인덱스(보통 0)
		{
			// 텍스처 경로를 클래스 멤버 변수에 저장 (단순화를 위해 첫 번째 메쉬의 텍스처만 저장)
			// 실제 프로젝트에서는 여러 메쉬가 다른 텍스처를 가질 수 있으므로 더 복잡한 구조가 필요합니다.
			m_texturePath = texturePath.C_Str();
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