#pragma once
#include "RenderComponent.h"

class SkyboxRenderComponent : public RenderComponent
{
public:
	SkyboxRenderComponent() = default;
	virtual ~SkyboxRenderComponent() = default;

	virtual void pre_render(ID3D12GraphicsCommandList* commandList, class Renderer* renderer) override;
};
