#include "stdafx.h"
#include "ResourceManager.h"
#include "Mesh.h" // ReadObjMesh, ReadGlbMesh 등을 포함해야 함

void ResourceManager::initialize(ID3D12Device* device)
{
    _device = device;

    // --- 기존 Scene::MakeSrv 로직이 여기로 이전 ---
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1024; // 충분한 크기로 할당
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    _device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_srvDescriptorHeap));

    _srvDescriptorIncrementSize =
        _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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

    // [변경] 새로 메시를 로드한 경우 (이때는 CPU 데이터만 로드됨)
    // ReadGlbMesh, ReadObjMesh 등 적절한 클래스 사용
    std::shared_ptr<Mesh> new_mesh = std::make_shared<ReadGlbMesh>(file_path);
    //TODO: 템플릿 코드로 만들어야할듯?

    _meshes[file_path] = new_mesh;

    // [추가] GPU에 업로드해야 할 '대기 목록'에 추가합니다.
    _pending_meshes.push_back(new_mesh);

    return new_mesh;
}

void ResourceManager::upload_pending_meshes(ID3D12GraphicsCommandList* command_list)
{
    // 대기 목록에 있는 모든 메시에 대해 upload_to_gpu를 호출합니다.
    for (const auto& mesh : _pending_meshes)
    {
        if (!mesh->is_uploaded())
        {
            mesh->upload_to_gpu(_device, command_list);
        }
    }
    // 업로드가 끝났으므로 대기 목록을 비웁니다.
    _pending_meshes.clear();
}

void ResourceManager::release_upload_buffers()
{
    // 이 함수는 GameFramework::BuildObjects 마지막에 호출되어야 합니다.
      // 모든 메시의 임시 업로드 버퍼를 해제합니다.
    for (auto const& [key, val] : _meshes)
    {
        // val->release_upload_buffers(); // Mesh 클래스에 해당 함수 구현 필요
    }
}
