#pragma once
#include "stdafx.h"
#include "RenderComponent.h"

struct CbGameHpInfo
{
    int _hp;
};

class HPRenderComponent : public RenderComponent
{
public:
    HPRenderComponent();
    virtual ~HPRenderComponent();

    virtual void render(ID3D12GraphicsCommandList* commandList, UINT frameIndex) override;

protected:
    ComPtr<ID3D12Resource> _cbGameHpInfo;
    CbGameHpInfo* _mappedCbGameHpInfo = nullptr;
};
