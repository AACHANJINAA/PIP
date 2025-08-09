#include "stdafx.h"
#include "Chess_Scene.h"
#include "ObjectManager.h"
#include "BoardCube.h"
#include "Chess_King.h"
#include "Other_King.h"

CChess_Scene::CChess_Scene(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	//BuildObjects(pd3dDevice, pd3dCommandList);
}

CChess_Scene::~CChess_Scene()
{
	ReleaseObjects();
}

void CChess_Scene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
    m_pd3dGraphicsRootSignature = CreateGraphicsRootSignature(pd3dDevice);

    // 셰이더 생성
    m_nShaders = 1;
    m_pShaders = new CObjectsShader[m_nShaders];
    m_pShaders[0].CreateShader(pd3dDevice, m_pd3dGraphicsRootSignature);

    // 카메라 생성
    m_ChessCamera = new CFreeCamera{};
    m_pCamera = m_ChessCamera;

    if (m_pCamera)
    {
        m_pCamera->CreateShaderVariables(pd3dDevice, pd3dCommandList);
    }

    m_ChessCamera->SetPosition(0.f, 5.f, -5.f);

    // 보드판 생성
    std::shared_ptr<CGameObject> Board{};
    CMesh* BoardMesh{};
    float MoveDistance{};

    for (int i = 0; i < 8; ++i) // 세로
    {
        for (int j = 0; j < 8; ++j) // 가로
        {
            Board = std::make_shared<CBoardCube>();
            BoardMesh = new CReadObjMesh{ pd3dDevice,pd3dCommandList,"Resource/Cube_Normal.obj" };
            if ((j + i) % 2)
            {
                BoardMesh->ChangeColor(pd3dCommandList, 0.941f, 0.851f, 0.710f, 1.f);
            }
            else
            {
                BoardMesh->ChangeColor(pd3dCommandList, 0.710f, 0.533f, 0.388f, 1.f);
            }
            Board->SetMesh(BoardMesh);
            Board->SetPosition(((Board->m_pMesh->m_Right - Board->m_pMesh->m_Left) * Board->GetSize().x * j),
                -(Board->m_pMesh->m_Top - Board->m_pMesh->m_Bottom) * Board->GetSize().y * 0.5f,
                ((Board->m_pMesh->m_Front - Board->m_pMesh->m_Back) * Board->GetSize().z * i));
            Board->m_PosX = j;
            Board->m_PosY = i;
            CObjectManager::GetManager()->PushFloorObejct(Board);
        }
    }

    Board = std::make_shared<CBoardCube>();
    BoardMesh = new CReadGlbMesh{ pd3dDevice,pd3dCommandList,"Resource/Test/Brute_Dance.glb" };

    Board->SetMesh(BoardMesh);
    Board->SetScale(0.01f, 0.01f, 0.01f);
    Board->SetPosition(((Board->m_pMesh->m_Right - Board->m_pMesh->m_Left) * Board->GetSize().x),
        0.f,
        ((Board->m_pMesh->m_Front - Board->m_pMesh->m_Back) * Board->GetSize().z));
    Board->m_PosX = 0;
    Board->m_PosY = 0;
    CObjectManager::GetManager()->PushFloorObejct(Board);

    //{
    //    // 플레이어 생성
    //    std::shared_ptr<CGameObject> Player = std::make_shared<CChess_King>(0, 0);
    //    CMesh* Chess_Mesh = new CReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Chess_King.obj" };
    //    Chess_Mesh->ChangeColor(pd3dCommandList, 1.0f, 1.0f, 1.0f, 1.f);
    //    Player.get()->SetMesh(Chess_Mesh);

    //    // 이동 거리 설정
    //    static_cast<CChess_King*>(Player.get())->SetDistance(MoveDistance);
    //    Player.get()->SetScale(1.f, 1.f, 1.f);

    //    // 매니저에 넣기
    //    CObjectManager::GetManager()->PushObject(Player);
    //}



    //{
    //    // 상대방 생성
    //    std::shared_ptr<CGameObject> Other = std::make_shared<COther_King>(7, 7);
    //    CMesh* Chess_Mesh = new CReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Chess_King.obj" };
    //    Chess_Mesh->ChangeColor(pd3dCommandList, 0.0f, 0.0f, 0.0f, 1.f);
    //    Other.get()->SetMesh(Chess_Mesh);

    //    // 이동 거리 설정
    //    static_cast<COther_King*>(Other.get())->SetDistance(MoveDistance);
    //    Other.get()->SetScale(1.f, 1.f, 1.f);

    //    // 매니저에 넣기
    //    CObjectManager::GetManager()->PushObject(Other);
    //}

    BuildLightsAndMaterials();
    CreateShaderVariables(pd3dDevice, pd3dCommandList);
}

