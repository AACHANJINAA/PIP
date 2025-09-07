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
    MakeDummyBonebuffer(pd3dDevice, pd3dCommandList); // 더미 생성
    // 셰이더 생성

    // 기존 오브젝트 셰이더
    _AllShaders.push_back(std::make_shared<CObjectsShader>());
    _AllShaders.back()->CreateShader(pd3dDevice, _AllRootSignature[0].Get());

    _AllShaders.push_back(std::make_shared<GlbShader>());
    _AllShaders.back()->CreateShader(pd3dDevice, _AllRootSignature[1].Get());
    // 대원 잘쓸게~
    _AllShaders.push_back(std::make_shared<DebugShader>());
    _AllShaders.back()->CreateShader(pd3dDevice, _AllRootSignature[0].Get()); // 디버그 셰이더는 일반 루트 시그니처 사용

    m_pDebugShader = _AllShaders.back().get(); // 멤버 변수에 포인터 저장
 
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
            Board->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가
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
    Board->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가

    BoardMesh = new ReadGlbMesh{ pd3dDevice,pd3dCommandList,"Resource/MapData/SM_Crate_01.glb", (Scene*)this};

    Board->SetMesh(BoardMesh);
    Board->SetShader(_AllShaders[1]); // GLB
    Board->m_pMaterial->SetShaderRootSignature(_AllRootSignature[1].Get());
   // Board->SetScale(1.f, 1.f, 1.f);
    Board->SetPosition(((Board->m_pMesh->m_Right - Board->m_pMesh->m_Left) * Board->GetSize().x),
        1.0f,
        ((Board->m_pMesh->m_Front - Board->m_pMesh->m_Back) * Board->GetSize().z));
    Board->m_PosX = 0;
    Board->m_PosY = 0;
    ObjectManager::Instance()->PushFloorObject(Board);

    // 언리얼에서 뽑은 FBX 테스트
    _pFbxObject = std::make_shared<BoardCube>();
    _pFbxObject->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가

    _pCollisionMesh = new ReadFbxMesh{ pd3dDevice,pd3dCommandList,"Resource/Test/TestCollision.fbx" };

    _pFbxObject->SetMesh(_pCollisionMesh);
    XMFLOAT3 Scale = XMFLOAT3(0.01f, 0.01f, 0.01f);
    _pFbxObject->SetScale(Scale.x, Scale.y, Scale.z);
    _pFbxObject->SetPosition(((_pFbxObject->m_pMesh->m_Right - _pFbxObject->m_pMesh->m_Left) * _pFbxObject->GetSize().x + 3),
        0.8f,
        ((_pFbxObject->m_pMesh->m_Front - _pFbxObject->m_pMesh->m_Back) * _pFbxObject->GetSize().z) + 5);
    Board->m_PosX = 0;
    Board->m_PosY = 0;
    ObjectManager::Instance()->PushFloorObject(_pFbxObject);

    // ----------------------------------------------------------------------------------------------------------------------------------------------
    // collision 디버깅 코드

    ReadFbxMesh* churchFbxMesh = dynamic_cast<ReadFbxMesh*>(_pCollisionMesh);

    if (_pCollisionMesh)
    {
        const auto& collisionPrimitives = _pCollisionMesh->GetCollisionPrimitives();
        debugObjects.clear(); // 이전 데이터 클리어

        // CollisionPrimitive 개수만큼 디버그 오브젝트를 미리 생성
        for (const auto& primitive : collisionPrimitives)
        {
            // OBB, AABB, Wireframe 오브젝트 3개를 생성만 하고 벡터에 추가
            // 위치 계산은 Render 함수에서 매 프레임 수행하므로 여기서는 안함
            auto debugOOBBObject = std::make_shared<BoardCube>();
            debugOOBBObject->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가
            debugOOBBObject->SetMesh(new DebugCollisionBox(pd3dDevice, pd3dCommandList, XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)));
            debugObjects.push_back(debugOOBBObject);

            auto debugAABBObject = std::make_shared<BoardCube>();
            debugAABBObject->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가
            debugAABBObject->SetMesh(new DebugCollisionBox(pd3dDevice, pd3dCommandList, XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)));
            debugObjects.push_back(debugAABBObject);

            auto debugWireframeObject = std::make_shared<BoardCube>();
            debugWireframeObject->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가
            debugWireframeObject->SetMesh(new DebugWireframeMesh(pd3dDevice, pd3dCommandList, primitive.vertices, primitive.indices, XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)));
            debugObjects.push_back(debugWireframeObject);
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
    m_pCamera->Update();

    std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& Arr = ObjectManager::Instance()->GetAllObject();
    for (std::list<std::shared_ptr<GameObject>>& Objects : Arr) {
        for (std::shared_ptr<GameObject>& Object : Objects) {
            if (nullptr != Object)
            {
                Object->Animate(fTimeElapsed, m_pCamera, pd3dCommandList);
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
    size_t ShaderNum{};
    for (auto const& [shader, objectGroup] : ObjectManager::Instance()->GetRenderMap())
    {
        // 셰이더 그룹이 바뀔 때 한 번만 상태를 설정
        // 같은 그룹의 첫번째 원소를 기준으로 설정
        objectGroup[0]->OnPrepareRender(pd3dCommandList); // PSO와 루트 시그니처를 여기서 설정

       

        // 현재 그룹(같은 셰이더 사용 하는 그룹)의 모든 오브젝트를 렌더링
        for (const std::shared_ptr<GameObject>& Object : objectGroup)
        {
            if (ShaderNum == 1) // Glb
            {
                // 전역 데이터 설정 (모든 객체가 이 조명과 재질 정보를 공유)
                UpdateShaderVariables(pd3dCommandList); // 조명/머터리얼 데이터 CPU->GPU 복사
                D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
                pd3dCommandList->SetGraphicsRootConstantBufferView(3, d3dGpuVirtualAddress);
                d3dGpuVirtualAddress = m_pd3dcbMaterials->GetGPUVirtualAddress();
                pd3dCommandList->SetGraphicsRootConstantBufferView(2, d3dGpuVirtualAddress);

                // A. 뼈 행렬 상수 버퍼 바인딩 (애니메이션용, 아직 작동X)
          // ReadGlbMesh가 애니메이션 데이터를 담고있는 상수 버퍼의 주소를 반환해야 합니다.
                D3D12_GPU_VIRTUAL_ADDRESS boneTransformAddress = dynamic_cast<ReadGlbMesh*>(Object->m_pMesh)->GetBoneTransformsBufferAddress();
                // [수정] 주소가 유효하면 실제 버퍼를, 아니면 더미 버퍼를 바인딩
                if (boneTransformAddress != 0)
                {
                    pd3dCommandList->SetGraphicsRootConstantBufferView(4, boneTransformAddress);
                }
                else
                {
                    // m_pDummyBoneBuffer는 Scene이나 렌더러가 하나쯤 가지고 있으면 좋음
                    pd3dCommandList->SetGraphicsRootConstantBufferView(4, GetDummyBoneBufferAddress());
                }

                // B. 텍스처 SRV 테이블 바인딩
                // ReadGlbMesh가 로딩 시 생성한 SRV의 GPU 핸들을 반환해야 합니다.
                D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = dynamic_cast<ReadGlbMesh*>(Object->m_pMesh)->GetSrvGpuHandle();
                if (textureSrvHandle.ptr != 0)
                {
                    // 루트 시그니처에 정의된 SRV 테이블 슬롯(예: 5번)에 텍스처를 바인딩합니다.
                    // 이 숫자(5)는 CreateSkinnedGraphicsRootSignature에서 SRV 테이블을 설정한 인덱스와 일치해야 합니다.
                    pd3dCommandList->SetGraphicsRootDescriptorTable(5, textureSrvHandle);
                }
                //Object->Render(pd3dCommandList, m_pCamera); // 디버깅용
            }
            Object->Render(pd3dCommandList,m_pCamera);
        }
        ++ShaderNum;
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

    if (isRenderFbxFileBoundingBoxes && _pCollisionMesh && _pFbxObject)
    {
        pd3dCommandList->SetGraphicsRootSignature(_AllRootSignature[0].Get());
        m_pDebugShader->OnPrepareRender(pd3dCommandList);
        m_pCamera->UpdateShaderVariables(pd3dCommandList);

        const auto& collisionPrimitives = _pCollisionMesh->GetCollisionPrimitives();
        XMMATRIX parentWorld = XMLoadFloat4x4(&_pFbxObject->m_xmf4x4World);

        for (size_t i = 0; i < collisionPrimitives.size(); ++i)
        {
            const auto& primitive = collisionPrimitives[i];

            std::shared_ptr<GameObject>& debugOOBBObject = debugObjects[i * 3 + 0];
            std::shared_ptr<GameObject>& debugAABBObject = debugObjects[i * 3 + 1];
            std::shared_ptr<GameObject>& debugWireframeObject = debugObjects[i * 3 + 2];

            // 1. OBB 위치 갱신
            XMMATRIX oobb_S = XMMatrixScaling(primitive.oobb.Extents.x * 2.0f, primitive.oobb.Extents.y * 2.0f, primitive.oobb.Extents.z * 2.0f);
            XMVECTOR oobb_quat = XMQuaternionNormalize(XMLoadFloat4(&primitive.oobb.Orientation));
            XMMATRIX oobb_R = XMMatrixRotationQuaternion(oobb_quat);
            XMMATRIX oobb_T = XMMatrixTranslation(primitive.oobb.Center.x, primitive.oobb.Center.y, primitive.oobb.Center.z);
            XMStoreFloat4x4(&debugOOBBObject->m_xmf4x4World, (oobb_S * oobb_R * oobb_T) * parentWorld);

            // 2. AABB 위치 갱신
            XMMATRIX aabb_S = XMMatrixScaling(primitive.aabb.Extents.x * 2.0f, primitive.aabb.Extents.y * 2.0f, primitive.aabb.Extents.z * 2.0f);
            XMMATRIX aabb_T = XMMatrixTranslation(primitive.aabb.Center.x, primitive.aabb.Center.y, primitive.aabb.Center.z);
            XMStoreFloat4x4(&debugAABBObject->m_xmf4x4World, (aabb_S * aabb_T) * parentWorld);

            // 3. 와이어프레임 위치 갱신
            debugWireframeObject->m_xmf4x4World = _pFbxObject->m_xmf4x4World;

            debugObjects[i * 3 + 0]->Render(pd3dCommandList, m_pCamera); // OBB
            debugObjects[i * 3 + 1]->Render(pd3dCommandList, m_pCamera); // AABB
            debugObjects[i * 3 + 2]->Render(pd3dCommandList, m_pCamera); // Wireframe
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


