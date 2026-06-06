#include "stdafx.h"
#include "FoliageRenderComponent.h"
#include "GameObject.h"
#include "CameraComponent.h"

FoliageRenderComponent::FoliageRenderComponent()
	: InstancedRenderComponent()
{
	_psoName = "gltf_instanced";
}

void FoliageRenderComponent::render(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
	if (!_mesh || _instanceCount == 0 || !_instanceBuffer)
		return;

	// 수정: 정적 함수 호출을 직접 사용
	if (!CameraComponent::get_main())
		return;

	auto owner = game_object();
	if (!owner)
		return;

	XMFLOAT3 camera_pos = CameraComponent::get_main()->game_object()->transform()->get_world_position();
	XMFLOAT3 my_pos = owner->transform()->get_world_position();

	float dx = camera_pos.x - my_pos.x;
	float dy = camera_pos.y - my_pos.y;
	float dz = camera_pos.z - my_pos.z;
	float dist_sq = dx * dx + dy * dy + dz * dz;

	if (dist_sq > _cullDistance * _cullDistance)
		return;

	InstancedRenderComponent::render(commandList, frame_index);
}