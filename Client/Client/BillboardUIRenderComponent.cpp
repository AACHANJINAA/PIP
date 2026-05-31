#include "stdafx.h"
#include "BillboardUIRenderComponent.h"
#include "GameFramework.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "Renderer.h"

BillboardUIRenderComponent::BillboardUIRenderComponent()
{
    _cbData.size = { 10.0f, 10.0f }; // 월드 단위의 기본 크기
    _cbData.alpha = 0.0f;
    _cbData.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    set_pso_name("billboard_ui");
    initialize_buffers();
}

BillboardUIRenderComponent::~BillboardUIRenderComponent()
{
    if (_cbResource)
        _cbResource->Unmap(0, nullptr);
    if (_vertexBuffer)
        _vertexBuffer->Unmap(0, nullptr);
}

void BillboardUIRenderComponent::initialize_buffers()
{
    auto device = GameFramework::instance()->device();

    // 상수 버퍼 (Constant Buffer)
    UINT cbSize = (sizeof(BillboardUICB) + 255) & ~255;
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&_cbResource));

    _cbResource->Map(0, nullptr, reinterpret_cast<void**>(&_cbMappedData));

    // 정점 버퍼 (1개의 점)
    UINT vbSize = sizeof(BillboardUIVertex);
    auto vbBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &vbBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&_vertexBuffer));

    _vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&_vbMappedData));

    // 로컬 원점으로 위치 설정 (트랜스폼이 이동시킬 것임)
    _vbMappedData[0].pos = { 0.0f, 0.0f, 0.0f };

    _vertexBufferView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
    _vertexBufferView.StrideInBytes = sizeof(BillboardUIVertex);
    _vertexBufferView.SizeInBytes = vbSize;
}

void BillboardUIRenderComponent::set_texture(const std::string& texture_path)
{
    _textureInfo = ResourceManager::instance()->load_texture(texture_path, true);
}

void BillboardUIRenderComponent::render(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
    if (_cbData.alpha <= 0.0f || !_textureInfo)
        return; // 투명하거나 텍스처가 없으면 렌더링하지 않음

    // 트랜스폼으로부터 위치 업데이트
    auto pos = game_object()->transform()->get_world_position();
    pos.y += _yOffset;
    _vbMappedData[0].pos = pos;

    // 상수 버퍼 업데이트
    *_cbMappedData = _cbData;

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
    commandList->IASetVertexBuffers(0, 1, &_vertexBufferView);

    // 루트 파라미터 0번은 cbMarker
    commandList->SetGraphicsRootConstantBufferView(0, _cbResource->GetGPUVirtualAddress());

    // 루트 파라미터 2번은 텍스처 테이블
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles = { _textureInfo->cpu_handle };
    Renderer::instance()->bind_texture_table(commandList, 2, handles);

    commandList->DrawInstanced(1, 1, 0, 0);
}
