#include "stdafx.h"
#include "RootSignature.h"

const std::string& DefaultRootSignatureGenerator::name() const
{
	static const std::string defaultName = "default";
	return defaultName;
}

ComPtr<ID3D12RootSignature> DefaultRootSignatureGenerator::create(ID3D12Device* device)
{
    ComPtr<ID3D12RootSignature> pd3dGraphicsRootSignature = NULL;
    D3D12_ROOT_PARAMETER pd3dRootParameters[4];
    // [수정] 0번 파라미터: 월드 행렬용 CBV
    pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    pd3dRootParameters[0].Descriptor.ShaderRegister = 0; // b0
    pd3dRootParameters[0].Descriptor.RegisterSpace = 0;
    pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [수정] 1번 파라미터: 카메라용 CBV
    pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    pd3dRootParameters[1].Descriptor.ShaderRegister = 1; // b1
    pd3dRootParameters[1].Descriptor.RegisterSpace = 0;
    pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // 머터리얼 정보를 위한 상수 버퍼 뷰(CBV) 추가
    pd3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    pd3dRootParameters[2].Descriptor.ShaderRegister = 2; // 셰이더의 b2 레지스터
    pd3dRootParameters[2].Descriptor.RegisterSpace = 0;
    pd3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // 조명 정보를 위한 상수 버퍼 뷰(CBV) 추가
    pd3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    pd3dRootParameters[3].Descriptor.ShaderRegister = 3; // 셰이더의 b3 레지스터
    pd3dRootParameters[3].Descriptor.RegisterSpace = 0;
    pd3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
    // 이 부분에 픽셀 쉐이더 접근안되게 하는거 지움

    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
    d3dRootSignatureDesc.NumParameters = _countof(pd3dRootParameters);
    d3dRootSignatureDesc.pParameters = pd3dRootParameters;
    d3dRootSignatureDesc.NumStaticSamplers = 0;
    d3dRootSignatureDesc.pStaticSamplers = NULL;
    d3dRootSignatureDesc.Flags = d3dRootSignatureFlags;

    ComPtr<ID3DBlob> pd3dSignatureBlob;
    ComPtr<ID3DBlob> pd3dErrorBlob;

    ::D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
    device->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
        pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&pd3dGraphicsRootSignature);

    return pd3dGraphicsRootSignature;
}

const std::string& SkinnedRootSignatureGenerator::name() const
{
    static const std::string skinnedName = "skinned";
	return skinnedName;
}

ComPtr<ID3D12RootSignature> SkinnedRootSignatureGenerator::create(ID3D12Device* device)
{
    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
    d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // 1. 텍스처(SRV)를 위한 디스크립터 테이블 설정
    D3D12_DESCRIPTOR_RANGE d3dDescriptorRanges[1];
    d3dDescriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    d3dDescriptorRanges[0].NumDescriptors = 1; // 텍스처는 1개
    d3dDescriptorRanges[0].BaseShaderRegister = 0; // 셰이더의 t0 레지스터에 연결
    d3dDescriptorRanges[0].RegisterSpace = 0;
    d3dDescriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // 2. 셰이더가 사용할 전체 파라미터 목록을 정의 (총 6개)
    D3D12_ROOT_PARAMETER d3dRootParameters[6];

    // [수정] 0번 파라미터: 월드 행렬용 CBV
    d3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[0].Descriptor.ShaderRegister = 0; // b0
    d3dRootParameters[0].Descriptor.RegisterSpace = 0;
    d3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [수정] 1번 파라미터: 카메라용 CBV
    d3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[1].Descriptor.ShaderRegister = 1; // b1
    d3dRootParameters[1].Descriptor.RegisterSpace = 0;
    d3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    d3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[2].Descriptor.ShaderRegister = 2; // b2: 재질
    d3dRootParameters[2].Descriptor.RegisterSpace = 0;
    d3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    d3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[3].Descriptor.ShaderRegister = 3; // b3: 조명
    d3dRootParameters[3].Descriptor.RegisterSpace = 0;
    d3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [새로운 파라미터] 스키닝 상수 버퍼
    d3dRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[4].Descriptor.ShaderRegister = 4; // b4: 스키닝 뼈 행렬
    d3dRootParameters[4].Descriptor.RegisterSpace = 0;
    d3dRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 버텍스 셰이더에서만 필요

    // [새로운 파라미터] 텍스처 디스크립터 테이블
    d3dRootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    d3dRootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
    d3dRootParameters[5].DescriptorTable.pDescriptorRanges = &d3dDescriptorRanges[0];
    d3dRootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // 픽셀 셰이더에서만 필요

    d3dRootSignatureDesc.NumParameters = _countof(d3dRootParameters);
    d3dRootSignatureDesc.pParameters = d3dRootParameters;

    // 3. 텍스처 샘플러 설정
    D3D12_STATIC_SAMPLER_DESC d3dStaticSamplerDesc = {};
    d3dStaticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    d3dStaticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d3dStaticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d3dStaticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d3dStaticSamplerDesc.MipLODBias = 0;
    d3dStaticSamplerDesc.MaxAnisotropy = 1;
    d3dStaticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    d3dStaticSamplerDesc.MinLOD = 0;
    d3dStaticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    d3dStaticSamplerDesc.ShaderRegister = 0; // 셰이더의 s0 레지스터에 연결
    d3dStaticSamplerDesc.RegisterSpace = 0;
    d3dStaticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    d3dRootSignatureDesc.NumStaticSamplers = 1;
    d3dRootSignatureDesc.pStaticSamplers = &d3dStaticSamplerDesc;

    // 4. 루트 서명 생성
    ComPtr<ID3D12RootSignature> pd3dGraphicsRootSignature = nullptr;
    ComPtr<ID3DBlob> pd3dSignatureBlob, pd3dErrorBlob;
    D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
    device->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pd3dGraphicsRootSignature));

    return pd3dGraphicsRootSignature;
}

