#include "stdafx.h"
#include "ReadOBJMesh.h"
// --- ReadObjMesh Derived Class ---

// [변경] 생성자 구현
ReadOBJMesh::ReadOBJMesh(const std::string& filePath)
{
	set_name(filePath); // Object의 이름 설정

	// --- 아래는 기존의 파싱 로직을 거의 그대로 사용합니다 ---
	std::ifstream in{ filePath };
	if (!in) {
		CERROR("OBJ파일 읽기 오류")
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
			LoadMtlFile(filePath, mtlFileName);
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

	_vertexDataBuffer.clear();
	std::vector<IlluminatedVertex> temp_vertices; // 임시 정점 저장용
	_indices.clear();

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
			XMFLOAT3 tangent{};
			XMVECTOR n_vec = XMLoadFloat3(&normal);

			XMVECTOR up = (abs(normal.y) < 0.99f) ? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

			XMVECTOR t_vec = XMVector3Normalize(XMVector3Cross(up, n_vec));
			XMStoreFloat3(&tangent, t_vec);

			IlluminatedVertex new_vertex(position, normal, texcoord, tangent);

			// 이 새 정점을 최종 정점 목록에 추가
			temp_vertices.emplace_back(new_vertex);

			// 방금 추가한 정점의 인덱스 번호를 구하고
			UINT new_index = static_cast<UINT>(temp_vertices.size() - 1);

			// 새로 만든 정점의 인덱스를 최종 인덱스 목록에 추가
			_indices.emplace_back(new_index);

			// map에 (위치/법선 조합, 새로 부여된 인덱스)를 기록 <- 이러면 다음에 똑같은게 나오면 중복체크가 되서 넘어가겠지 
			vertex_map[vertex_key] = new_index;
		}
		else
		{
			// 정점을 또 만들지 않고, map에서 찾은 기존 인덱스 번호를 최종 인덱스 목록에 추가하여 재사용
			_indices.emplace_back(it->second);
		}
	}

	// (수정) IlluminatedVertex로 변경
	auto [min_x, max_x] =
		std::minmax_element(temp_vertices.begin(), temp_vertices.end(),
			[](const IlluminatedVertex& a, const IlluminatedVertex& b)
			{
				return a._position.x < b._position.x;
			});
	auto [min_y, max_y] =
		std::minmax_element(temp_vertices.begin(), temp_vertices.end(),
			[](const IlluminatedVertex& a, const IlluminatedVertex& b)
			{
				return a._position.y < b._position.y;
			});
	auto [min_z, max_z] =
		std::minmax_element(temp_vertices.begin(), temp_vertices.end(),
			[](const IlluminatedVertex& a, const IlluminatedVertex& b)
			{
				return a._position.z < b._position.z;
			});

	_bottom = min_y->_position.y;
	_top = max_y->_position.y;

	_right = max_x->_position.x;
	_left = min_x->_position.x;

	_front = max_z->_position.z;
	_back = min_z->_position.z;

	_orientedBoundingBox = CreateOOBB(XMFLOAT3(min_x->_position.x, min_y->_position.y, min_z->_position.z),
		XMFLOAT3(max_x->_position.x, max_y->_position.y, max_z->_position.z));

	//--- 아래 렌더 시 필요한 정보
	set_vertex_data_buffer(temp_vertices);
	_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;


}
ReadOBJMesh::~ReadOBJMesh()
{

}

// .mtl 파일을 읽어 재질 정보를 파싱하고 m_mapMaterials 맵에 저장하는 함수
void ReadOBJMesh::LoadMtlFile(const std::string& objFilePath, const std::string& mtlFileName)
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
			m_mapMaterials[currentMtlName] = OBJMaterial();
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