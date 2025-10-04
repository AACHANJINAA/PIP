#include "stdafx.h"
#include "ReadFBXMesh.h"

ReadFBXMesh::ReadFBXMesh(const std::string& file_path)
{
	// Assimp Importer 객체 생성
	Assimp::Importer importer;

	// 파일을 읽어 Assimp의 scene 객체로 변환
	// aiProcess_Triangulate: 모든 면을 삼각형으로 분할
	// aiProcess_FlipUVs: UV(텍스처 좌표)의 y축을 뒤집기
	// aiProcess_CalcTangentSpace: 탄젠트와 바이탄젠트 계산
	const aiScene* pScene = importer.ReadFile(file_path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

	// 파일 읽기 실패 시 처리
	if (!pScene || pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !pScene->mRootNode)
	{
		// 에러 로그 출력
		OutputDebugStringA(importer.GetErrorString());
		return;
	}

	// 루트 노드부터 시작하여 모든 노드를 재귀적으로 처리
	ProcessNode(pScene->mRootNode, pScene);

    if (_vertexDataBuffer.empty())
    {
        // 기본값으로 초기화하거나, 오류 처리를 할 수 있습니다.
        _orientedBoundingBox = BoundingOrientedBox();
    }
    else
    {
        IlluminatedVertex* I_begin = reinterpret_cast<IlluminatedVertex*>(_vertexDataBuffer.data());
        IlluminatedVertex* I_end = I_begin + (_vertexDataBuffer.size() / sizeof(IlluminatedVertex));

        auto [min_x, max_x] = std::minmax_element(I_begin, I_end,
            [](const IlluminatedVertex& a, const IlluminatedVertex& b) {
                return a._position.x < b._position.x;
            });

        auto [min_y, max_y] = std::minmax_element(I_begin, I_end,
            [](const IlluminatedVertex& a, const IlluminatedVertex& b) {
                return a._position.y < b._position.y;
            });

        auto [min_z, max_z] = std::minmax_element(I_begin, I_end,
            [](const IlluminatedVertex& a, const IlluminatedVertex& b) {
                return a._position.z < b._position.z;
            });


        XMFLOAT3 Min(min_x->_position.x, min_y->_position.y, min_z->_position.z);
        XMFLOAT3 Max(max_x->_position.x, max_y->_position.y, max_z->_position.z);

        _orientedBoundingBox = CreateOOBB(Min, Max);

        _vertexStride = sizeof(IlluminatedVertex);
        _primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

ReadFBXMesh::~ReadFBXMesh()
{
	// 소멸자
}

void ReadFBXMesh::ProcessNode(aiNode* node, const aiScene* scene)
{
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		ProcessMesh(mesh, scene);
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		ProcessNode(node->mChildren[i], scene);
	}
}

void ReadFBXMesh::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
    std::string meshNameStr = mesh->mName.C_Str();

    if (meshNameStr.rfind("UCX_", 0) == 0)
    {
        CollisionPrimitive primitive;

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            const aiVector3D& vtx = mesh->mVertices[i];
            if (std::isnan(vtx.x) || std::isnan(vtx.y) || std::isnan(vtx.z) ||
                std::isinf(vtx.x) || std::isinf(vtx.y) || std::isinf(vtx.z))
            {
                char buffer[256];
                sprintf_s(buffer, "ReadFBXMesh::ProcessMesh ERROR : Invalid vertex data loaded from mesh: %s at index %u\n", meshNameStr.c_str(), i);
                OutputDebugStringA(buffer);
                continue;
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

        DirectX::BoundingBox::CreateFromPoints(primitive.aabb, primitive._vertices.size(),
            &primitive._vertices[0]._position, sizeof(Vertex));
        DirectX::BoundingOrientedBox::CreateFromPoints(primitive.oobb, primitive._vertices.size(),
            &primitive._vertices[0]._position, sizeof(Vertex));

        _collisionPrimitives.push_back(primitive);
    }
    else
    {
        std::vector<IlluminatedVertex> temp_vertices;
        temp_vertices.reserve(mesh->mNumVertices); // 성능을 위해 미리 메모리 할당

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            IlluminatedVertex vertex;

            vertex._position.x = mesh->mVertices[i].x;
            vertex._position.y = mesh->mVertices[i].y;
            vertex._position.z = mesh->mVertices[i].z;
			vertex._position.z = mesh->mVertices[i].z;
            if (mesh->HasNormals())
            {
                vertex._normal.x = mesh->mNormals[i].x;
                vertex._normal.y = mesh->mNormals[i].y;
                vertex._normal.z = mesh->mNormals[i].z;
            }
			
            if (mesh->mTextureCoords[0])
            {
                vertex._texCoord.x = mesh->mTextureCoords[0][i].x;
                vertex._texCoord.y = mesh->mTextureCoords[0][i].y;
            }
            else
            {
                vertex._texCoord = XMFLOAT2(0.0f, 0.0f);
            }
			

            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            aiColor4D diffuseColor;
            if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor))
            {
                vertex._diffuse = XMFLOAT4(diffuseColor.r, diffuseColor.g, diffuseColor.b, diffuseColor.a);
            }
            else
            {
                vertex._diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            }

            aiString texturePath;
            if (AI_SUCCESS == material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath))
            {
                m_texturePath = texturePath.C_Str();
            }
            temp_vertices.push_back(vertex);
        }
		
        // 새 인덱스를 추가하기 전, 현재까지 누적된 정점 수를 base index로 사용
        UINT baseVertex = _vertexCount;

        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
            {
                _indices.push_back(face.mIndices[j] + baseVertex);
            }
        }
		
        // 임시 벡터의 정점 데이터를 메인 버퍼(_vertexDataBuffer)에 복사
        size_t current_buffer_size = _vertexDataBuffer.size();
        size_t new_vertices_size = temp_vertices.size() * sizeof(IlluminatedVertex);
        _vertexDataBuffer.resize(current_buffer_size + new_vertices_size);
        memcpy(_vertexDataBuffer.data() + current_buffer_size, temp_vertices.data(), new_vertices_size);

        // 전체 정점 개수 업데이트
        _vertexCount += temp_vertices.size();
    }
	
}