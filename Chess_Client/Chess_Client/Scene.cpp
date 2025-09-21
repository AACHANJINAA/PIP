#include "stdafx.h"
#include "Scene.h"
#include "ObjectManager.h"
Scene::Scene()
{

}

Scene::~Scene()
{
}

bool Scene::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{

    // 이 함수는 마우스를 입력하면 바로 실행됨

    switch (nMessageID)
    {
    case WM_LBUTTONDOWN: // 왼쪽 마우스 입력
        GameObject* m_pLockedObject;

        m_pLockedObject = PickObjectPointedByCursor(LOWORD(lParam), HIWORD(lParam));

        if (m_pLockedObject)
        {
            m_pLockedObject->_isCollided = true;
        }

        break;
    }

    return(false);
}

bool Scene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
    return(false);
}

void Scene::LoadSceneFromFile(const std::string& filename, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
    // 1. 파일 스트림 열기
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Fatal Error: Failed to open scene file: " << filename << std::endl;
        return;
    }

    try
    {
        // 2. JSON 파싱
        json sceneJson;
        file >> sceneJson; // 이 부분에서 JSON 형식이 아니면 예외 발생
        file.close();

        // 3. JSON 배열 순회
        for (const auto& objectData : sceneJson)
        {
            // .at() 함수를 사용하면 키가 없을 때 예외가 발생하여 catch에서 처리 가능
            std::string name = objectData.at("Name");
            std::string meshName = objectData.at("Mesh");

            // Transform 정보 파싱
            const auto& transformData = objectData.at("Transform");
            XMFLOAT3 location = JsonHelper::ParseVector3(transformData.at("Location"));
            XMFLOAT3 rotation = JsonHelper::ParseRotation(transformData.at("Rotation"));
            XMFLOAT3 scale = JsonHelper::ParseVector3(transformData.at("Scale"));

            // 텍스처 정보 파싱 (선택적일 수 있으므로 .contains로 확인)
            if (objectData.contains("Textures")) {
                for (const auto& texturePath : objectData["Textures"]) {
                    // TODO: 텍스처 경로 처리 로직
                }
            }
            // TODO: 게임 오브젝트 생성 및 배치 로직

            std::shared_ptr<GameObject> Board = std::make_shared<BoardCube>();
            std::shared_ptr<Mesh> BoardMesh;
            meshName = "Resource/MapData/" + meshName + ".glb";

            auto Board_Transform = Board->get_component<TransformComponent>();
            auto Board_Render = Board->get_component<RenderComponent>();

            BoardMesh = make_shared<ReadGlbMesh>(pd3dDevice, pd3dCommandList, meshName, (Scene*)this);

            if (!BoardMesh || !BoardMesh->IsValid())
                continue;

            if (Board_Render)
            {
                auto materialShader = std::make_shared<Material_Shader>();
                Board_Render->set_material(materialShader);

                Board_Render->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가
                Board_Render->set_mesh(BoardMesh);
                Board_Render->set_shader(_AllShaders[1]); // GLB
                Board_Render->get_material_shader()->set_shader_root_signature(_AllRootSignature[1].Get());
            }

            if (Board_Transform)
            {
                Board_Transform->rotate(rotation.x, -rotation.y, rotation.z);
                Board_Transform->set_scale(scale.x, scale.y, scale.z);
                Board_Transform->set_position(location.x, location.y, location.z);
            }
            ObjectManager::Instance()->PushFloorObject(Board);

        }
    }
    catch (const json::exception& e)
    {
        // JSON 파싱 또는 데이터 접근 중 발생하는 모든 종류의 에러를 여기서 처리
        std::cerr << "Scene file load error: " << e.what() << std::endl;
        return;
    }
}

// (수정) 머터리얼 + 조명 파라미터추가
ID3D12RootSignature* Scene::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
    ID3D12RootSignature* pd3dGraphicsRootSignature = NULL;
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
    pd3dDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
        pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&pd3dGraphicsRootSignature);

    return(pd3dGraphicsRootSignature);
}

