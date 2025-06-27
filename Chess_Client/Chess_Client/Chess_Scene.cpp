#include "stdafx.h"
#include "Chess_Scene.h"
#include "ObjectManager.h"
#include "BoardCube.h"
#include "Chess_King.h"

CChess_Scene::CChess_Scene(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	BuildObjects(pd3dDevice, pd3dCommandList);
}

CChess_Scene::~CChess_Scene()
{
	ReleaseObjects();
}

void CChess_Scene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
    m_pd3dGraphicsRootSignature = CreateGraphicsRootSignature(pd3dDevice);
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
    CBoardCube* Board{};
    CMesh* BoardMesh{};


    for (int i = 0; i < 8; ++i) // 세로
    {
        for (int j = 0; j < 8; ++j) // 가로
        {
            Board = new CBoardCube{};
            BoardMesh = new CReadObjMesh{ pd3dDevice,pd3dCommandList,"Resource/Cube.obj" };
            if((j + i) % 2)
            {
                BoardMesh->ChangeColor(pd3dCommandList, 0.941f, 0.851f, 0.710f, 1.f);
            }
            else
            {
                BoardMesh->ChangeColor(pd3dCommandList, 0.710, 0.533, 0.388, 1.f);
            }
            Board->SetMesh(BoardMesh);
            Board->SetPosition(((Board->m_pMesh->m_Right - Board->m_pMesh->m_Left) * Board->GetSize().x * j),
                -(Board->m_pMesh->m_Top - Board->m_pMesh->m_Bottom) * Board->GetSize().y * 0.5f,
                ((Board->m_pMesh->m_Front - Board->m_pMesh->m_Back) * Board->GetSize().z * i));
            Board->m_PosX = j;
            Board->m_PosY = i;
            CObjectManager::GetManager()->PushObject(Board);
        }
    }



    float MoveDistance = (Board->m_pMesh->m_Right - Board->m_pMesh->m_Left) * Board->GetSize().x;

    // 플레이어 생성
    CChess_King* Player = new CChess_King{0,0};
    CMesh* Chess_Mesh = new CReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Chess_King.obj"};
    Chess_Mesh->ChangeColor(pd3dCommandList, 1.0f, 1.0f, 1.0f, 1.f);
    Player->SetMesh(Chess_Mesh);

    // 이동 거리 설정
    Player->SetDistance(MoveDistance);
    Player->SetScale(1.f,1.f,1.f);

    // 매니저에 넣기
    CObjectManager::GetManager()->PushObject(Player);
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

    std::vector<std::list<CGameObject*>*>& Vec = CObjectManager::GetManager()->GetAllObject();

    if (Vec.size()) {
        for (std::list<CGameObject*>*& Objects : Vec) {
            if (Objects != nullptr) {
                for (CGameObject*& Object : *Objects) {
                    Object->ProcessInput(fElapsedTime, hWnd, nMessageID, ptOldCursorPos);
                    Object->UpdateBoundingBox();
                }
            }
        }
    }
  
}

void CChess_Scene::AnimateObjects(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList)
{
    std::vector<std::list<CGameObject*>*>& Vec = CObjectManager::GetManager()->GetAllObject();

    if (Vec.size()) {
        for (std::list<CGameObject*>*& Objects : Vec) {
            if (Objects != nullptr) {
                for (CGameObject*& Object : *Objects) {
                    Object->Animate(fTimeElapsed, pd3dCommandList);
                    Object->UpdateBoundingBox();
                }
            }
        }
    }

    m_ChessCamera->UpdateAnimateCamera(fTimeElapsed);
    m_pCamera->Update();

    Collision(fTimeElapsed);
}

void CChess_Scene::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
    m_pCamera->Update();

    m_pCamera->SetViewportsAndScissorRects(pd3dCommandList);
    pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature);
    m_pCamera->UpdateShaderVariables(pd3dCommandList);

    for (int i = 0; i < m_nShaders; i++)
    {
        m_pShaders[i].Render(pd3dCommandList, m_pCamera);
    }



    std::vector<std::list<CGameObject*>*>& Vec = CObjectManager::GetManager()->GetAllObject();

    if (Vec.size()) {
        for (std::list<CGameObject*>*& Objects : Vec) {
            if (Objects != nullptr) {
                for (CGameObject*& Object : *Objects) {
                    Object->Render(pd3dCommandList, m_pCamera);
                }
            }
        }
    }

  
}

void CChess_Scene::Collision(float fElapsedTime)
{
    std::vector<std::list<CGameObject*>*>& Vec = CObjectManager::GetManager()->GetAllObject();

    if (Vec.size()) {
        for (std::list<CGameObject*>*& Objects : Vec) {
            if (Objects != nullptr) {
                for (CGameObject*& Object : *Objects) {
                    if (nullptr != Object)
                    {
                        Object->Collision(fElapsedTime);
                    }
                }
            }
        }
    }

    CObjectManager::GetManager()->UpdateAll();

    std::list<CGameObject*> EnemyList = CObjectManager::GetManager()->GetEnemy();

    for (CGameObject* Enemy : EnemyList) {
        if (!Enemy->m_bCollision)
        {
            return;
        }
    }
}
