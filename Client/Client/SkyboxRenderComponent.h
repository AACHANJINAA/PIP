#include "RenderComponent.h"

class SkyboxRenderComponent : public RenderComponent
{
public:
    SkyboxRenderComponent();
    virtual ~SkyboxRenderComponent() = default;

    void render(ID3D12GraphicsCommandList* commandList, UINT frame_index);
};