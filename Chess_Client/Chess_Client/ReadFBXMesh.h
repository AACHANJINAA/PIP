#pragma once
#include "Mesh.h"

class ReadFBXMesh : public Mesh
{
public:
	ReadFBXMesh() {};
	ReadFBXMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const std::string str);
	virtual ~ReadFBXMesh();

	const std::vector<CollisionPrimitive>& GetCollisionPrimitives() const { return _collisionPrimitives; }
private:
	// Assimp Scene의 노드를 재귀적으로 처리하는 함수
	void ProcessNode(aiNode* node, const aiScene* scene, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	// Assimp Mesh를 처리하여 정점/인덱스 데이터를 추출하는 함수
	void ProcessMesh(aiMesh* mesh, const aiScene* scene, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

private:
	std::string m_texturePath; // 로드된 텍스처 파일 경로 (단순화를 위해 하나만 저장)
	std::vector<CollisionPrimitive> _collisionPrimitives;
};