ID3D12RootSignature* Scene::CreateSkinnedGraphicsRootSignature(ID3D12Device* pd3dDevice)
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
    ID3D12RootSignature* pd3dGraphicsRootSignature = nullptr;
    ComPtr<ID3DBlob> pd3dSignatureBlob, pd3dErrorBlob;
    D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
    pd3dDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pd3dGraphicsRootSignature));

    return pd3dGraphicsRootSignature;
}

ID3D12RootSignature* Scene::GetGraphicsRootSignature()
{
    //return(m_pd3dGraphicsRootSignature);
    return (nullptr);
}

// 핸들 할당 함수 구현
void Scene::AllocateNextSrvDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& outGpuHandle)
{
    outCpuHandle = _SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    outCpuHandle.ptr += (_SrvDescriptorIncrementSize * _AllocatedSrvCount);

    outGpuHandle = _SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    outGpuHandle.ptr += (_SrvDescriptorIncrementSize * _AllocatedSrvCount);

    _AllocatedSrvCount++; // 카운터 증가
}

GameObject* Scene::PickObjectPointedByCursor(int xClient, int yClient)
{
    XMFLOAT3 xmf3PickPosition = {0.0f, 0.0f, 0.0f};
    xmf3PickPosition.x = (((2.0f * xClient) / (float)m_pCamera->m_d3dViewport.Width) - 1) / m_pCamera->m_xmf4x4Projection._11;
    xmf3PickPosition.y = -(((2.0f * yClient) / (float)m_pCamera->m_d3dViewport.Height) - 1) / m_pCamera->m_xmf4x4Projection._22;
    xmf3PickPosition.z = 1.0f;

    XMVECTOR xmvPickPosition = XMLoadFloat3(&xmf3PickPosition);
    XMMATRIX xmmtxView = XMLoadFloat4x4(&m_pCamera->m_xmf4x4View);

    bool nIntersected = false;
    float fNearestHitDistance = FLT_MAX;
    GameObject* pNearestObject = NULL;


    std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& Arr = ObjectManager::Instance()->GetAllObject();

    if (Arr.size()) {
        int i = 0;
        for (auto& Objects : Arr) {
            for (auto& Object : Objects) {
                float fHitDistance = FLT_MAX;
                nIntersected = Object.get()->pick_model_obb(xmvPickPosition, xmmtxView, &fHitDistance);
                if (nIntersected && (fHitDistance < fNearestHitDistance))
                {
                    fNearestHitDistance = fHitDistance;
                    pNearestObject = Object.get();
                }
            }
            ++i;
            if (4 == i) {
                break;
            }
            
        }
    }

    return(pNearestObject);
    return nullptr;
}

void Scene::MakeDummyBonebuffer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
    UINT nBoneCount = 128;
    UINT nBufferSize = sizeof(XMFLOAT4X4) * nBoneCount;

    // UPLOAD 힙에 생성하여 CPU가 항상 접근 가능하도록 합니다.
    // 내용은 비어있어도 되지만, 0으로 초기화해두면 더 안정적입니다.
    m_pd3dcbDummyBoneTransforms = ::CreateBufferResource(pd3dDevice, pd3dCommandList, nullptr, nBufferSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

    // 버퍼의 내용을 0으로 초기화 (선택 사항이지만 권장)
    UINT8* pMappedData = nullptr;
    D3D12_RANGE readRange{ 0, 0 };
    m_pd3dcbDummyBoneTransforms->Map(0, &readRange, reinterpret_cast<void**>(&pMappedData));
    memset(pMappedData, 0, nBufferSize);
    m_pd3dcbDummyBoneTransforms->Unmap(0, nullptr);
}

D3D12_GPU_VIRTUAL_ADDRESS Scene::GetDummyBoneBufferAddress() const
{
    if (m_pd3dcbDummyBoneTransforms)
    {
        return m_pd3dcbDummyBoneTransforms->GetGPUVirtualAddress();
    }
    return 0;
}

