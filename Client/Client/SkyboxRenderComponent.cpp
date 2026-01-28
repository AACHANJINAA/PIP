#include "stdafx.h"
#include "SkyboxRenderComponent.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "CameraComponent.h"
#include "GameObject.h"

SkyboxRenderComponent::SkyboxRenderComponent()
{
    set_name("SkyboxRenderComponent");
}

void SkyboxRenderComponent::render(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
    if (_mesh)
    {
        _mesh->render(commandList);
    }
}