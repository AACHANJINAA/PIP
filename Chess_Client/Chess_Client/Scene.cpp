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
            m_pLockedObject->m_bCollision = true;
        }

        break;
    }

    return(false);
}

bool Scene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
    return(false);
}

// (수정) 머터리얼 + 조명 파라미터추가
ID3D12RootSignature* Scene::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
    ID3D12RootSignature* pd3dGraphicsRootSignature = NULL;
    D3D12_ROOT_PARAMETER pd3dRootParameters[4];
    // 월드 변환 행렬용 
    pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    pd3dRootParameters[0].Constants.Num32BitValues = 16;
    pd3dRootParameters[0].Constants.ShaderRegister = 0;
    pd3dRootParameters[0].Constants.RegisterSpace = 0;
    pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    // 카메라 행렬용
    pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    pd3dRootParameters[1].Constants.Num32BitValues = 36;
    pd3dRootParameters[1].Constants.ShaderRegister = 1;
    pd3dRootParameters[1].Constants.RegisterSpace = 0;
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

ID3D12RootSignature* Scene::GetGraphicsRootSignature()
{
    return(m_pd3dGraphicsRootSignature);
}

GameObject* Scene::PickObjectPointedByCursor(int xClient, int yClient)
{
    XMFLOAT3 xmf3PickPosition;
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
                nIntersected = Object.get()->PickModelOBB(xmvPickPosition, xmmtxView, &fHitDistance);
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

void Scene::ReleaseUploadBuffers()
{
    auto& Arr = ObjectManager::Instance()->GetAllObject();

    for (auto& Objects : Arr) {
        for (auto& Object : Objects) {
            Object.get()->ReleaseUploadBuffers();
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
    m_pMaterials->m_pReflections[0].m_xmf4Ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
    m_pMaterials->m_pReflections[0].m_xmf4Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
    m_pMaterials->m_pReflections[0].m_xmf4Specular = XMFLOAT4(1.f, 1.f, 1.f, 16.0f);
    m_pMaterials->m_pReflections[0].m_xmf4Emissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
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