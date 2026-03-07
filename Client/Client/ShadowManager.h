#pragma once
#include "stdafx.h"

struct CbCascades
{
    XMFLOAT4X4 lightVP[3];
};

struct CbShadow
{
    XMFLOAT4X4 lightVP[3];
    float splitNear; // 20.0f
    float splitMid;  // 80.0f
    float bias;      // 0.005f
    float pad;
};

class ShadowManager : public Singleton<ShadowManager>
{
public:
    friend Singleton<ShadowManager>;

    void initialize(ID3D12Device* device);
    void update_and_execute(ID3D12GraphicsCommandList* cmd, UINT frame_index);
    void bind_for_lighting(ID3D12GraphicsCommandList* cmd, UINT shadowCbParamIdx, UINT shadowSrvParamIdx, class Renderer* renderer);

private:
    ShadowManager() = default;
    ~ShadowManager() = default;

    void build_cascade_matrices();

    ComPtr<ID3D12Resource>       _shadowMapArray;
    ComPtr<ID3D12DescriptorHeap> _dsvHeap;        // CPU only, 3 slots
    ComPtr<ID3D12DescriptorHeap> _srvHeap;        // CPU only, 1 slot

    ComPtr<ID3D12Resource>       _cbCascades;
    CbCascades* _mappedCbCascades = nullptr;

    ComPtr<ID3D12Resource>       _cbShadow[2];    // double buffering
    CbShadow* _mappedCbShadow[2] = {};

    ComPtr<ID3D12PipelineState>  _shadowPso;
    ComPtr<ID3D12RootSignature>  _shadowRootSig;

    CbCascades _cascadeData;
    CbShadow   _shadowData;

    UINT _currentFrameIndex = 0;
};