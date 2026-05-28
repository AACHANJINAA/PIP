#include "stdafx.h"
#include "InstancedRenderComponent.h"
#include "GameFramework.h"
#include "Mesh.h"
#include "ResourceManager.h"

InstancedRenderComponent::InstancedRenderComponent() : RenderComponent() {
    _psoName = "gltf_instanced";
}

void InstancedRenderComponent::set_instance_data(const std::vector<XMMATRIX>& transforms) {
    _instanceCount = static_cast<UINT>(transforms.size());
    if (_instanceCount == 0) return;

    // 1. 행렬들을 Transpose하여 준비 (HLSL은 Column-major 선호)
    std::vector<XMMATRIX> transposedTransforms;
    for (const auto& mat : transforms) {
        transposedTransforms.push_back(XMMatrixTranspose(mat));
    }

    // 2. GPU 버퍼 생성 (Default Heap 권장하나, 구현 편의상 Upload Heap 예시)
    UINT bufferSize = static_cast<UINT>(sizeof(XMMATRIX) * _instanceCount);

    auto device = GameFramework::instance()->device();
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_instanceBuffer));

    // 3. 데이터 복사
    void* pData = nullptr;
    _instanceBuffer->Map(0, nullptr, &pData);
    memcpy(pData, transposedTransforms.data(), bufferSize);
    _instanceBuffer->Unmap(0, nullptr);
}

void InstancedRenderComponent::render(ID3D12GraphicsCommandList* commandList, UINT frame_index) {
    if (!_mesh || _instanceCount == 0 || !_instanceBuffer) return;

    // t12 레지스터(루트 파라미터 12번)에 인스턴스 버퍼 바인딩
    commandList->SetGraphicsRootShaderResourceView(12, _instanceBuffer->GetGPUVirtualAddress());

    // 인스턴싱 드로우 콜
    _mesh->render_instance(commandList, _instanceCount);
}