#include "stdafx.h"
#include "LinearAllocator.h"

LinearAllocator::LinearAllocator(ID3D12Device* device, size_t totalSize, UINT frameCount)
{
	// 256바이트 정렬이 가능하도록 전체 사이즈 보정
	_totalSize = (totalSize + CB_ALIGNMENT) & ~CB_ALIGNMENT;

	// 전체 공간을 프레임 개수(SWAP_CHAIN_BUFFERS)만큼 균등 분할
	_frameSize = _totalSize / frameCount;
	_frameSize = (_frameSize + CB_ALIGNMENT) & ~CB_ALIGNMENT; // 프레임 사이즈도 256 정렬

	// CPU가 매 프레임 데이터를 덮어쓸 것이므로 UPLOAD 힙 사용
	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(_totalSize);

	HRESULT hr = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&_resource)
	);
	_ASSERTE(SUCCEEDED(hr));

	_resource->SetName(L"LinearAllocator_UploadBuffer");

	// CPU 매핑 (Map은 딱 한 번만 호출하고 프로그램 종료 시까지 유지합니다)
	CD3DX12_RANGE readRange(0, 0); // CPU가 읽지는 않으므로 (0, 0)
	hr = _resource->Map(0, &readRange, &_cpuBase);
	_ASSERTE(SUCCEEDED(hr));

	// GPU 가상 주소 획득
	_gpuBase = _resource->GetGPUVirtualAddress();
}

LinearAllocator::~LinearAllocator()
{
	if (_resource)
	{
		_resource->Unmap(0, nullptr);
		_cpuBase = nullptr;
	}
}

LinearAllocator::Allocation LinearAllocator::allocate(size_t size)
{
	// 할당할 크기를 256바이트 배수로 올림 연산
	size_t alignedSize = (size + CB_ALIGNMENT) & ~CB_ALIGNMENT;

	// 현재 프레임에 할당된 메모리 범위를 초과하는지 검사 (메모리 부족 방어)
	size_t frameMaxBoundary = (_currentFrameIndex + 1) * _frameSize;
	if (_currentOffset + alignedSize > frameMaxBoundary)
	{
		// 이 로그가 뜬다면 초기 설정한 32MB 용량이 부족하다는 뜻
		CERROR("LinearAllocator Out of Memory in current frame!");
		return { nullptr, 0 };
	}

	// 할당할 위치 계산
	Allocation alloc;
	alloc.cpuPtr = static_cast<uint8_t*>(_cpuBase) + _currentOffset;
	alloc.gpuAddr = _gpuBase + _currentOffset;

	// 오프셋 증가 (다음 NPC가 쓸 주소 갱신)
	_currentOffset += alignedSize;

	return alloc;
}

void LinearAllocator::reset(UINT frameIndex)
{
	_currentFrameIndex = frameIndex;
	// 이번 프레임 전용 메모리 구역의 시작점으로 오프셋 점프
	_currentOffset = frameIndex * _frameSize;
}