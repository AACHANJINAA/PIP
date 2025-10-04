#include "stdafx.h"
#include "ReadFBXMesh.h"

ReadFBXMesh::ReadFBXMesh(const std::string str)
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

	// [수정] FBX의 모든 서브메쉬 데이터를 통합할 임시 벡터를 생성합니다.
	std::vector<IlluminatedVertex> combined_vertices;
	std::vector<UINT> combined_indices;

	// [수정] 루트 노드부터 시작하여 모든 노드를 재귀적으로 처리하고, 데이터를 임시 벡터에 누적합니다.
	ProcessNode(pScene->mRootNode, pScene, combined_vertices, combined_indices);

	// [수정] 누적된 데이터로 최종 버퍼를 설정합니다.
	set_vertex_data_buffer(combined_vertices);
	_indices = combined_indices;

	// [수정] 바운딩 박스 계산은 모든 정점이 통합된 후에 수행합니다.
	if (!combined_vertices.empty())
	{
		auto [min_x, max_x] = std::minmax_element(combined_vertices.begin(), combined_vertices.end(),
			[](const IlluminatedVertex & a, const IlluminatedVertex & b) {
			return a._position.x < b._position.x;
		});
		auto [min_y, max_y] = std::minmax_element(combined_vertices.begin(), combined_vertices.end(),
			[](const IlluminatedVertex & a, const IlluminatedVertex & b) {
			return a._position.y < b._position.y;
		});
		auto [min_z, max_z] = std::minmax_element(combined_vertices.begin(), combined_vertices.end(),
			[](const IlluminatedVertex & a, const IlluminatedVertex & b) {
			return a._position.z < b._position.z;
		});

		XMFLOAT3 min_pos(min_x->_position.x, min_y->_position.y, min_z->_position.z);
		XMFLOAT3 max_pos(max_x->_position.x, max_y->_position.y, max_z->_position.z);

		_orientedBoundingBox = CreateOOBB(min_pos, max_pos);
	}
}

ReadFBXMesh::~ReadFBXMesh()
{
	// 소멸자
}

void ReadFBXMesh::ProcessNode(aiNode* node, const aiScene* scene, std::vector<IlluminatedVertex>& combined_vertices, std::vector<UINT>& combined_indices)
{
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		// [수정] 통합 벡터를 그대로 전달
		ProcessMesh(mesh, scene, combined_vertices, combined_indices);
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		// [수정] 통합 벡터를 그대로 전달
		ProcessNode(node->mChildren[i], scene, combined_vertices, combined_indices);
	}
}

void ReadFBXMesh::ProcessMesh(aiMesh* mesh, const aiScene* scene, 
	std::vector<IlluminatedVertex>& combined_vertices,
	std::vector<UINT>& combined_indices)
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
				CLOG("!!! CRITICAL ERROR: Invalid vertex data loaded from mesh: " << meshNameStr << "at index " << i);
				continue; // 이 비정상적인 정점은 건너뜁니다.
			}

			Vertex v = {};
			v._position.x = mesh->mVertices[i].x;
			v._position.y = mesh->mVertices[i].y;
			v._position.z = mesh->mVertices[i].z;
			primitive._vertices.push_back(v);
		}

		for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
		{
			aiFace face = mesh->mFaces[i];

			for (unsigned int j = 0; j < face.mNumIndices; ++j)
			{
				primitive._indices.push_back(face.mIndices[j]);
			}
		}

		// AABB 계산 
		DirectX::BoundingBox::CreateFromPoints(primitive.aabb, primitive._vertices.size(), &primitive._vertices[0]._position, sizeof(Vertex));
		// OBB 계산
		DirectX::BoundingOrientedBox::CreateFromPoints(primitive.oobb, primitive._vertices.size(), &primitive._vertices[0]._position, sizeof(Vertex));

		XMVECTOR quat = XMLoadFloat4(&primitive.oobb.Orientation);

		XMVECTOR lengthSq = XMVector4LengthSq(quat);

		float fLengthSq;
		XMStoreFloat(&fLengthSq, lengthSq);

		_collisionPrimitives.push_back(primitive);
	}
	else {
		// [수정] 지역 변수 이름을 _vertices에서 mesh_vertices로 변경하여 혼동을 방지합니다.
		std::vector<IlluminatedVertex> mesh_vertices;
		mesh_vertices.reserve(mesh->mNumVertices);	// 미리 메모리를 할당합니다.
		
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			// Render Mesh
			IlluminatedVertex vertex;

			// 위치 (Position)
			vertex._position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

			// 법선 (Normal)
			if (mesh->HasNormals())
			{
				vertex._normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
			}
			else
			{
				vertex._normal = { 0.0f, 0.0f, 0.0f };
			}


			// 텍스처 좌표 (Texture Coordinate)
			if (mesh->mTextureCoords[0]) // 텍스처 좌표 채널이 존재하는지 확인
			{
				vertex._texCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
			}
			else
			{
				vertex._texCoord = { 0.0f, 0.0f };
			}

			// 재질 정보 처리
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			aiColor4D diffuseColor;
			aiString texturePath;

			// 확산 색상 가져오기 (없으면 흰색 기본값)
			if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor))
			{
				vertex._diffuse = XMFLOAT4(diffuseColor.r, diffuseColor.g, diffuseColor.b, diffuseColor.a);
			}
			else
			{
				vertex._diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f); // 기본 흰색
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

			mesh_vertices.push_back(vertex);
		}

		UINT index_offset = static_cast<UINT>(combined_vertices.size());

		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; j++)
			{
				// [수정] _indices가 아닌, 함수 인자로 받은 combined_indices에 추가해야 합니다.
				combined_indices.push_back(face.mIndices[j] + index_offset);
			}
		}

		// [수정] 지역 변수인 mesh_vertices의 내용을 combined_vertices에 합칩니다.
		combined_vertices.insert(combined_vertices.end(), mesh_vertices.begin(), mesh_vertices.end());
	}
}