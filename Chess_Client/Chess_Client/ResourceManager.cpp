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

std::shared_ptr<Mesh> ResourceManager::load_mesh(const std::string& filePath, ID3D12GraphicsCommandList* command_list)
{
    // 1. 맵에 이미 로드된 메시인지 확인
    auto it = _meshes.find(filePath);
    if (it != _meshes.end())
    {
        // 맵에 존재하면, 저장된 shared_ptr을 반환 (중복 로드 방지)
        return it->second;
    }

    // 2. 맵에 없다면, 새로 로드
    std::shared_ptr<Mesh> newMesh = nullptr;

    // 파일 확장자를 찾아 어떤 로더를 사용할지 결정
    std::string extension;
    size_t pos = filePath.find_last_of('.');
    if (pos != std::string::npos) {
        extension = filePath.substr(pos);
    }

    if (extension == ".obj")
    {
        newMesh = std::make_shared<ReadObjMesh>(_device, command_list, filePath.c_str());
    }
    else if (extension == ".glb")
    {
        // ReadGlbMesh 생성자가 Scene*을 받는다면, 일단 null이나 다른 방식으로 처리 필요
        // newMesh = std::make_shared<ReadGlbMesh>(_device, _commandList, filePath, nullptr);
        newMesh = std::make_shared<ReadGlbMesh>(_device, command_list, filePath, nullptr
            /*TODO: 일단 널로 했는데 변경 필요->디스크립터 테이블을 씬에서 들고 있었는데 여기서 들고 있게 변경 필요*/);
    }

    if (newMesh) // && newMesh->is_valid()) // is_valid 같은 유효성 검사 함수가 있다면 추가
    {
        // 3. 로드에 성공하면 맵에 저장하고 반환
        _meshes[filePath] = newMesh;
        return newMesh;
    }

    // 로드 실패 시 nullptr 반환
    return nullptr;
}
