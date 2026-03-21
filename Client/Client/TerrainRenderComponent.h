
#pragma once
#include "RenderComponent.h"

class TerrainRenderComponent : public RenderComponent
{
public:
	TerrainRenderComponent();
	virtual ~TerrainRenderComponent();

	virtual void pre_render(ID3D12GraphicsCommandList* commandList, class Renderer* renderer) override;
private:
	ComPtr<ID3D12Resource> _terrain_info_cbuffer;
	UINT8* _terrain_info_cbuffer_cpu_address = nullptr;

	// Layer 정보 상수 버퍼
	ComPtr<ID3D12Resource> _layer_info_cbuffer;
	UINT8* _layer_info_cbuffer_cpu_address = nullptr;
};