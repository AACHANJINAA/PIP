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

    // 4개의 텍스처(t0, t1, t2, t3)를 포함하는 하나의 Descriptor Range를 정의합니다.
    CD3DX12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND); // t0부터 시작하는 4개의 SRV

    CD3DX12_ROOT_PARAMETER params[5];

    // 0번 파라미터: 월드 행렬용 CBV
	params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL); // b0
    // 1번 파라미터: 카메라용 CBV
	params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL); // b1
    // 머터리얼 정보를 위한 상수 버퍼 뷰(CBV) 추가
	params[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL); // b2
    // 조명 정보를 위한 상수 버퍼 뷰(CBV) 추가
	params[3].InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_ALL); // b3
     // 파라미터 4: 텍스처를 위한 하나의 Descriptor Table
	params[4].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL); // t0~t3

    d3dRootSignatureDesc.NumParameters = _countof(params);
    d3dRootSignatureDesc.pParameters = params;

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
	CD3DX12_DESCRIPTOR_RANGE ranges[4];

    for (int i = 0; i < 4; ++i)
    {
        ranges[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
    }

    // 2. 셰이더가 사용할 전체 파라미터 목록을 정의 <- 텍스쳐 테이블도 추가됨
	CD3DX12_ROOT_PARAMETER params[8]; // CBV 4개 + SRV 테이블 1개 = 5개

    // 0번 월드 행렬용 CBV
	params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL); // b0
    // 1번 카메라용 CBV
	params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL); // b1
    // 2번 재질용 CBV
	params[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL); // b2
	// 3번 조명용 CBV
	params[3].InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_ALL); // b3

	// 4번 텍스처 디스크립터 테이블
    for (int i = 0; i < 4; ++i)
    {
		params[4 + i].InitAsDescriptorTable(1, &ranges[i], D3D12_SHADER_VISIBILITY_PIXEL); // t0~t3
    }

    d3dRootSignatureDesc.NumParameters = _countof(params);
    d3dRootSignatureDesc.pParameters = params;

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
    CD3DX12_DESCRIPTOR_RANGE ranges[4];
    for (int i = 0; i < 4; ++i)
    {
        ranges[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
    }
    // 2. 셰이더가 사용할 전체 파라미터 목록을 정의 <- 텍스쳐 테이블도 추가됨
    CD3DX12_ROOT_PARAMETER params[9]; // CBV 4개 + SRV 테이블 1개 = 5개

    // 0번 월드 행렬용 CBV
    params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL); // b0
    // 1번 카메라용 CBV
	params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL); // b1
    // 2번 재질용 CBV
	params[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL); // b2
    // 3번 조명용 CBV
	params[3].InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_ALL); // b3

    // 4번 텍스처 디스크립터 테이블
    for (int i = 0; i < 4; ++i)
    {
        int param_index = 4 + i;
        params[param_index].InitAsDescriptorTable(1, &ranges[i], D3D12_SHADER_VISIBILITY_PIXEL); // t0~t3
    }

    // 8번 체력용 CBV
	params[8].InitAsConstantBufferView(8, 1, D3D12_SHADER_VISIBILITY_PIXEL); // b4: 체력

    d3dRootSignatureDesc.NumParameters = _countof(params);
    d3dRootSignatureDesc.pParameters = params;

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
    CD3DX12_DESCRIPTOR_RANGE ranges[4];
    for (int i = 0; i < 4; ++i)
    {
        ranges[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
    }

    // 2. 루트 파라미터 정의 (총 9개)
    // [0~3]: 공통 CBV
    // [4~7]: 공통 텍스처 테이블 (순서 유지!)
    // [8]  : [추가] 스키닝 뼈대 행렬 (맨 뒤로 이동)
    CD3DX12_ROOT_PARAMETER params[9];

    // [0] b0: 월드 행렬
	params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL); // b0
    // [1] b1: 카메라
	params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL); // b1
    // [2] b2: 재질
	params[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL); // b2
    // [3] b3: 조명
	params[3].InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_ALL); // b3
    // [4~7] t0~t3: 텍스처 디스크립터 테이블 (GltfRootSignature와 위치 동일하게 유지)
    for (int i = 0; i < 4; ++i)
    {
        // 파라미터 인덱스 4, 5, 6, 7
        int param_index = 4 + i;
		params[param_index].InitAsDescriptorTable(1, &ranges[i], D3D12_SHADER_VISIBILITY_PIXEL); // t0~t3
    }
    // [8] b4: [추가] 스키닝 뼈대 행렬 (맨 뒤에 추가)
	params[8].InitAsConstantBufferView(4, 0, D3D12_SHADER_VISIBILITY_VERTEX); // b4

    d3dRootSignatureDesc.NumParameters = _countof(params);
    d3dRootSignatureDesc.pParameters = params;

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

    CD3DX12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);


    CD3DX12_ROOT_PARAMETER params[5];

    // [0] b0: 월드 행렬 (CBV)
	params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL); // b0
    // [1] b1: 카메라 (CBV)
	params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL); // b
    // [2] b2: 재질 (CBV) - 스카이박스는 안 써도 칸은 비워둡니다 (호환성)
	params[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL); // b2
    // [3] b3: 조명 (CBV) - 마찬가지로 칸 유지
	params[3].InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_ALL); // b3
    // [4] t0: 텍스처 테이블 (Descriptor Table)
	params[4].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL); // t0

    d3dRootSignatureDesc.NumParameters = _countof(params);
    d3dRootSignatureDesc.pParameters = params;

    D3D12_STATIC_SAMPLER_DESC d3dStaticSamplerDesc = {};

    d3dStaticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

    // [주소 모드] 스카이박스는 끝부분 선(Seam) 방지를 위해 CLAMP가 안전함
    d3dStaticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d3dStaticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    d3dStaticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

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
    CD3DX12_DESCRIPTOR_RANGE ranges[1];

    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

    // Root Parameters
    CD3DX12_ROOT_PARAMETER params[5];
    // [0] b0: World Matrix (cbPerObject)
    params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL); // b0
    // [1] b1: Camera (cbPerFrame)
	params[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL); // b1
    // [2] b2: Terrain Info (cbTerrain)
	params[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL); // b2
    // [3] b3: Light (cbPerLight)
	params[3].InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_ALL); // b3
	// [4] t0~t4: Texture Descriptor Table
    params[4].InitAsDescriptorTable(1, &ranges[0]);

    d3dRootSignatureDesc.NumParameters = _countof(params);
    d3dRootSignatureDesc.pParameters = params;

    // Static Sampler (s0)
    D3D12_STATIC_SAMPLER_DESC d3dStaticSamplerDesc = {};
    d3dStaticSamplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
    d3dStaticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
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


