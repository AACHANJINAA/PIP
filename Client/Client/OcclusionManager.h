#pragma once
#include "stdafx.h"

class OcclusionManager : public Singleton<OcclusionManager> {
    friend Singleton<OcclusionManager>;
private:
    OcclusionManager() = default;
public:
    void initialize(ID3D12Device* device, UINT max_objects = 2000);

    // N-1 프레임 결과(읽기용): 조건부 렌더링에 사용
    ID3D12Resource* get_result_buffer_for_predication(UINT frame_index) const {
        return _resultBuffers[(frame_index + 1) % SWAP_CHAIN_BUFFERS].Get();
    }
    // N 프레임 결과(쓰기용): 이번 프레임 결과 저장
    ID3D12Resource* get_result_buffer_for_resolve(UINT frame_index) const {
        return _resultBuffers[frame_index % SWAP_CHAIN_BUFFERS].Get();
    }

    ID3D12QueryHeap* get_query_heap() const { return _queryHeap.Get(); }
	UINT get_active_index_count() const { return _nextAvailableIndex - static_cast<UINT>(_freeIndices.size()); }

    UINT allocate_query_index();
    void release_query_index(UINT index);
    void resolve_queries(ID3D12GraphicsCommandList* cmdList, UINT frame_index);
    void set_max_query_index_this_frame(UINT index) { _maxQueryIndexThisFrame = std::max(_maxQueryIndexThisFrame, index); }
    void reset_max_query_index() { _maxQueryIndexThisFrame = 0; }

private:
    ComPtr<ID3D12QueryHeap> _queryHeap;
    std::array<ComPtr<ID3D12Resource>, SWAP_CHAIN_BUFFERS> _resultBuffers;
    std::vector<UINT> _freeIndices;
    UINT _nextAvailableIndex = 0;
    UINT _maxObjects = 0;
    UINT _maxQueryIndexThisFrame = 0;
};