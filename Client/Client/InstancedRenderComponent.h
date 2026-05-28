#pragma once
#include "RenderComponent.h"

class InstancedRenderComponent : public RenderComponent
{
public:
    InstancedRenderComponent();
    virtual ~InstancedRenderComponent(){};

    // 인스턴스 행렬 데이터를 설정하고 GPU 버퍼를 생성합니다.
    void set_instance_data(const std::vector<XMMATRIX>& transforms);

    // Renderer가 호출할 드로우 함수
    virtual void render(ID3D12GraphicsCommandList* commandList, UINT frame_index) override;

private:
    ComPtr<ID3D12Resource> _instanceBuffer;
    UINT _instanceCount = 0;
};