#include "stdafx.h"
#include "SkyboxRenderComponent.h"

SkyboxRenderComponent::SkyboxRenderComponent()
{
    set_name("SkyboxRenderComponent");
    set_pso_name("skybox");
}
void SkyboxRenderComponent::render(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
    if (_mesh)
    {
        _mesh->render(commandList);
    }
}