#include "stdafx.h"
#include "Chess_Scene.h"
#include "ObjectManager.h"
#include "BoardCube.h"
#include "MainPlayer.h"
#include "OtherPlayer.h"
#include "GlbShader.h"

Chess_Scene::Chess_Scene(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	//BuildObjects(pd3dDevice, pd3dCommandList);
}

Chess_Scene::~Chess_Scene()
{
	ReleaseObjects();
}

void Chess_Scene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
    _SignatureNum = 2;
    _AllRootSignature.resize(_SignatureNum);
   
    _AllRootSignature[0] = CreateGraphicsRootSignature(pd3dDevice); // 일반
    _AllRootSignature[1] = CreateSkinnedGraphicsRootSignature(pd3dDevice); // GLB

    MakeSrv(pd3dDevice); // Srv 디스크립터 생성

    // 셰이더 생성

    // 기존 오브젝트 셰이더
    _AllShaders.push_back(std::make_shared<CObjectsShader>());
    _AllShaders.back()->CreateShader(pd3dDevice, _AllRootSignature[0].Get());

    _AllShaders.push_back(std::make_shared<GlbShader>());
    _AllShaders.back()->CreateShader(pd3dDevice, _AllRootSignature[1].Get());
 
    // 카메라 생성
    m_ChessCamera = new FreeCamera{};
    m_pCamera = m_ChessCamera;
    m_ChessCamera->SetCameraMode(CAMERA_MODE::CAMERA_THIRD_PERSON);
    m_ChessCamera->SetOffset(5.f);
    m_ChessCamera->Rotate(30.f,0.f,0.f);

    if (m_pCamera)
    {
        m_pCamera->CreateShaderVariables(pd3dDevice, pd3dCommandList);
    }

    m_ChessCamera->SetPosition(0.f, 5.f, -5.f);

    // 보드판 생성
    std::shared_ptr<GameObject> Board{};
    Mesh* BoardMesh{};
    float MoveDistance{};

    for (int i = 0; i < 8; ++i) // 세로
    {
        for (int j = 0; j < 8; ++j) // 가로
        {
            Board = std::make_shared<BoardCube>();
            BoardMesh = new ReadObjMesh{ pd3dDevice,pd3dCommandList,"Resource/Cube_Normal.obj" };
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
            ObjectManager::Instance()->PushFloorObject(Board);
        }
    }

    Board = std::make_shared<BoardCube>();

    BoardMesh = new ReadGlbMesh{ pd3dDevice,pd3dCommandList,"Resource/Test/Brute_Dance.glb", (Scene*)this};

    Board->SetMesh(BoardMesh);
    Board->SetShader(_AllShaders[1]); // GLB
    Board->m_pMaterial->SetShaderRootSignature(_AllRootSignature[1].Get());
    Board->SetScale(0.01f, 0.01f, 0.01f);
    Board->SetPosition(((Board->m_pMesh->m_Right - Board->m_pMesh->m_Left) * Board->GetSize().x),
        0.f,
        ((Board->m_pMesh->m_Front - Board->m_pMesh->m_Back) * Board->GetSize().z));
    Board->m_PosX = 0;
    Board->m_PosY = 0;
    ObjectManager::Instance()->PushFloorObject(Board);

    // 언리얼에서 뽑은 FBX 테스트
    std::shared_ptr<GameObject> churchObject = std::make_shared<BoardCube>();
    Mesh* churchMesh = new ReadFbxMesh{ pd3dDevice,pd3dCommandList,"Resource/Test/medieval_church.fbx" };

    churchObject->SetMesh(churchMesh);
   

    XMFLOAT3 churchScale = XMFLOAT3(0.001f, 0.001f, 0.001f);
    churchObject->SetScale(churchScale.x, churchScale.y, churchScale.z);
    churchObject->SetPosition(((churchObject->m_pMesh->m_Right - churchObject->m_pMesh->m_Left) * churchObject->GetSize().x), 0.f,
        ((churchObject->m_pMesh->m_Front - churchObject->m_pMesh->m_Back) * churchObject->GetSize().z) + 1);
    Board->m_PosX = 0;
    Board->m_PosY = 0;
    ObjectManager::Instance()->PushFloorObject(churchObject);

    // ----------------------------------------------------------------------------------------------------------------------------------------------
    // collision 디버깅 코드

    ReadFbxMesh* churchFbxMesh = dynamic_cast<ReadFbxMesh*>(churchMesh);

    if (churchFbxMesh)
    {
        const auto& collisionBoxes = churchFbxMesh->GetCollisionBoxes();
        OutputDebugStringA(("CollisionBox count: " + std::to_string(collisionBoxes.size()) + "\n").c_str());
        XMFLOAT4 debugColor = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);

        for (const auto& box : collisionBoxes)
        {
            std::shared_ptr<GameObject> debugBoxObject = std::make_shared<BoardCube>();

            DebugCubeMesh* debugCubeMesh = new DebugCubeMesh(pd3dDevice, pd3dCommandList, debugColor);
        
            debugBoxObject->SetMesh(debugCubeMesh);
           

            XMMATRIX parentWorld = XMLoadFloat4x4(&churchObject->m_xmf4x4World);

            XMMATRIX S = XMMatrixScaling(box.Extents.x * 2.0f, box.Extents.y * 2.0f, box.Extents.z * 2.0f);
            XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&box.Orientation));
            XMMATRIX T = XMMatrixTranslation(box.Center.x, box.Center.y, box.Center.z);
            XMMATRIX localBoxMatrix = S * R * T;

            XMMATRIX finalWorldMatrix = localBoxMatrix * parentWorld;

            XMStoreFloat4x4(&debugBoxObject->m_xmf4x4World, finalWorldMatrix);

            debugObjects.push_back(debugBoxObject);
            OutputDebugStringA(("DebugCube count: " + std::to_string(debugObjects.size()) + "\n").c_str());
            OutputDebugStringA(("Box Center: " + std::to_string(box.Center.x) + ", " + std::to_string(box.Center.y) + ", " + std::to_string(box.Center.z) + "\n").c_str());
            OutputDebugStringA(("Box Extents: " + std::to_string(box.Extents.x) + ", " + std::to_string(box.Extents.y) + ", " + std::to_string(box.Extents.z) + "\n").c_str());
        }
    }





    // ----------------------------------------------------------------------------------------------------------------------------------------------

    //{
    //    // 플레이어 생성
    //    std::shared_ptr<GameObject> Player = std::make_shared<CChess_King>(0, 0);
    //    Mesh* Chess_Mesh = new ReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Chess_King.obj" };
    //    Chess_Mesh->ChangeColor(pd3dCommandList, 1.0f, 1.0f, 1.0f, 1.f);
    //    Player.get()->SetMesh(Chess_Mesh);

    //    // 이동 거리 설정
    //    static_cast<CChess_King*>(Player.get())->SetDistance(MoveDistance);
    //    Player.get()->SetScale(1.f, 1.f, 1.f);

    //    // 매니저에 넣기
    //    ObjectManager::Instance()->PushObject(Player);
    //}



    //{
    //    // 상대방 생성
    //    std::shared_ptr<GameObject> Other = std::make_shared<OtherPlayer>(7, 7);
    //    Mesh* Chess_Mesh = new ReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Chess_King.obj" };
    //    Chess_Mesh->ChangeColor(pd3dCommandList, 0.0f, 0.0f, 0.0f, 1.f);
    //    Other.get()->SetMesh(Chess_Mesh);

    //    // 이동 거리 설정
    //    static_cast<OtherPlayer*>(Other.get())->SetDistance(MoveDistance);
    //    Other.get()->SetScale(1.f, 1.f, 1.f);

    //    // 매니저에 넣기
    //    ObjectManager::Instance()->PushObject(Other);
    //}

    BuildLightsAndMaterials();
    CreateShaderVariables(pd3dDevice, pd3dCommandList);
}

