#pragma once
#include "stdafx.h"
#include "RenderComponent.h"
#include "ResourceManager.h"

struct alignas(16) HPBarCB {
	DirectX::XMFLOAT2 size;     // g_Size (가로, 세로 반폭)
	float             padding[2]; // 16바이트 정렬을 위한 8바이트 패딩
};

struct HPBarVertex {
	DirectX::XMFLOAT3 pos;      // 12바이트 (float x, y, z) -> POSITION
	float             hpRatio;  // 4바이트 (float)          -> TEXCOORD0
};

class MonsterHPUIRenderComponent : public RenderComponent
{
public:
	MonsterHPUIRenderComponent();
	virtual ~MonsterHPUIRenderComponent();
	// render 함수는 이제 Renderer에 의해 호출됩니다.
	virtual void render(ID3D12GraphicsCommandList* commandList, UINT frame_index) override;

	void set_hp_back_texture(const std::string& texture_path); // HP 바 배경 텍스처 설정
	void set_hp_bar_texture(const std::string& texture_path); // HP 바 텍스처 설정


	// UI는 frustum culling 불필요
	virtual bool is_visible(const BoundingFrustum& frustum) const override { return true; }

	// UI는 유효하지 않은 bounding box 반환
	virtual BoundingOrientedBox get_world_bounding_box() const override
	{
		BoundingOrientedBox box;
		box.Center = XMFLOAT3(0, 0, 0);
		box.Extents = XMFLOAT3(0, 0, 0);
		box.Orientation = XMFLOAT4(0, 0, 0, 1);
		return box;
	}

private:
	void initialize_constant_buffer();
	void initialize_vertex_buffer();
	void upload_shader();


	DirectX::XMFLOAT2 _size{ 0.1f, 0.02f }; // HP 바 크기

	// 상수 버퍼 관련 멤버 변수
	ComPtr<ID3D12Resource> _cbResource; // GPU 리소스
	HPBarCB* _cbMappedData = nullptr; // CPU에서 접근 가능한 주소

	// 정점 버퍼 관련 멤버 변수
	ComPtr<ID3D12Resource> _vertexBuffer;
	HPBarVertex* _vbMappedData = nullptr;

	const UINT _maxMonsterCount = 10000; // 넉넉하게 10000마리까지 지원

	ResourceManager::TextureInfo* _texture_HP_back = nullptr; // HP바 배경 텍스처
	ResourceManager::TextureInfo* _texture_HP_Bar = nullptr; // HP 바 텍스처
};

