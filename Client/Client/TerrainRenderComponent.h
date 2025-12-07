
#pragma once
#include "RenderComponent.h"

class TerrainRenderComponent : public RenderComponent
{
public:
	TerrainRenderComponent() = default;
	virtual ~TerrainRenderComponent() = default;

	virtual void pre_render(ID3D12GraphicsCommandList* commandList, class Renderer* renderer) override;
};