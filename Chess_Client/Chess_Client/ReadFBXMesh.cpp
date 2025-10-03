#include "stdafx.h"
#include "ReadFBXMesh.h"

ReadFBXMesh::ReadFBXMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::string str)
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

	auto [min_x, max_x] = std::minmax_element(_vertexDataBuffer.begin(), _vertexDataBuffer.end(),
		[](const IlluminatedVertex& a, const IlluminatedVertex& b) {
			return a._position.x < b._position.x;
		});

	auto [min_y, max_y] = std::minmax_element(_vertexDataBuffer.begin(), _vertexDataBuffer.end(),
		[](const IlluminatedVertex& a, const IlluminatedVertex& b) {
			return a._position.y < b._position.y;
		});

	auto [min_z, max_z] = std::minmax_element(_vertexDataBuffer.begin(), _vertexDataBuffer.end(),
		[](const IlluminatedVertex& a, const IlluminatedVertex& b) {
			return a._position.z < b._position.z;
		});

	XMFLOAT3 Min(min_x->_position.x, min_y->_position.y, min_z->_position.z);
	XMFLOAT3 Max(max_x->_position.x, max_y->_position.y, max_z->_position.z);

	_orientedBoundingBox = CreateOOBB(Min, Max);

	_vertexStride = sizeof(IlluminatedVertex);
	_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

ReadFBXMesh::~ReadFBXMesh()
{
	// 소멸자
}

void ReadFBXMesh::ProcessNode(aiNode* node, const aiScene* scene, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
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

void ReadFBXMesh::ProcessMesh(aiMesh* mesh, const aiScene* scene, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
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
		// 현재 메쉬의 정점 정보를 임시로 담을 벡터
		std::vector<IlluminatedVertex> _vertices;
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			// Render Mesh
			IlluminatedVertex vertex;

			// 위치 (Position)
			vertex._position.x = mesh->mVertices[i].x;
			vertex._position.y = mesh->mVertices[i].y;
			vertex._position.z = mesh->mVertices[i].z;

			// 법선 (Normal)
			if (mesh->HasNormals())
			{
				vertex._normal.x = mesh->mNormals[i].x;
				vertex._normal.y = mesh->mNormals[i].y;
				vertex._normal.z = mesh->mNormals[i].z;
			}

			// 텍스처 좌표 (Texture Coordinate)
			if (mesh->mTextureCoords[0]) // 텍스처 좌표 채널이 존재하는지 확인
			{
				vertex._texCoord.x = mesh->mTextureCoords[0][i].x;
				vertex._texCoord.y = mesh->mTextureCoords[0][i].y;
			}
			else
			{
				vertex._texCoord = XMFLOAT2(0.0f, 0.0f);
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

			_vertices.push_back(vertex);
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
				_indices.push_back(face.mIndices[j] + _vertices.size());
			}
		}

		// 임시 정점 벡터를 클래스의 전체 정점 벡터(m_Vertexvec)에 합침
		_vertices.insert(_vertices.end(), _vertices.begin(), _vertices.end());
	}
}