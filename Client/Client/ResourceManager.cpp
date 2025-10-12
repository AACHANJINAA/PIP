#include "stdafx.h"
#include "ResourceManager.h"
#include "Mesh.h" // ReadObjMesh, ReadGlbMesh 등을 포함해야 함
#include "ReadFBXMesh.h"
#include "ReadGlbMesh.h"
#include "ReadOBJMesh.h"
#include "ReadGLTFMesh.h"

void ResourceManager::initialize(ID3D12Device* device)
{

    // --- 기존 Scene::MakeSrv 로직이 여기로 이전 ---
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1024; // 충분한 크기로 할당
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_srvDescriptorHeap));

    _srvDescriptorIncrementSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    _allocatedSrvCount = 0;
}

void ResourceManager::allocate_srv_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE& outCpuHandle,
	D3D12_GPU_DESCRIPTOR_HANDLE& outGpuHandle)
{
    outCpuHandle = _srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    outCpuHandle.ptr += (_srvDescriptorIncrementSize * _allocatedSrvCount);

    outGpuHandle = _srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    outGpuHandle.ptr += (_srvDescriptorIncrementSize * _allocatedSrvCount);

    _allocatedSrvCount++;
}

std::shared_ptr<Mesh> ResourceManager::load_mesh(const std::string& file_path)
{
    // 이미 로드된 메시인지 확인
    auto it = _meshes.find(file_path);
    if (it != _meshes.end()) {
        return it->second;
    }

    // [수정] 파일 확장자에 따라 적절한 메시 로더를 선택
    std::shared_ptr<Mesh> new_mesh = nullptr;
    std::filesystem::path path(file_path);
    std::string extension = path.extension().string();

    CINFO("Loading mesh: " << file_path << " | Detected extension: [" << extension << "]");

    if (extension == ".obj")
    {
        new_mesh = std::make_shared<ReadOBJMesh>(file_path);
    }
    else if (extension == ".glb")
    {
        new_mesh = std::make_shared<ReadGlbMesh>(file_path);
    }
    else if (extension == ".fbx") 
    {
        new_mesh = std::make_shared<ReadFBXMesh>(file_path);
    }
    else if (extension == ".gltf")
    {
        new_mesh = std::make_shared<ReadGLTFMesh>(file_path);
    }
    else
    {
        CERROR("Unsupported mesh file format: " << file_path);
        return nullptr;
    }

    if (!new_mesh)
    {
        CERROR("Failed to create mesh object for: " << file_path);
        return nullptr;
    }

    _meshes[file_path] = new_mesh;
    _pending_meshes.push_back(new_mesh);

    return new_mesh;
}

void ResourceManager::upload_pending_meshes(ID3D12Device* device, ID3D12GraphicsCommandList* command_list)
{
    // 대기 목록에 있는 모든 메시에 대해 upload_to_gpu를 호출합니다.
    for (const auto& mesh : _pending_meshes)
    {
        if (!mesh->is_uploaded())
        {
            mesh->upload_to_gpu(device, command_list);
        }
    }
    // 업로드가 끝났으므로 대기 목록을 비웁니다.
    _pending_meshes.clear();
}

void ResourceManager::release_upload_buffers()
{
    for (const auto& val : _meshes | std::views::values)
    {
        if (val)
        {
            val->release_upload_buffers();
        }
    }
}

void ResourceManager::unload_unused_meshes()
{
    // 맵을 순회하면서 직접 원소를 제거하면 반복자가 무효화되어 위험
    std::vector<std::string> keys_to_unload;

    for (const auto& pair : _meshes)
    {
        const std::string& path = pair.first;
        const std::shared_ptr<Mesh>& mesh_ptr = pair.second;

        // use_count()가 1이라는 것은 오직 이 ResourceManager만이 참조하고 있다는 의미입니다.
        if (mesh_ptr.use_count() == 1)
        {
            keys_to_unload.push_back(path);
        }
    }

    // 수집된 키를 기반으로 맵에서 해당 메시들을 제거합니다.
    for (const std::string& key : keys_to_unload)
    {
        _meshes.erase(key);
        // 맵에서 shared_ptr이 제거되면, 참조 카운트가 0이 되어
        // Mesh 객체의 소멸자가 호출되고, GPU 리소스(ComPtr)도 자동으로 해제됩니다.

        // 로그를 남겨서 확인하면 좋습니다.
        CINFO("Unloaded unused mesh: " << key);
    }
}

