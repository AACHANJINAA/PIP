#include "stdafx.h"
#include "Scene.h"
#include "ObjectManager.h"
CScene::CScene()
{

}

CScene::~CScene()
{
}

bool CScene::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{

    // 이 함수는 마우스를 입력하면 바로 실행됨

    switch (nMessageID)
    {
    case WM_LBUTTONDOWN: // 왼쪽 마우스 입력
        CGameObject* m_pLockedObject;

        m_pLockedObject = PickObjectPointedByCursor(LOWORD(lParam), HIWORD(lParam));

        if (m_pLockedObject)
        {
            m_pLockedObject->m_bCollision = true;
        }

        break;
    }

    return(false);
}

bool CScene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
    return(false);
}

ID3D12RootSignature* CScene::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
    ID3D12RootSignature* pd3dGraphicsRootSignature = NULL;
    D3D12_ROOT_PARAMETER pd3dRootParameters[2];
    pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    pd3dRootParameters[0].Constants.Num32BitValues = 16;
    pd3dRootParameters[0].Constants.ShaderRegister = 0;
    pd3dRootParameters[0].Constants.RegisterSpace = 0;
    pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    pd3dRootParameters[1].Constants.Num32BitValues = 32;
    pd3dRootParameters[1].Constants.ShaderRegister = 1;
    pd3dRootParameters[1].Constants.RegisterSpace = 0;
    pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
    d3dRootSignatureDesc.NumParameters = _countof(pd3dRootParameters);
    d3dRootSignatureDesc.pParameters = pd3dRootParameters;
    d3dRootSignatureDesc.NumStaticSamplers = 0;
    d3dRootSignatureDesc.pStaticSamplers = NULL;
    d3dRootSignatureDesc.Flags = d3dRootSignatureFlags;

    ID3DBlob* pd3dSignatureBlob = NULL;
    ID3DBlob* pd3dErrorBlob = NULL;

    ::D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
    pd3dDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
        pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&pd3dGraphicsRootSignature);

    if (pd3dSignatureBlob)
        pd3dSignatureBlob->Release();

    if (pd3dErrorBlob)
        pd3dErrorBlob->Release();

    return(pd3dGraphicsRootSignature);
}

ID3D12RootSignature* CScene::GetGraphicsRootSignature()
{
    return(m_pd3dGraphicsRootSignature);
}

CGameObject* CScene::PickObjectPointedByCursor(int xClient, int yClient)
{
    XMFLOAT3 xmf3PickPosition;
    xmf3PickPosition.x = (((2.0f * xClient) / (float)m_pCamera->m_d3dViewport.Width) - 1) / m_pCamera->m_xmf4x4Projection._11;
    xmf3PickPosition.y = -(((2.0f * yClient) / (float)m_pCamera->m_d3dViewport.Height) - 1) / m_pCamera->m_xmf4x4Projection._22;
    xmf3PickPosition.z = 1.0f;

    XMVECTOR xmvPickPosition = XMLoadFloat3(&xmf3PickPosition);
    XMMATRIX xmmtxView = XMLoadFloat4x4(&m_pCamera->m_xmf4x4View);

    bool nIntersected = false;
    float fNearestHitDistance = FLT_MAX;
    CGameObject* pNearestObject = NULL;
    std::vector<std::list<CGameObject*>*>& Vec = CObjectManager::GetManager()->GetAllObject();

    if (Vec.size()) {
        int i = 0;
        for (std::list<CGameObject*>*& Objects : Vec) {
            if (Objects != nullptr) {
                for (CGameObject*& Object : *Objects) {
                    float fHitDistance = FLT_MAX;
                    nIntersected = Object->PickModelOBB(xmvPickPosition, xmmtxView, &fHitDistance);
                    if (nIntersected && (fHitDistance < fNearestHitDistance))
                    {
                        fNearestHitDistance = fHitDistance;
                        pNearestObject = Object;
                    }
                }
                ++i;
                if (4 == i) {
                    break;
                }
            }
        }
    }
    return(pNearestObject);
    return nullptr;
}

void CScene::ReleaseUploadBuffers()
{
    std::vector<std::list<CGameObject*>*>& Vec = CObjectManager::GetManager()->GetAllObject();

    if (Vec.size()) {
        for (std::list<CGameObject*>*& Objects : Vec) {
            if (Objects != nullptr) {
                for (CGameObject*& Object : *Objects) {
                    Object->ReleaseUploadBuffers();
                }
            }
        }
    }
}
