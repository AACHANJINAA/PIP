#include"stdafx.h"

/*버퍼 리소스를 생성하는 함수이다. 버퍼의 힙 유형에 따라 버퍼 리소스를 생성하고 초기화 데이터가 있으면 초기화
한다.*/
ID3D12Resource* CreateBufferResource(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList
	* pd3dCommandList, void* pData, UINT nBytes, D3D12_HEAP_TYPE d3dHeapType,
	D3D12_RESOURCE_STATES d3dResourceStates, ID3D12Resource** ppd3dUploadBuffer)
{
	ID3D12Resource* pd3dBuffer = NULL;
	D3D12_HEAP_PROPERTIES d3dHeapPropertiesDesc;
	::ZeroMemory(&d3dHeapPropertiesDesc, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapPropertiesDesc.Type = d3dHeapType;
	d3dHeapPropertiesDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapPropertiesDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapPropertiesDesc.CreationNodeMask = 1;
	d3dHeapPropertiesDesc.VisibleNodeMask = 1;
	D3D12_RESOURCE_DESC d3dResourceDesc;
	::ZeroMemory(&d3dResourceDesc, sizeof(D3D12_RESOURCE_DESC));
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	d3dResourceDesc.Alignment = 0; d3dResourceDesc.Width = nBytes;
	d3dResourceDesc.Height = 1;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	d3dResourceDesc.SampleDesc.Count = 1;
	d3dResourceDesc.SampleDesc.Quality = 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_STATES d3dResourceInitialStates = D3D12_RESOURCE_STATE_COPY_DEST;
	if (d3dHeapType == D3D12_HEAP_TYPE_UPLOAD) d3dResourceInitialStates =
		D3D12_RESOURCE_STATE_GENERIC_READ;
	else if (d3dHeapType == D3D12_HEAP_TYPE_READBACK) d3dResourceInitialStates =
		D3D12_RESOURCE_STATE_COPY_DEST;
	HRESULT hResult = pd3dDevice->CreateCommittedResource(&d3dHeapPropertiesDesc,
		D3D12_HEAP_FLAG_NONE, &d3dResourceDesc, d3dResourceInitialStates, NULL,
		__uuidof(ID3D12Resource), (void**)&pd3dBuffer);
	if (pData)
	{
		switch (d3dHeapType)
		{
		case D3D12_HEAP_TYPE_DEFAULT:
		{
			if (ppd3dUploadBuffer)
			{
				//업로드 버퍼를 생성한다. 
				d3dHeapPropertiesDesc.Type = D3D12_HEAP_TYPE_UPLOAD;
				pd3dDevice->CreateCommittedResource(&d3dHeapPropertiesDesc,
					D3D12_HEAP_FLAG_NONE, &d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
					__uuidof(ID3D12Resource), (void**)ppd3dUploadBuffer);

				//업로드 버퍼를 매핑하여 초기화 데이터를 업로드 버퍼에 복사한다. 
				D3D12_RANGE d3dReadRange = { 0, 0 };
				UINT8* pBufferDataBegin = NULL;
				(*ppd3dUploadBuffer)->Map(0, &d3dReadRange, (void**)&pBufferDataBegin);
				memcpy(pBufferDataBegin, pData, nBytes);
				(*ppd3dUploadBuffer)->Unmap(0, NULL);

				//업로드 버퍼의 내용을 디폴트 버퍼에 복사한다.
				pd3dCommandList->CopyResource(pd3dBuffer, *ppd3dUploadBuffer);
				D3D12_RESOURCE_BARRIER d3dResourceBarrier;
				::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
				d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				d3dResourceBarrier.Transition.pResource = pd3dBuffer;
				d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
				d3dResourceBarrier.Transition.StateAfter = d3dResourceStates;
				d3dResourceBarrier.Transition.Subresource =
					D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);
				break;
			}
		}
		case D3D12_HEAP_TYPE_UPLOAD:
		{
			D3D12_RANGE d3dReadRange = { 0, 0 };
			UINT8* pBufferDataBegin = NULL;
			pd3dBuffer->Map(0, &d3dReadRange, (void**)&pBufferDataBegin);
			memcpy(pBufferDataBegin, pData, nBytes);
			pd3dBuffer->Unmap(0, NULL);
			break;
		}
		case D3D12_HEAP_TYPE_READBACK:
			break;
		}
	}
	return(pd3dBuffer);
}

//TODO: 오류 있을 확률 5만 2천 퍼센트
inline void UpdateSubresources(
	ID3D12GraphicsCommandList* pCmdList,
	ID3D12Resource* pDestinationResource,
	ID3D12Resource* pIntermediate,
	UINT64 IntermediateOffset,
	UINT FirstSubresource,
	UINT NumSubresources,
	D3D12_SUBRESOURCE_DATA* pSrcData)
{
	// 리소스의 Description을 가져옵니다.
	D3D12_RESOURCE_DESC DestinationDesc = pDestinationResource->GetDesc();
	ID3D12Device* pDevice;
	pDestinationResource->GetDevice(IID_PPV_ARGS(&pDevice));

	// 복사에 필요한 메모리 레이아웃(크기, 정렬 등)을 계산합니다.
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts =
		(D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)_alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) *
			NumSubresources);
	UINT64* pRowSizesInBytes = (UINT64*)_alloca(sizeof(UINT64) * NumSubresources);
	UINT* pNumRows = (UINT*)_alloca(sizeof(UINT) * NumSubresources);

	UINT64 RequiredSize = 0;

	// [수정] GetDesc()로 얻은 구조체의 주소를 직접 넘겨줍니다.
	pDevice->GetCopyableFootprints(&DestinationDesc, FirstSubresource, NumSubresources,
		IntermediateOffset, pLayouts, pNumRows, pRowSizesInBytes, &RequiredSize);
	pDevice->Release(); // GetDevice로 얻은 참조 카운트를 감소시킵니다.

	// CPU 데이터를 UPLOAD 힙(pIntermediate)으로 복사
	BYTE* pMappedData;
	pIntermediate->Map(0, nullptr, reinterpret_cast<void**>(&pMappedData));
	for (UINT i = 0; i < NumSubresources; ++i)
	{
		D3D12_SUBRESOURCE_DATA SubresourceData = pSrcData[i];
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT& DestFootprint = pLayouts[i];

		BYTE* pDestSlice = pMappedData + DestFootprint.Offset;
		const BYTE* pSrcSlice = (const BYTE*)SubresourceData.pData;

		// 한 줄씩 레이아웃에 맞춰 복사합니다.
		for (UINT y = 0; y < pNumRows[i]; ++y)
		{
			memcpy(pDestSlice + y * DestFootprint.Footprint.RowPitch,
				pSrcSlice + y * SubresourceData.RowPitch,
				pRowSizesInBytes[i]);
		}
	}
	pIntermediate->Unmap(0, nullptr);

	// UPLOAD 힙에서 DEFAULT 힙으로 복사 명령 기록
	if (DestinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		pCmdList->CopyBufferRegion(pDestinationResource, 0, pIntermediate, IntermediateOffset,
			RequiredSize);
	}
	else
	{
		for (UINT i = 0; i < NumSubresources; ++i)
		{
			D3D12_TEXTURE_COPY_LOCATION Dst = { pDestinationResource, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
				FirstSubresource + i };
			D3D12_TEXTURE_COPY_LOCATION Src = { pIntermediate, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
				pLayouts[i] };
			pCmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
		}
	}
}