void Chess_Scene::ReleaseObjects()
{
	ObjectManager::Instance()->DeleteAll();

    for (auto& iter : _AllShaders)
    {
        iter->ReleaseShaderVariables();
    }
    _AllShaders.clear();

	if (m_pCamera)
	{
		delete m_pCamera;
		m_pCamera = nullptr;
	}
}

void Chess_Scene::ProcessInput(float fElapsedTime, HWND hWnd, UINT nMessageID, POINT ptOldCursorPos)
{
    m_ChessCamera->KeyInput(fElapsedTime, hWnd, nMessageID, ptOldCursorPos);


    std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& Arr = ObjectManager::Instance()->GetAllObject();

    for (std::list<std::shared_ptr<GameObject>>& Objects : Arr) {
        for (std::shared_ptr<GameObject>& Object : Objects) {
            if (nullptr != Object)
            {
                Object->ProcessInput(fElapsedTime, hWnd, nMessageID, ptOldCursorPos);
                Object->UpdateBoundingBox();
            }
        }
    }

    m_ChessCamera->KeyInput(fElapsedTime, hWnd, nMessageID, ptOldCursorPos);
 
}

void Chess_Scene::AnimateObjects(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList)
{
    m_pCamera->Rotate();

    std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& Arr = ObjectManager::Instance()->GetAllObject();

    for (std::list<std::shared_ptr<GameObject>>& Objects : Arr) {
        for (std::shared_ptr<GameObject>& Object : Objects) {
            if (nullptr != Object)
            {
                Object->Animate(fTimeElapsed, pd3dCommandList);
                Object.get()->UpdateBoundingBox();
            }
        }
    }

    
    std::list<std::shared_ptr<GameObject>>& ObjectList = ObjectManager::Instance()->GetEnemy();
    for (std::shared_ptr<GameObject>& Object : ObjectList) {
        if (nullptr != Object)
        {
            Object->m_pMesh->ChangeColor(pd3dCommandList, std::dynamic_pointer_cast<OtherPlayer>(Object)->GetHP() / 100.f,
                1.f,
                std::dynamic_pointer_cast<OtherPlayer>(Object)->GetHP() / 100.f,
                1.f);
        }
    }

    Collision(fTimeElapsed);

    m_ChessCamera->UpdateAnimateCamera(fTimeElapsed);
}

