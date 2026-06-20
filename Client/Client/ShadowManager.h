#pragma once
#include "stdafx.h"

// 256바이트 정렬을 위한 헬퍼 구조체
struct CbCascadeSingle
{
    XMFLOAT4X4 lightVP;
    float padding[48]; // 64바이트(행렬) + 192바이트 = 256바이트 맞춤
};

struct CbCascades
{
    CbCascadeSingle cascades[3]; // 총 3개의 Cascade 행렬
};
struct CbShadow
{
    XMFLOAT4X4 lightVP[3];
    XMFLOAT4X4 staticLightVP[3];
    float splitNear;
    float splitMid;
    float bias;      
    float pad;
};

class ShadowManager : public Singleton<ShadowManager>
{
public:
    friend Singleton<ShadowManager>;

    void initialize(ID3D12Device* device);
    void update_and_execute(ID3D12GraphicsCommandList* cmd, UINT frame_index);
    void bind_for_lighting(ID3D12GraphicsCommandList* cmd, UINT shadowCbParamIdx, UINT shadowSrvParamIdx, class Renderer* renderer);

	void set_shadow_max_distance(float distance) { shadow_max_distance = distance; }
	float get_shadow_max_distance() const { return shadow_max_distance; }

	void set_static_update_distance_threshold(float distance) { _staticUpdateDistanceThreshold = distance; }
	float static_update_distance_threshold() const { return _staticUpdateDistanceThreshold; }

private:
    ShadowManager() = default;
    ~ShadowManager() = default;

    void build_cascade_matrices();

    int _shadowmapSize = 6144;

    float shadow_max_distance = 300;

    ComPtr<ID3D12Resource>       _shadowMapArray;
    ComPtr<ID3D12DescriptorHeap> _dsvHeap;        // CPU only, 3 slots
    ComPtr<ID3D12DescriptorHeap> _srvHeap;        // CPU only, 1 slot

    ComPtr<ID3D12Resource>       _staticShadowMapArray;
    ComPtr<ID3D12DescriptorHeap> _dsvStaticHeap;
    ComPtr<ID3D12DescriptorHeap> _srvStaticHeap;

    ComPtr<ID3D12Resource>       _cbCascades;
    CbCascades* _mappedCbCascades = nullptr;

    ComPtr<ID3D12Resource>       _cbShadow[3];    // double buffering
    CbShadow* _mappedCbShadow[3] = {};

    ComPtr<ID3D12PipelineState>  _shadowPso;
    ComPtr<ID3D12RootSignature>  _shadowRootSig;

    CbCascades _cascadeData;
    CbShadow   _shadowData;

    CbCascades _staticCascadeData;
    f3 _lastStaticUpdateCamPos = {-999999.0f, -999999.0f, -999999.0f};
    bool _forceStaticUpdate = true;
    float _staticUpdateDistanceThreshold = 20.0f; // 정적 그림자 맵 갱신 거리 기준 (단위: 미터)

    UINT _currentFrameIndex = 0;
    UINT _frameCount{ 0 }; // 캐스케이드 업데이트 주기 분리용 카운터
};