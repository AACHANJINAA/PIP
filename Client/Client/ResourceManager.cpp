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

    //CINFO("Loading mesh: " << file_path << " | Detected extension: [" << extension << "]");

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
        //CINFO("Unloaded unused mesh: " << key);
    }
}

#include "DDSTextureLoader12.h"
#include "d3dx12.h"

std::shared_ptr<Texture> ResourceManager::load_texture(const std::string & file_path, ID3D12Device * device, ID3D12GraphicsCommandList * command_list)
{
    // 1. 캐싱 로직 (기존과 동일)
    auto it = _textures.find(file_path);
    if (it != _textures.end()) {
        return it->second;
    }

    auto new_texture = std::make_shared<Texture>();
    new_texture->name = file_path;

    std::wstring wfile_path(file_path.begin(), file_path.end());

    // --- B안: 수동 업로드 구현 시작 ---

    // 2. LoadDDSTextureFromFile 함수로 최종 리소스 생성 및 서브리소스 정보 가져오기
    // 이 함수는 COPY_DEST 상태의 최종 텍스처 리소스까지만 생성해줍니다.
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    HRESULT hr = DirectX::LoadDDSTextureFromFile(
        device,
        wfile_path.c_str(),
        &new_texture->resource, // [출력] 최종 텍스처 리소스 (Default Heap)
        new_texture->ddsData,   // [출력] 파일에서 읽은 데이터 (메모리 관리용)
        subresources);          // [출력] 업로드에 필요한 서브리소스 정보

    if (FAILED(hr)) {
        CERROR("Failed to load DDS texture data: " << file_path);
        return nullptr;
    }

    // 3. 업로드 힙(임시 버퍼) 생성
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(new_texture->resource.Get(), 0, static_cast<UINT>(subresources.size()));

    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&new_texture->uploadHeap));

    if (FAILED(hr)) {
        CERROR("Failed to create upload heap for texture: " << file_path);
        return nullptr;
    }

    // 4. 커맨드 리스트에 데이터 복사 명령 기록
    // (CPU 데이터 -> 업로드 힙 -> 최종 텍스처 리소스)
    UpdateSubresources(command_list, new_texture->resource.Get(), new_texture->uploadHeap.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());

    // 5. 리소스 배리어 설정 (COPY_DEST -> PIXEL_SHADER_RESOURCE)
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        new_texture->resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    command_list->ResourceBarrier(1, &barrier);

    // --- B안: 수동 업로드 구현 끝 ---

    // 6. SRV 생성 (기존과 동일)
    allocate_srv_descriptor(new_texture->cpuSrvHandle, new_texture->gpuSrvHandle);

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = new_texture->resource->GetDesc().Format;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels = new_texture->resource->GetDesc().MipLevels;

    device->CreateShaderResourceView(new_texture->resource.Get(), &srv_desc, new_texture->cpuSrvHandle);

    // 7. 캐시 저장 및 반환 (기존과 동일)
    _textures[file_path] = new_texture;
    return new_texture;
}