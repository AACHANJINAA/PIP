#pragma once
#include "Mesh.h"
struct OBJMaterial
{
	std::string name;
	XMFLOAT4 Ka; // Ambient
	XMFLOAT4 Kd; // Diffuse
	XMFLOAT4 Ks; // Specular
	float Ns;    // Specular Exponent 
};
class ReadOBJMesh : public Mesh
{
public:
	ReadOBJMesh() = default;
	ReadOBJMesh(const std::string& filePath);

	//void ChangeColor(float r, float g, float b, float a);

	//virtual void UpdateVertices(ID3D12GraphicsCommandList* pd3dCommandList);

	~ReadOBJMesh() override;
private:
	void LoadMtlFile(const std::string& objFilePath, const std::string& mtlFileName);
private:
	float _front = 0.0f;
	float _back = 0.0f;
	float _left = 0.0f;
	float _right = 0.0f;
	float _top = 0.0f;
	float _bottom = 0.0f;
	std::map<std::string, OBJMaterial> m_mapMaterials;
};
