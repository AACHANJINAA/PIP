#include "stdafx.h"
#include "OcclusionManager.h"

void OcclusionManager::initialize(ID3D12Device* device, UINT max_objects) {
    _maxObjects = max_objects;
    _nextAvailableIndex = 0;

    D3D12_QUERY_HEAP_DESC heapDesc = { D3D12_QUERY_HEAP_TYPE_OCCLUSION, max_objects, 0 };
    if (FAILED(device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&_queryHeap)))) {
        CERROR("Failed to Create Occlusion Query Heap!");
    }

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * max_objects);

    for (int i = 0; i < SWAP_CHAIN_BUFFERS; ++i) {
        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
			D3D12_RESOURCE_STATE_PREDICATION,
            nullptr,
            IID_PPV_ARGS(&_resultBuffers[i])
        );

        if (FAILED(hr)) {
            CERROR("Failed to create Occlusion Result Buffer!");
            return;
        }
    }
}

UINT OcclusionManager::allocate_query_index() {
    if (!_freeIndices.empty()) {
        UINT idx = _freeIndices.back();
        _freeIndices.pop_back();
        return idx;
    }
    if (_nextAvailableIndex >= _maxObjects) {
        CERROR("Occlusion Query Heap is Full!");
        return 0;
    }

    return _nextAvailableIndex++;
}

void OcclusionManager::release_query_index(UINT index) {
    if (index != 0xFFFFFFFF) {
        _freeIndices.push_back(index);
    }
}

void OcclusionManager::resolve_queries(ID3D12GraphicsCommandList* cmdList, UINT frame_index) {
    if (_maxQueryIndexThisFrame == 0) return;

    ID3D12Resource* buffer = get_result_buffer_for_resolve(frame_index);
    if (!_queryHeap || !buffer) return;

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer, D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &barrier);

    cmdList->ResolveQueryData(_queryHeap.Get(), D3D12_QUERY_TYPE_OCCLUSION, 0, _nextAvailableIndex, buffer, 0);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PREDICATION);
    cmdList->ResourceBarrier(1, &barrier);

    reset_max_query_index();
}