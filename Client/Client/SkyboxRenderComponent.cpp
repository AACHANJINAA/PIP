#include "stdafx.h"
#include "SkyboxRenderComponent.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "CameraComponent.h"

void SkyboxRenderComponent::pre_render(ID3D12GraphicsCommandList* commandList, Renderer* renderer)
{
    // 1. 카메라의 Skybox 전용 상수 버퍼 바인딩
    CameraComponent* camera = CameraComponent::get_main();
    if (camera && camera->get_cb_skybox())
    {
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = camera->get_cb_skybox()->GetGPUVirtualAddress();
        commandList->SetGraphicsRootConstantBufferView(2, cbAddress); // 루트 파라미터 인덱스 2번
    }

    // 2. Skybox 큐브맵 텍스처 바인딩
    D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu_handle = ResourceManager::instance()->get_skybox_srv_cpu();
    if (srv_cpu_handle.ptr != 0)
    {
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles = { srv_cpu_handle };
        renderer->bind_texture_table(commandList, 4, handles); // 루트 파라미터 인덱스 4번
    }
}