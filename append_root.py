import os

with open('Client/Client/RootSignature.cpp', 'a', encoding='utf-8-sig') as f:
    f.write('''

const std::string& BillboardUIRootSignatureGenerator::name() const
{
    static const std::string name = "billboard_ui";
    return name;
}

ComPtr<ID3D12RootSignature> BillboardUIRootSignatureGenerator::create(ID3D12Device* device)
{
    CD3DX12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0: 텍스처

    CD3DX12_ROOT_PARAMETER rootParameters[3];
    // b2: cbMarker (마커 상수 버퍼) -> Root Parameter 0
    rootParameters[0].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL);
    // b1: cbCamera (카메라 상수 버퍼) -> Root Parameter 1
    rootParameters[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);
    // t0: 텍스처 테이블 -> Root Parameter 2
    rootParameters[2].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC samplerDesc(
        0, // 쉐이더 레지스터 번호
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // 필터
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP   // addressW
    );

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &samplerDesc,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
    {
        if (error != nullptr)
        {
            OutputDebugStringA((char*)error->GetBufferPointer());
        }
        return nullptr;
    }

    ComPtr<ID3D12RootSignature> rootSignature;
    device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    return rootSignature;
}
''')