const std::string& UIRootSignatureGenerator::name() const
{
    static const std::string terrainName = "ui";
    return terrainName;
}

ComPtr<ID3D12RootSignature> UIRootSignatureGenerator::create(ID3D12Device* device)
{
    // 루트 파라미터 정의
    CD3DX12_ROOT_PARAMETER1 slot_root_parameter[3];

    // 0: 화면 정보 상수 버퍼 (b0)
    slot_root_parameter[0].InitAsConstantBufferView(0);

    // 1: UI 요소 정보 상수 버퍼 (b1)
    slot_root_parameter[1].InitAsConstantBufferView(1);

    // 2: 텍스처 (t0)
    CD3DX12_DESCRIPTOR_RANGE1 texture_range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    slot_root_parameter[2].InitAsDescriptorTable(1, &texture_range);

    // 샘플러 설정
    CD3DX12_STATIC_SAMPLER_DESC sampler(
        0,                                // register(s0)
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,  // 필터
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // U 주소 모드
        D3D12_TEXTURE_ADDRESS_MODE_WRAP   // V 주소 모드
    );

    // 루트 시그니처 생성
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
    root_signature_desc.Init_1_1(
        3,                      // 파라미터 개수
        slot_root_parameter,    // 파라미터 배열
        1,                      // 샘플러 개수
        &sampler,               // 샘플러 배열
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    ComPtr<ID3DBlob> signature_blob;
    ComPtr<ID3DBlob> error_blob;
    D3DX12SerializeVersionedRootSignature(
        &root_signature_desc,
        D3D_ROOT_SIGNATURE_VERSION_1_1,
        &signature_blob,
        &error_blob
    );

    if (error_blob)
    {
        CERROR("UI Root Signature Serialization Error: "
            << (char*)error_blob->GetBufferPointer());
    }

    ComPtr<ID3D12RootSignature> root_signature;
    device->CreateRootSignature(
        0,
        signature_blob->GetBufferPointer(),
        signature_blob->GetBufferSize(),
        IID_PPV_ARGS(&root_signature)
    );

    return root_signature;
}

const std::string& MonsterHPUIRootSignatureGenerator::name() const
{
    static const std::string monster_HP_UI_name = "Monster_HP_UI";
    return monster_HP_UI_name;
}

ComPtr<ID3D12RootSignature> MonsterHPUIRootSignatureGenerator::create(ID3D12Device* device)
{
    // 루트 파라미터 정의
    CD3DX12_ROOT_PARAMETER1 slot_root_parameter[4];

    // [0] b1: 카메라 정보 (대원님 요청사항: 반드시 b1)
    slot_root_parameter[0].InitAsConstantBufferView(1);

    // [1] b2: HP 바 크기 정보 (g_Size)
    slot_root_parameter[1].InitAsConstantBufferView(2);

    // [수정] 2개의 테이블로 분리 (각각 t0, t1)
    CD3DX12_DESCRIPTOR_RANGE1 range0(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0
    CD3DX12_DESCRIPTOR_RANGE1 range1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1

    slot_root_parameter[2].InitAsDescriptorTable(1, &range0); // 슬롯 2 -> t0
    slot_root_parameter[3].InitAsDescriptorTable(1, &range1); // 슬롯 3 -> t1

    // 3. 정적 샘플러 설정 (s0)
    CD3DX12_STATIC_SAMPLER_DESC linear_sampler(
        0,                                // register(s0)
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP
    );

    // 4. 루트 시그니처 설명자 초기화
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
    root_signature_desc.Init_1_1(
        _countof(slot_root_parameter),
        slot_root_parameter,
        1,
        &linear_sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    // 5. 직렬화 및 생성
    ComPtr<ID3DBlob> signature_blob;
    ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(
        &root_signature_desc,
        D3D_ROOT_SIGNATURE_VERSION_1_1,
        &signature_blob,
        &error_blob
    );

    if (FAILED(hr))
    {
        if (error_blob)
        {
            CERROR("Monster HP UI Root Signature Error: " << (char*)error_blob->GetBufferPointer());
        }
        return nullptr;
    }

    ComPtr<ID3D12RootSignature> root_signature;
    device->CreateRootSignature(
        0,
        signature_blob->GetBufferPointer(),
        signature_blob->GetBufferSize(),
        IID_PPV_ARGS(&root_signature)
    );

    return root_signature;
}