std::shared_ptr<Texture> ResourceManager::load_texture(const std::string& file_path, ID3D12Device* device, ID3D12GraphicsCommandList* command_list)
{
    // 이미 로드된 텍스처인지 확인
    auto it = _textures.find(file_path);
    if (it != _textures.end()) {
        return it->second;
    }

    // 새 텍스처 로드
    auto new_texture = std::make_shared<Texture>();
    new_texture->name = file_path;

    // DirectXTex를 사용하여 DDS 파일 로드
    std::wstring wfile_path(file_path.begin(), file_path.end());
    TexMetadata metadata;
    ScratchImage scratch_image;

    HRESULT hr = LoadFromDDSFile(wfile_path.c_str(), DDS_FLAGS_NONE, &metadata, scratch_image);
    if (FAILED(hr)) {
        CERROR("Failed to load texture: " << file_path);
        return nullptr;
    }

    // 텍스처 리소스 생성 - Default Heap
    D3D12_HEAP_PROPERTIES heap_Props = {};
    heap_Props.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_Props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_Props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_Props.CreationNodeMask = 1;
    heap_Props.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC texture_desc = {};
    texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_desc.Alignment = 0;
    texture_desc.Width = metadata.width;
    texture_desc.Height = static_cast<UINT>(metadata.height);
    texture_desc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
    texture_desc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
    texture_desc.Format = metadata.format;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // 업로드를 위해 COPY_DEST 상태로 시작
    hr = device->CreateCommittedResource(&heap_Props, D3D12_HEAP_FLAG_NONE, &texture_desc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&new_texture->resource));

    if (FAILED(hr)) {
        CERROR("Failed to create texture resource for: " << file_path);
        return nullptr;
    }
    
	// Upload heap을 사용하여 텍스처 데이터 업로드
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    for (size_t i = 0; i < scratch_image.GetImageCount(); ++i) {
        const Image* img = scratch_image.GetImage(i, 0, 0);
        D3D12_SUBRESOURCE_DATA subresource = {};
        subresource.pData = img->pixels;
        subresource.RowPitch = img->rowPitch;
        subresource.SlicePitch = img->slicePitch;
        subresources.push_back(subresource);
	}

    UINT64 upload_buffer_size = 0;
	device->GetCopyableFootprints(&texture_desc, 0, static_cast<UINT>(subresources.size()), 0, nullptr, nullptr, nullptr, &upload_buffer_size);

    new_texture->uploadHeap = ::CreateBufferResource(device, command_list, nullptr, upload_buffer_size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

	::UpdateSubresources(command_list, new_texture->resource.Get(), new_texture->uploadHeap.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());

    // 리소스 베리어 설정
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = new_texture->resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	command_list->ResourceBarrier(1, &barrier);

    // SRV 생성
	allocate_srv_descriptor(new_texture->cpuSrvHandle, new_texture->gpuSrvHandle);

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = metadata.format;
    srv_desc.Format = metadata.format;
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0; // 가장 높은 해상도 맵맵 0
    srv_desc.Texture2D.MipLevels = metadata.mipLevels; // 전체 밉맵 개수

    device->CreateShaderResourceView(new_texture->resource.Get(), &srv_desc, new_texture->cpuSrvHandle);

    // 캐시 저장 후 반환
	_textures[file_path] = new_texture;

	CINFO("Loaded texture: " << file_path);
	return new_texture;
}
