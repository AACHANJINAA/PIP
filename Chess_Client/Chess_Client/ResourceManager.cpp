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

std::shared_ptr<Mesh> ResourceManager::load_mesh(const std::string& filePath)
{
    auto it = _meshes.find(filePath);
    if (it != _meshes.end())
    {
        return it->second;
    }

    std::shared_ptr<Mesh> newMesh = nullptr;
    std::string extension = filePath.substr(filePath.find_last_of("."));

    if (extension == ".obj")
    {
        newMesh = std::make_shared<ReadObjMesh>(filePath);
    }
    else if (extension == ".glb")
    {
		newMesh = std::make_shared<ReadGlbMesh>(filePath);
    }

    if (newMesh)
    {
        _meshes[filePath] = newMesh;
        return newMesh;
    }
    return nullptr;
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
