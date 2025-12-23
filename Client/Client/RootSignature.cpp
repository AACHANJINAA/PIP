#include "stdafx.h"
#include "RootSignature.h"

const std::string& DefaultRootSignatureGenerator::name() const
{
	static const std::string defaultName = "default";
	return defaultName;
}

ComPtr<ID3D12RootSignature> DefaultRootSignatureGenerator::create(ID3D12Device* device)
{
	D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
	::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
	d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // [수정] 4개의 텍스처(t0, t1, t2, t3)를 포함하는 하나의 Descriptor Range를 정의합니다.
    D3D12_DESCRIPTOR_RANGE d3dDescriptorRanges[1];
    d3dDescriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    d3dDescriptorRanges[0].NumDescriptors = 4; // 텍스처 4개를 사용합니다.
    d3dDescriptorRanges[0].BaseShaderRegister = 0; // t0 레지스터에서 시작합니다.
    d3dDescriptorRanges[0].RegisterSpace = 0;
    d3dDescriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER d3dRootParameters[5];
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

    // 머터리얼 정보를 위한 상수 버퍼 뷰(CBV) 추가
    d3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[2].Descriptor.ShaderRegister = 2; // 셰이더의 b2 레지스터
    d3dRootParameters[2].Descriptor.RegisterSpace = 0;
    d3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // 조명 정보를 위한 상수 버퍼 뷰(CBV) 추가
    d3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[3].Descriptor.ShaderRegister = 3; // 셰이더의 b3 레지스터
    d3dRootParameters[3].Descriptor.RegisterSpace = 0;
    d3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

     // [수정] 파라미터 4: 텍스처를 위한 하나의 Descriptor Table
    d3dRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    d3dRootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
    d3dRootParameters[4].DescriptorTable.pDescriptorRanges = d3dDescriptorRanges;
    d3dRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    d3dRootSignatureDesc.NumParameters = _countof(d3dRootParameters);
    d3dRootSignatureDesc.pParameters = d3dRootParameters;

    // 정적 샘플러 설정 (기존과 동일)
    D3D12_STATIC_SAMPLER_DESC d3dStaticSamplerDesc = {};
    d3dStaticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    d3dStaticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    d3dStaticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    d3dStaticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    d3dStaticSamplerDesc.MipLODBias = 0;
    d3dStaticSamplerDesc.MaxAnisotropy = 16;
    d3dStaticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    d3dStaticSamplerDesc.MinLOD = 0;
    d3dStaticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    d3dStaticSamplerDesc.ShaderRegister = 0; // s0 레지스터
    d3dStaticSamplerDesc.RegisterSpace = 0;
    d3dStaticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    
    d3dRootSignatureDesc.NumStaticSamplers = 1;
    d3dRootSignatureDesc.pStaticSamplers = &d3dStaticSamplerDesc;
    
    // 루트 시그니처 생성 (기존과 동일)
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
    D3D12_DESCRIPTOR_RANGE d3dDescriptorRanges[4];
    for (int i = 0; i < 4; ++i)
    {
        d3dDescriptorRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        d3dDescriptorRanges[i].NumDescriptors = 1; // 텍스처는 1개
        d3dDescriptorRanges[i].BaseShaderRegister = i; // 셰이더의 t0 레지스터에 연결
        d3dDescriptorRanges[i].RegisterSpace = 0;
        d3dDescriptorRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }
    // 2. 셰이더가 사용할 전체 파라미터 목록을 정의 <- 텍스쳐 테이블도 추가됨
	D3D12_ROOT_PARAMETER d3dRootParameters[8]; // CBV 4개 + SRV 테이블 1개 = 5개

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
    for (int i = 0; i < 4; ++i)
    {
        d3dRootParameters[4 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        d3dRootParameters[4 + i].DescriptorTable.NumDescriptorRanges = 1;
        d3dRootParameters[4 + i].DescriptorTable.pDescriptorRanges = &d3dDescriptorRanges[i];
        d3dRootParameters[4 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // 픽셀 셰이더에서만 필요
    }

    d3dRootSignatureDesc.NumParameters = _countof(d3dRootParameters);
    d3dRootSignatureDesc.pParameters = d3dRootParameters;

    //// 3. 텍스처 샘플러 설정
    D3D12_STATIC_SAMPLER_DESC d3dStaticSamplerDesc = {};
    d3dStaticSamplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
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

const std::string& GltfHpRootSignatureGenerator::name() const
{
    static const std::string gltfName = "gltf_hp";
    return gltfName;
}

ComPtr<ID3D12RootSignature> GltfHpRootSignatureGenerator::create(ID3D12Device* device)
{
    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
    d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    //// 1. 텍스처(SRV)를 위한 디스크립터 테이블 설정
    D3D12_DESCRIPTOR_RANGE d3dDescriptorRanges[4];
    for (int i = 0; i < 4; ++i)
    {
        d3dDescriptorRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        d3dDescriptorRanges[i].NumDescriptors = 1; // 텍스처는 1개
        d3dDescriptorRanges[i].BaseShaderRegister = i; // 셰이더의 t0 레지스터에 연결
        d3dDescriptorRanges[i].RegisterSpace = 0;
        d3dDescriptorRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }
    // 2. 셰이더가 사용할 전체 파라미터 목록을 정의 <- 텍스쳐 테이블도 추가됨
    D3D12_ROOT_PARAMETER d3dRootParameters[9]; // CBV 4개 + SRV 테이블 1개 = 5개

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

    // 4번 텍스처 디스크립터 테이블
    for (int i = 0; i < 4; ++i)
    {
        d3dRootParameters[4 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        d3dRootParameters[4 + i].DescriptorTable.NumDescriptorRanges = 1;
        d3dRootParameters[4 + i].DescriptorTable.pDescriptorRanges = &d3dDescriptorRanges[i];
        d3dRootParameters[4 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // 픽셀 셰이더에서만 필요
    }

    // 8번 체력용 CBV
    d3dRootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[8].Descriptor.ShaderRegister = 8; // b4: 체력
    d3dRootParameters[8].Descriptor.RegisterSpace = 1;
    d3dRootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // 픽셀 쉐이더에서만 볼거임

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

    // 1. 텍스처(SRV)를 위한 디스크립터 범위 설정 (t0 ~ t3)
    D3D12_DESCRIPTOR_RANGE d3dDescriptorRanges[4];
    for (int i = 0; i < 4; ++i)
    {
        d3dDescriptorRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        d3dDescriptorRanges[i].NumDescriptors = 1;
        d3dDescriptorRanges[i].BaseShaderRegister = i; // t0, t1, t2, t3
        d3dDescriptorRanges[i].RegisterSpace = 0;
        d3dDescriptorRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }

    // 2. 루트 파라미터 정의 (총 9개)
    // [0~3]: 공통 CBV
    // [4~7]: 공통 텍스처 테이블 (순서 유지!)
    // [8]  : [추가] 스키닝 뼈대 행렬 (맨 뒤로 이동)
    D3D12_ROOT_PARAMETER d3dRootParameters[9];

    // [0] b0: 월드 행렬
    d3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[0].Descriptor.ShaderRegister = 0;
    d3dRootParameters[0].Descriptor.RegisterSpace = 0;
    d3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [1] b1: 카메라
    d3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[1].Descriptor.ShaderRegister = 1;
    d3dRootParameters[1].Descriptor.RegisterSpace = 0;
    d3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [2] b2: 재질
    d3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[2].Descriptor.ShaderRegister = 2;
    d3dRootParameters[2].Descriptor.RegisterSpace = 0;
    d3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [3] b3: 조명
    d3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[3].Descriptor.ShaderRegister = 3;
    d3dRootParameters[3].Descriptor.RegisterSpace = 0;
    d3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // -----------------------------------------------------------------------
    // [4~7] t0~t3: 텍스처 디스크립터 테이블 (GltfRootSignature와 위치 동일하게 유지)
    // -----------------------------------------------------------------------
    for (int i = 0; i < 4; ++i)
    {
        // 파라미터 인덱스 4, 5, 6, 7
        int rootParamIndex = 4 + i;
        d3dRootParameters[rootParamIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        d3dRootParameters[rootParamIndex].DescriptorTable.NumDescriptorRanges = 1;
        d3dRootParameters[rootParamIndex].DescriptorTable.pDescriptorRanges = &d3dDescriptorRanges[i];
        d3dRootParameters[rootParamIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }

    // -----------------------------------------------------------------------
    // [8] b4: [추가] 스키닝 뼈대 행렬 (맨 뒤에 추가)
    // -----------------------------------------------------------------------
    d3dRootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[8].Descriptor.ShaderRegister = 4; // 레지스터는 여전히 b4
    d3dRootParameters[8].Descriptor.RegisterSpace = 0;
    d3dRootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VS 전용

    d3dRootSignatureDesc.NumParameters = _countof(d3dRootParameters);
    d3dRootSignatureDesc.pParameters = d3dRootParameters;

    // 3. 정적 샘플러 설정 (기존과 동일)
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
    d3dStaticSamplerDesc.ShaderRegister = 0;
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////skybox///////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


const std::string& SkyBoxRootSignatureGenerator::name() const
{
    static const std::string skyboxName = "skybox";
    return skyboxName;
}

ComPtr<ID3D12RootSignature> SkyBoxRootSignatureGenerator::create(ID3D12Device* device)
{
    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
    d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_DESCRIPTOR_RANGE d3dDescriptorRanges[1];
    d3dDescriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    d3dDescriptorRanges[0].NumDescriptors = 1; // 텍스처 1개
    d3dDescriptorRanges[0].BaseShaderRegister = 0; // t0 레지스터
    d3dDescriptorRanges[0].RegisterSpace = 0;
    d3dDescriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER d3dRootParameters[5];

    // [0] b0: 월드 행렬 (CBV)
    d3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[0].Descriptor.ShaderRegister = 0;
    d3dRootParameters[0].Descriptor.RegisterSpace = 0;
    d3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [1] b1: 카메라 (CBV)
    d3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[1].Descriptor.ShaderRegister = 1;
    d3dRootParameters[1].Descriptor.RegisterSpace = 0;
    d3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [2] b2: 재질 (CBV) - 스카이박스는 안 써도 칸은 비워둡니다 (호환성)
    d3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[2].Descriptor.ShaderRegister = 2;
    d3dRootParameters[2].Descriptor.RegisterSpace = 0;
    d3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [3] b3: 조명 (CBV) - 마찬가지로 칸 유지
    d3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[3].Descriptor.ShaderRegister = 3;
    d3dRootParameters[3].Descriptor.RegisterSpace = 0;
    d3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [4] t0: 텍스처 테이블 (Descriptor Table)
    d3dRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    d3dRootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
    d3dRootParameters[4].DescriptorTable.pDescriptorRanges = d3dDescriptorRanges;
    d3dRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    d3dRootSignatureDesc.NumParameters = _countof(d3dRootParameters);
    d3dRootSignatureDesc.pParameters = d3dRootParameters;

    D3D12_STATIC_SAMPLER_DESC d3dStaticSamplerDesc = {};

    d3dStaticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

    // [주소 모드] 스카이박스는 끝부분 선(Seam) 방지를 위해 CLAMP가 안전함
    d3dStaticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    d3dStaticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    d3dStaticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

    d3dStaticSamplerDesc.MipLODBias = 0;
    d3dStaticSamplerDesc.MaxAnisotropy = 1; // LINEAR 쓸 땐 1, ANISOTROPIC 쓸 땐 16
    d3dStaticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    d3dStaticSamplerDesc.MinLOD = 0;
    d3dStaticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

    // [레지스터] 셰이더의 register(s0)와 연결
    d3dStaticSamplerDesc.ShaderRegister = 0;
    d3dStaticSamplerDesc.RegisterSpace = 0;
    d3dStaticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    d3dRootSignatureDesc.NumStaticSamplers = 1;
    d3dRootSignatureDesc.pStaticSamplers = &d3dStaticSamplerDesc;

    ComPtr<ID3D12RootSignature> pd3dGraphicsRootSignature = nullptr;
    ComPtr<ID3DBlob> pd3dSignatureBlob, pd3dErrorBlob;

    D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);

    if (pd3dErrorBlob)
    {
        OutputDebugStringA((char*)pd3dErrorBlob->GetBufferPointer());
    }

    device->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pd3dGraphicsRootSignature));

    return pd3dGraphicsRootSignature;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////Terrain//////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const std::string& TerrainRootSignatureGenerator::name() const
{
    static const std::string terrainName = "terrain";
    return terrainName;
}

ComPtr<ID3D12RootSignature> TerrainRootSignatureGenerator::create(ID3D12Device* device)
{
    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
    d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Descriptor Range for Textures (t0, t1, t2, t3, t4)
    D3D12_DESCRIPTOR_RANGE d3dDescriptorRanges[1];

    d3dDescriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    d3dDescriptorRanges[0].NumDescriptors = 5;
	d3dDescriptorRanges[0].BaseShaderRegister = 0; // t0, t1, t2, t3, t4
    d3dDescriptorRanges[0].RegisterSpace = 0;
    d3dDescriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    // Root Parameters
    D3D12_ROOT_PARAMETER d3dRootParameters[5];

    // [0] b0: World Matrix (cbPerObject)
    d3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[0].Descriptor.ShaderRegister = 0;
    d3dRootParameters[0].Descriptor.RegisterSpace = 0;
    d3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [1] b1: Camera (cbPerFrame)
    d3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[1].Descriptor.ShaderRegister = 1;
    d3dRootParameters[1].Descriptor.RegisterSpace = 0;
    d3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [2] b2: Terrain Info (cbTerrain)
    d3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[2].Descriptor.ShaderRegister = 2;
    d3dRootParameters[2].Descriptor.RegisterSpace = 0;
    d3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    d3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;                                                         
    d3dRootParameters[3].Descriptor.ShaderRegister = 3;                                                                         
    d3dRootParameters[3].Descriptor.RegisterSpace = 0;                                                                          
    d3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    d3dRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    d3dRootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
    d3dRootParameters[4].DescriptorTable.pDescriptorRanges = d3dDescriptorRanges;
    d3dRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    d3dRootSignatureDesc.NumParameters = _countof(d3dRootParameters);
    d3dRootSignatureDesc.pParameters = d3dRootParameters;

    // Static Sampler (s0)
    D3D12_STATIC_SAMPLER_DESC d3dStaticSamplerDesc = {};
    d3dStaticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    d3dStaticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d3dStaticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d3dStaticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d3dStaticSamplerDesc.MipLODBias = 0;
    d3dStaticSamplerDesc.MaxAnisotropy = 16;
    d3dStaticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    d3dStaticSamplerDesc.MinLOD = 0;
    d3dStaticSamplerDesc.ShaderRegister = 0;
    d3dStaticSamplerDesc.RegisterSpace = 0;
    d3dStaticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    d3dRootSignatureDesc.NumStaticSamplers = 1;
    d3dRootSignatureDesc.pStaticSamplers = &d3dStaticSamplerDesc;

    // Create Root Signature
    ComPtr<ID3D12RootSignature> pd3dGraphicsRootSignature = nullptr;
    ComPtr<ID3DBlob> pd3dSignatureBlob, pd3dErrorBlob;
    D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
    device->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&pd3dGraphicsRootSignature));

    return pd3dGraphicsRootSignature;
}