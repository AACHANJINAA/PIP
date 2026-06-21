#include "stdafx.h"
#include "DescriptorManager.h"

void DescriptorManager::initialize(ID3D12Device* device, UINT descriptor_count)
{
	_capacity = descriptor_count;

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = _capacity; // 충분한 크기로 할당
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = _isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_descriptorHeap)))) {
		CERROR("Descriptor Heap Create Failed");
		return;
	}

	_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_currentIndex = 0;
}

bool DescriptorManager::allocate_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE& out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE& out_gpu_handle)
{
	if (!_freeList.empty())
	{
		UINT index = _freeList.front();
		_freeList.pop();

		out_cpu_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(_descriptorHeap->GetCPUDescriptorHandleForHeapStart(), index, _descriptorSize);
		if (_isShaderVisible) out_gpu_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), index, _descriptorSize);
		else out_gpu_handle.ptr = 0;
		return true;
	}

	if (_currentIndex >= _capacity)
	{
	    CERROR("Descriptor Heap is full!");
	    return false; // 더 이상 할당할 수 없음
	}

	// CPU handle 계산
	out_cpu_handle = _descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	out_cpu_handle.ptr += (_descriptorSize * _currentIndex);

	// GPU handle 계산
	// GPU handle은 힙이 shader-visible일 때만 유효하게 계산, 아니면 0으로 둠
	if (_isShaderVisible)
	{
		out_gpu_handle = _descriptorHeap->GetGPUDescriptorHandleForHeapStart();
		out_gpu_handle.ptr += (_descriptorSize * _currentIndex);
	}
	else
	{
		out_gpu_handle.ptr = 0; // 명시적으로 무효화 (실수 사용 방지)
	}

	_currentIndex++;

	return true;
}

void DescriptorManager::free_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
{
	SIZE_T start_ptr = _descriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	if (cpu_handle.ptr >= start_ptr) {
		UINT index = static_cast<UINT>((cpu_handle.ptr - start_ptr) / _descriptorSize);
		_freeList.push(index);
	}
}

