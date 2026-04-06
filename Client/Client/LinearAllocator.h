#pragma once
#include "stdafx.h"

// DX12의 상수 버퍼는 무조건 256바이트 단위로 정렬되어야 함
#define CB_ALIGNMENT 255 

class LinearAllocator
{
public:
	// 할당 시 CPU에서 데이터를 복사할 주소와 GPU가 렌더링할 때 읽을 주소를 함께 반환
	struct Allocation
	{
		void* cpuPtr;
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddr;
	};

	LinearAllocator(ID3D12Device* device, size_t totalSize, UINT frameCount);
	~LinearAllocator();

	// 요청한 크기만큼 메모리를 할당 (256바이트 정렬 적용)
	Allocation allocate(size_t size);

	// 매 프레임 시작 시 호출하여, 현재 프레임에 할당된 메모리 구간의 처음으로 오프셋을 리셋
	void reset(UINT frameIndex); // 프레임 인덱스를 받아야 함 -> 우리 게임이 다중 프레임 버퍼링이기 때문

	size_t get_total_size() const { return _totalSize; }

private:
	ComPtr<ID3D12Resource> _resource;

	void* _cpuBase = nullptr;					// CPU 매핑 시작 주소
	D3D12_GPU_VIRTUAL_ADDRESS _gpuBase = 0;		// GPU 가상 주소 시작 지점

	size_t _totalSize = 0;			// 전체 할당된 메모리 크기 (예: 32MB)
	size_t _frameSize = 0;			// 1프레임당 사용 가능한 최대 메모리 크기 (예: 16MB)

	size_t _currentOffset = 0;		// 현재 사용 중인 오프셋
	size_t _currentFrameIndex = 0;	// 현재 렌더링 중인 프레임 인덱스
};