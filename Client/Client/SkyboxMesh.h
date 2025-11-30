#pragma once
#include "Mesh.h"

class SkyboxMesh : public Mesh
{
public:
	SkyboxMesh(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	~SkyboxMesh() override = default;
};