void CChess_Scene::ReleaseObjects()
{
	CObjectManager::GetManager()->DeleteAll();

    for (int i = 0; i < m_nShaders; i++)
    {
        m_pShaders[i].ReleaseShaderVariables();
    }
    if (m_pShaders)
    {
        delete[] m_pShaders;
    }

	if (m_pCamera)
	{
		delete m_pCamera;
		m_pCamera = nullptr;
	}
}

void CChess_Scene::ProcessInput(float fElapsedTime, HWND hWnd, UINT nMessageID, POINT ptOldCursorPos)
{
    m_ChessCamera->KeyInput(fElapsedTime, hWnd, nMessageID, ptOldCursorPos);


    std::array<std::list<std::shared_ptr<CGameObject>>, ALLARRAYSIZE>& Arr = CObjectManager::GetManager()->GetAllObject();

    for (std::list<std::shared_ptr<CGameObject>>& Objects : Arr) {
        for (std::shared_ptr<CGameObject>& Object : Objects) {
            if (nullptr != Object)
            {
                Object->ProcessInput(fElapsedTime, hWnd, nMessageID, ptOldCursorPos);
                Object->UpdateBoundingBox();
            }
        }
    }
  
}

void CChess_Scene::AnimateObjects(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList)
{


    std::array<std::list<std::shared_ptr<CGameObject>>, ALLARRAYSIZE>& Arr = CObjectManager::GetManager()->GetAllObject();

    for (std::list<std::shared_ptr<CGameObject>>& Objects : Arr) {
        for (std::shared_ptr<CGameObject>& Object : Objects) {
            if (nullptr != Object)
            {
                Object->Animate(fTimeElapsed, pd3dCommandList);
                Object.get()->UpdateBoundingBox();
            }
        }
    }

    
    

    std::list<std::shared_ptr<CGameObject>>& ObjectList = CObjectManager::GetManager()->GetEnemy();
    for (std::shared_ptr<CGameObject>& Object : ObjectList) {
        if (nullptr != Object)
        {
            Object->m_pMesh->ChangeColor(pd3dCommandList, std::dynamic_pointer_cast<COther_King>(Object)->GetHP() / 100.f,
                1.f,
                std::dynamic_pointer_cast<COther_King>(Object)->GetHP() / 100.f,
                1.f);
        }
    }


    m_ChessCamera->UpdateAnimateCamera(fTimeElapsed);
    m_pCamera->Update();

    Collision(fTimeElapsed);
}

// (수정) 조명, 머터리얼 데이터 연결 [PONG]
void CChess_Scene::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
    m_pCamera->Update();

    m_pCamera->SetViewportsAndScissorRects(pd3dCommandList);
    pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature);
    m_pCamera->UpdateShaderVariables(pd3dCommandList);

    UpdateShaderVariables(pd3dCommandList); // 조명/머터리얼 데이터 CPU -> GPU 복사

    D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = m_pd3dcbMaterials->GetGPUVirtualAddress();
    pd3dCommandList->SetGraphicsRootConstantBufferView(2, d3dGpuVirtualAddress); // 재질 버퍼를 b2에 연결

    d3dGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
    pd3dCommandList->SetGraphicsRootConstantBufferView(3, d3dGpuVirtualAddress); // 조명 버퍼를 b3에 연결

    for (int i = 0; i < m_nShaders; i++)
    {
        m_pShaders[i].Render(pd3dCommandList, m_pCamera);
    }

    std::array<std::list<std::shared_ptr<CGameObject>>, ALLARRAYSIZE>& Arr = CObjectManager::GetManager()->GetAllObject();

    for (std::list<std::shared_ptr<CGameObject>>& Objects : Arr) {
        for (std::shared_ptr<CGameObject>& Object : Objects) {
            if (nullptr != Object)
            {
                Object->Render(pd3dCommandList, m_pCamera);
            }
        }
    }
}

void CChess_Scene::Collision(float fElapsedTime)
{
    std::array<std::list<std::shared_ptr<CGameObject>>, ALLARRAYSIZE>& Arr = CObjectManager::GetManager()->GetAllObject();

    for (std::list<std::shared_ptr<CGameObject>>& Objects : Arr) {
        for (std::shared_ptr<CGameObject>& Object : Objects) {
            if (nullptr != Object)
            {
                Object->Collision(fElapsedTime);
            }
        }
    }
}
