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
    std::unique_ptr<CGameObject> Board{};
    CMesh* BoardMesh{};
    float MoveDistance{};

    for (int i = 0; i < 8; ++i) // 세로
    {
        for (int j = 0; j < 8; ++j) // 가로
        {
            Board = std::make_unique<CBoardCube>();
            BoardMesh = new CReadObjMesh{ pd3dDevice,pd3dCommandList,"Resource/Cube.obj" };
            if ((j + i) % 2)
            {
                BoardMesh->ChangeColor(pd3dCommandList, 0.941f, 0.851f, 0.710f, 1.f);
            }
            else
            {
                BoardMesh->ChangeColor(pd3dCommandList, 0.710, 0.533, 0.388, 1.f);
            }
            Board.get()->SetMesh(BoardMesh);
            Board.get()->SetPosition(((Board.get()->m_pMesh->m_Right - Board.get()->m_pMesh->m_Left) * Board.get()->GetSize().x * j),
                -(Board.get()->m_pMesh->m_Top - Board.get()->m_pMesh->m_Bottom) * Board.get()->GetSize().y * 0.5f,
                ((Board.get()->m_pMesh->m_Front - Board.get()->m_pMesh->m_Back) * Board.get()->GetSize().z * i));
            Board.get()->m_PosX = j;
            Board.get()->m_PosY = i;


            MoveDistance = (Board.get()->m_pMesh->m_Right - Board.get()->m_pMesh->m_Left) * Board.get()->GetSize().x;
            CObjectManager::GetManager()->PushObject(std::move(Board));
        }
    }


    {
        // 플레이어 생성
        std::unique_ptr<CGameObject> Player = std::make_unique<CChess_King>(0, 0);
        CMesh* Chess_Mesh = new CReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Chess_King.obj" };
        Chess_Mesh->ChangeColor(pd3dCommandList, 1.0f, 1.0f, 1.0f, 1.f);
        Player.get()->SetMesh(Chess_Mesh);

        // 이동 거리 설정
        static_cast<CChess_King*>(Player.get())->SetDistance(MoveDistance);
        Player.get()->SetScale(1.f, 1.f, 1.f);

        // 매니저에 넣기
        CObjectManager::GetManager()->PushObject(std::move(Player));
    }



    {
        // 상대방 생성
        std::unique_ptr<CGameObject> Other = std::make_unique<COther_King>(7, 7);
        CMesh* Chess_Mesh = new CReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Chess_King.obj" };
        Chess_Mesh->ChangeColor(pd3dCommandList, 0.0f, 0.0f, 0.0f, 1.f);
        Other.get()->SetMesh(Chess_Mesh);

        // 이동 거리 설정
        static_cast<COther_King*>(Other.get())->SetDistance(MoveDistance);
        Other.get()->SetScale(1.f, 1.f, 1.f);

        // 매니저에 넣기
        CObjectManager::GetManager()->PushObject(std::move(Other));
    }
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


    std::array<std::list<std::unique_ptr<CGameObject>>*, ALLARRAYSIZE>& Arr = CObjectManager::GetManager()->GetAllObject();

    if (Arr.size()) {
        for (std::list<std::unique_ptr<CGameObject>>*& Objects : Arr) {
            if (Objects != nullptr) {
                for (std::unique_ptr<CGameObject>& Object : *Objects) {
                    if (nullptr != Object)
                    {
                        Object.get()->ProcessInput(fElapsedTime, hWnd, nMessageID, ptOldCursorPos);
                        Object.get()->UpdateBoundingBox();
                    }
                }
            }
        }
    }
  
}

void CChess_Scene::AnimateObjects(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList)
{

    std::array<std::list<std::unique_ptr<CGameObject>>*, ALLARRAYSIZE>& Arr = CObjectManager::GetManager()->GetAllObject();

    if (Arr.size()) {
        for (std::list<std::unique_ptr<CGameObject>>*& Objects : Arr) {
            if (Objects != nullptr) {
                for (std::unique_ptr<CGameObject>& Object : *Objects) {
                    if (nullptr != Object)
                    {
                        Object.get()->Animate(fTimeElapsed, pd3dCommandList);
                        Object.get()->UpdateBoundingBox();
                    }
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

    std::array<std::list<std::unique_ptr<CGameObject>>*, ALLARRAYSIZE>& Arr = CObjectManager::GetManager()->GetAllObject();

    if (Arr.size()) {
        for (std::list<std::unique_ptr<CGameObject>>*& Objects : Arr) {
            if (Objects != nullptr) {
                for (std::unique_ptr<CGameObject>& Object : *Objects) {
                    if (nullptr != Object)
                    {
                        Object.get()->Render(pd3dCommandList, m_pCamera);
                    }
                }
            }
        }
    }

  
}

void CChess_Scene::Collision(float fElapsedTime)
{
    std::array<std::list<std::unique_ptr<CGameObject>>*, ALLARRAYSIZE>& Arr = CObjectManager::GetManager()->GetAllObject();

    if (Arr.size()) {
        for (std::list<std::unique_ptr<CGameObject>>*& Objects : Arr) {
            if (Objects != nullptr) {
                for (std::unique_ptr<CGameObject>& Object : *Objects) {
                    if (nullptr != Object)
                    {
                        Object.get()->Collision(fElapsedTime);
                    }
                }
            }
        }
    }
}
