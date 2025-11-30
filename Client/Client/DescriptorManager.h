#pragma once

class DescriptorManager : public Singleton<DescriptorManager>
{
	friend Singleton<DescriptorManager>;

	DescriptorManager() = default;
	~DescriptorManager() = default;
public:
	void initialize(ID3D12Device* device, UINT descriptor_count=2048);

	// 사용 가능한 디스크립터 핸들(CPU / GPU 쌍)을 할당
	bool allocate_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE & out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE & out_gpu_handle);

	// 디스크립터 힙 반환
	ID3D12DescriptorHeap* get_descriptor_heap() const { return _descriptorHeap.Get(); }
	UINT get_descriptor_size() const { return _descriptorSize; }

private:
	ComPtr<ID3D12DescriptorHeap> _descriptorHeap;
	UINT _descriptorSize = 0;
	UINT _capacity = 0;
	UINT _currentIndex = 0;
	bool _isShaderVisible = false;

	// TODO: 추후 free_descriptor 구현을 위해 free list 등을 추가 가능
};