void Scene::MakeSrv(ID3D12Device* pd3dDevice)
{
    // ▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼ SRV 디스크립터 힙 생성 ▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼
    // 텍스처와 같은 셰이더 리소스 뷰(SRV)들을 담을 디스크립터 힙을 생성합니다.
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1024; // 이 씬에서 사용할 최대 텍스처 개수 (임의로 128로 설정, 필요시 조절)
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // **매우 중요**: 셰이더가 접근 가능해야 함
    srvHeapDesc.NodeMask = 0;

    // 디스크립터 힙 생성
    pd3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_SrvDescriptorHeap));

    // SRV 디스크립터의 크기를 저장해 둡니다. 핸들 주소를 계산할 때 필요합니다.
    _SrvDescriptorIncrementSize = pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // 할당된 디스크립터 개수 카운터를 0으로 초기화합니다.
    _AllocatedSrvCount = 0;
    // ▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲
}

void Scene::ReleaseUploadBuffers()
{
    auto& Arr = ObjectManager::Instance()->GetAllObject();

    for (auto& Objects : Arr) {
        for (auto& Object : Objects) {
            auto Object_Render = Object->get_component<RenderComponent>();
            if (Object_Render)
                Object_Render->release_upload_buffers();
        }
    }
}

void Scene::BuildLightsAndMaterials()
{
    m_pLights = new LIGHTS;
    ::ZeroMemory(m_pLights, sizeof(LIGHTS));
    m_pLights->m_xmf4GlobalAmbient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);

    // 백색 방향성 조명
    m_pLights->m_pLights[0].m_bEnable = true;
    m_pLights->m_pLights[0].m_nType = DIRECTIONAL_LIGHT; 
    m_pLights->m_pLights[0].m_xmf4Ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
    m_pLights->m_pLights[0].m_xmf4Diffuse = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
    m_pLights->m_pLights[0].m_xmf4Specular = XMFLOAT4(1.f, 1.f, 1.f, 0.0f);
    m_pLights->m_pLights[0].m_xmf3Direction = XMFLOAT3(0.0f, -1.0f, 1.0f);

    m_pMaterials = new MATERIALS;
    ::ZeroMemory(m_pMaterials, sizeof(MATERIALS));

    // 흰색 플라스틱 느낌
    m_pMaterials->m_pReflections[0]._ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
    m_pMaterials->m_pReflections[0]._diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
    m_pMaterials->m_pReflections[0]._specular = XMFLOAT4(1.f, 1.f, 1.f, 16.0f);
    m_pMaterials->m_pReflections[0]._emissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
}

void Scene::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
    UINT ncbElementBytes = ((sizeof(LIGHTS) + 255) & ~255); // 256의 배수
    m_pd3dcbLights = ::CreateBufferResource(pd3dDevice, pd3dCommandList, NULL, ncbElementBytes, 
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, NULL);
    m_pd3dcbLights->Map(0, NULL, (void**)&m_pcbMappedLights);

    ncbElementBytes = ((sizeof(MATERIALS) + 255) & ~255); // 256의 배수
    m_pd3dcbMaterials = ::CreateBufferResource(pd3dDevice, pd3dCommandList, NULL, ncbElementBytes, 
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, NULL);
    m_pd3dcbMaterials->Map(0, NULL, (void**)&m_pcbMappedMaterials);
}

void Scene::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
    ::memcpy(m_pcbMappedLights, m_pLights, sizeof(LIGHTS));
    ::memcpy(m_pcbMappedMaterials, m_pMaterials, sizeof(MATERIALS));
}

void Scene::ReleaseShaderVariables()
{
    if (m_pd3dcbLights) m_pd3dcbLights->Unmap(0, NULL);
    if (m_pd3dcbMaterials) m_pd3dcbMaterials->Unmap(0, NULL);
}