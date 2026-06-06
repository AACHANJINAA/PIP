#pragma once
#include "InstancedRenderComponent.h"

class FoliageRenderComponent : public InstancedRenderComponent
{
public:
	FoliageRenderComponent();
	virtual ~FoliageRenderComponent() = default;

	// 거리 기반 컬링이 적용된 렌더 함수
	virtual void render(ID3D12GraphicsCommandList* commandList, UINT frame_index) override;

	// 렌더링 거리 설정 (기본값: 100m)
	void set_cull_distance(float distance) { _cullDistance = distance; }
	float cull_distance() const { return _cullDistance; }

	// Shadow Pass 제외 플래그
	void set_cast_shadow(bool cast) { _castShadow = cast; }
	bool cast_shadow() const { return _castShadow; }

private:
	float _cullDistance = 100.0f;  // 100미터 이상이면 컬링
	bool _castShadow = false;       // 그림자 투사 안함
};