// (수정) 조명, 머터리얼 데이터 연결 [PONG]
void Chess_Scene::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
    m_pCamera->Update();
    m_pCamera->SetViewportsAndScissorRects(pd3dCommandList);

    ObjectManager::Instance()->MakeRenderMap(m_pCamera);

    // 기본 루트 시그니처 설정 (텍스처 없는 일반 객체용)
    {
        pd3dCommandList->SetGraphicsRootSignature(_AllRootSignature[0].Get());
        // 기본 셰이더 PSO
        _AllShaders[0]->OnPrepareRender(pd3dCommandList);

        // 전역 데이터 설정 (모든 객체가 이 조명과 재질 정보를 공유)
        UpdateShaderVariables(pd3dCommandList); // 조명/머터리얼 데이터 CPU->GPU 복사
        D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
        pd3dCommandList->SetGraphicsRootConstantBufferView(3, d3dGpuVirtualAddress);
        d3dGpuVirtualAddress = m_pd3dcbMaterials->GetGPUVirtualAddress();
        pd3dCommandList->SetGraphicsRootConstantBufferView(2, d3dGpuVirtualAddress);

        // SRV 디스크립터 힙 설정 (모든 객체가 이 힙을 공유)
        if (_SrvDescriptorHeap)
        {
            ID3D12DescriptorHeap* ppd3dDescriptorHeaps[] = { _SrvDescriptorHeap.Get() };
            pd3dCommandList->SetDescriptorHeaps(_countof(ppd3dDescriptorHeaps), ppd3dDescriptorHeaps);
        }
    }

    for (auto const& [shader, objectGroup] : ObjectManager::Instance()->GetRenderMap())
    {
        // 셰이더 그룹이 바뀔 때 한 번만 상태를 설정
        // 같은 그룹의 첫번째 원소를 기준으로 설정
        objectGroup[0]->OnPrepareRender(pd3dCommandList); // PSO와 루트 시그니처를 여기서 설정

        // 현재 그룹(같은 셰이더 사용 하는 그룹)의 모든 오브젝트를 렌더링
        for (const std::shared_ptr<GameObject>& Object : objectGroup)
        {
            Object->Render(pd3dCommandList,m_pCamera);
        }
    }

    /*std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& Arr = ObjectManager::Instance()->GetAllObject();

    for (std::list<std::shared_ptr<GameObject>>& Objects : Arr) {
        for (std::shared_ptr<GameObject>& Object : Objects) {
            if (nullptr != Object)
            {
                Object->Render(pd3dCommandList, m_pCamera);
            }
        }
    }*/

    // 디버그 객체를 렌더링하는 블록을 추가합니다.
    if (isRenderFbxFileBoundingBoxes) // 올바른 플래그 이름을 사용합니다.
    {
        for (std::shared_ptr<GameObject>& debugObject : debugObjects) // 올바른 벡터 이름을 사용합니다.
        {
          if (nullptr != debugObject)
          {
              debugObject->Render(pd3dCommandList, m_pCamera);
          }
        }
    }
}

void Chess_Scene::Collision(float fElapsedTime)
{
    std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& Arr = ObjectManager::Instance()->GetAllObject();

    for (std::list<std::shared_ptr<GameObject>>& Objects : Arr) {
        for (std::shared_ptr<GameObject>& Object : Objects) {
            if (nullptr != Object)
            {
                Object->Collision(fElapsedTime);
            }
        }
    }
}


