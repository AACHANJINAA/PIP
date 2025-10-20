#include "stdafx.h"
#include "TextureManager.h"
#include "Texture.h"
#include "DescriptorManager.h"
#include "DDSTextureLoader12.h"
#include "d3dx12.h"

void TextureManager::initialize(ID3D12Device* device)
{
	_device = device;
}

std::shared_ptr<Texture> TextureManager::load_texture(const std::string& file_path, ID3D12GraphicsCommandList* command_list)
{
    // 1. 캐싱 로직 (기존과 동일)
    auto it = _textures.find(file_path);

    auto new_texture = std::make_shared<Texture>();
    new_texture->name = file_path;
    new_texture->w_name = std::wstring(file_path.begin(), file_path.end());

    // DDS 텍스쳐 데이터 로드
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    HRESULT hr = DirectX::LoadDDSTextureFromFile(
        _device,
        new_texture->w_name.c_str(),
        &new_texture->resource, // [출력] 최종 텍스처 리소스 (Default Heap)
        new_texture->dds_data,   // [출력] 파일에서 읽은 데이터 (메모리 관리용)
        subresources);          // [출력] 업로드에 필요한 서브리소스 정보

    if (FAILED(hr)) {
        CERROR("Failed to load DDS texture data: " << file_path);
        return nullptr;
    }

    // 업로드 힙(임시 버퍼) 생성
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(new_texture->resource.Get(), 0, static_cast<UINT>(subresources.size()));

    D3D12_HEAP_PROPERTIES heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    hr = _device->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&new_texture->upload_heap));

    if (FAILED(hr)) {
        CERROR("Failed to create upload heap for texture: " << file_path);
        return nullptr;
    }

    // 커맨드 리스트에 데이터 복사 명령 기록
    // (CPU 데이터 -> 업로드 힙 -> 최종 텍스처 리소스)
    UpdateSubresources(command_list, new_texture->resource.Get(), new_texture->upload_heap.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());

    // 5. 리소스 배리어 설정 (COPY_DEST -> PIXEL_SHADER_RESOURCE)
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        new_texture->resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    command_list->ResourceBarrier(1, &barrier);

    if (!DescriptorManager::instance()->allocate_descriptor(new_texture->cpu_srv_handle, new_texture->gpu_srv_handle))
    {
		return nullptr;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = new_texture->resource->GetDesc().Format;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels = new_texture->resource->GetDesc().MipLevels;

    _device->CreateShaderResourceView(new_texture->resource.Get(), &srv_desc, new_texture->cpu_srv_handle);

    // 7. 캐시 저장 및 반환 (기존과 동일)
    _textures[file_path] = new_texture;
    return new_texture;
}

void TextureManager::release_upload_buffers()
{
    for (auto const& [key, val] : _textures)
    {
        if (val && val->upload_heap)
        {
            val->upload_heap.Reset();
		}
    }
}