const std::string& GltfRootSignatureGenerator::name() const
{
    static const std::string gltfName = "gltf";
	return gltfName;
}

ComPtr<ID3D12RootSignature> GltfRootSignatureGenerator::create(ID3D12Device* device)
{
    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
    d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    //// 1. 텍스처(SRV)를 위한 디스크립터 테이블 설정
    D3D12_DESCRIPTOR_RANGE d3dDescriptorRanges[1];
    d3dDescriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    d3dDescriptorRanges[0].NumDescriptors = 4; // 텍스처는 1개
    d3dDescriptorRanges[0].BaseShaderRegister = 0; // 셰이더의 t0 레지스터에 연결
    d3dDescriptorRanges[0].RegisterSpace = 0;
    d3dDescriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // 2. 셰이더가 사용할 전체 파라미터 목록을 정의 <- 텍스쳐 테이블도 추가됨
	D3D12_ROOT_PARAMETER d3dRootParameters[5]; // CBV 4개 + SRV 테이블 1개 = 5개

    // 0번 월드 행렬용 CBV
    d3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[0].Descriptor.ShaderRegister = 0; // b0
    d3dRootParameters[0].Descriptor.RegisterSpace = 0;
    d3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // 1번 카메라용 CBV
    d3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[1].Descriptor.ShaderRegister = 1; // b1
    d3dRootParameters[1].Descriptor.RegisterSpace = 0;
    d3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // 2번 재질용 CBV
    d3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[2].Descriptor.ShaderRegister = 2; // b2: 재질
    d3dRootParameters[2].Descriptor.RegisterSpace = 0;
    d3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// 3번 조명용 CBV
    d3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[3].Descriptor.ShaderRegister = 3; // b3: 조명
    d3dRootParameters[3].Descriptor.RegisterSpace = 0;
    d3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    //// [새로운 파라미터] 스키닝 상수 버퍼
    //d3dRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    //d3dRootParameters[4].Descriptor.ShaderRegister = 4; // b4: 스키닝 뼈 행렬
    //d3dRootParameters[4].Descriptor.RegisterSpace = 0;
    //d3dRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 버텍스 셰이더에서만 필요

	// 4번 텍스처 디스크립터 테이블
    d3dRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    d3dRootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
    d3dRootParameters[4].DescriptorTable.pDescriptorRanges = &d3dDescriptorRanges[0];
    d3dRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // 픽셀 셰이더에서만 필요

    d3dRootSignatureDesc.NumParameters = _countof(d3dRootParameters);
    d3dRootSignatureDesc.pParameters = d3dRootParameters;

    //// 3. 텍스처 샘플러 설정
    D3D12_STATIC_SAMPLER_DESC d3dStaticSamplerDesc = {};
    d3dStaticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    d3dStaticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d3dStaticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d3dStaticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d3dStaticSamplerDesc.MipLODBias = 0;
    d3dStaticSamplerDesc.MaxAnisotropy = 1;
    d3dStaticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    d3dStaticSamplerDesc.MinLOD = 0;
    d3dStaticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    d3dStaticSamplerDesc.ShaderRegister = 0; // 셰이더의 s0 레지스터에 연결
    d3dStaticSamplerDesc.RegisterSpace = 0;
    d3dStaticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    d3dRootSignatureDesc.NumStaticSamplers = 1;
    d3dRootSignatureDesc.pStaticSamplers = &d3dStaticSamplerDesc;

    // 4. 루트 서명 생성
    ComPtr<ID3D12RootSignature> pd3dGraphicsRootSignature = nullptr;
    ComPtr<ID3DBlob> pd3dSignatureBlob, pd3dErrorBlob;
    D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
    device->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pd3dGraphicsRootSignature));

    return pd3dGraphicsRootSignature;
}
