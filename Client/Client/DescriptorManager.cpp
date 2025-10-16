#include "stdafx.h"
#include "DescriptorManager.h"

void DescriptorManager::initialize(ID3D12Device* device, UINT descriptor_count)
{
	_capacity = descriptor_count;

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = _capacity; // 충분한 크기로 할당
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_descriptorHeap)))) {
		CERROR("Descriptor Heap Create Failed");
		return;
	}

	_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_currentIndex = 0;
}

bool DescriptorManager::allocate_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE& out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE& out_gpu_handle)
{
	if (_currentIndex >= _capacity)
	{
	    CERROR("Descriptor Heap is full!");
	    return false; // 더 이상 할당할 수 없음
	}

	// cpu handle 계산
	out_cpu_handle = _descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	out_cpu_handle.ptr += (_descriptorSize * _currentIndex);

	// GPU handle 계산
	out_gpu_handle = _descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	out_gpu_handle.ptr += (_descriptorSize * _currentIndex);

	_currentIndex++;

	return true;
}
