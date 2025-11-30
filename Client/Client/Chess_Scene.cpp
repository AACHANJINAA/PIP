#include "stdafx.h"
#include "Chess_Scene.h"

#include "FreeCameraScript.h"
#include "ObjectManager.h"
#include "GameObject.h"

#include "MainPlayerScript.h"
#include "GltfAnimationTestScript.h"

#include "TransformComponent.h"
#include "RenderComponent.h"
#include "Mesh.h"
#include "ResourceManager.h"
// #include "PlayerScript.h"

#include "GltfTestScript.h"

#include "Renderer.h"    

void Chess_Scene::build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	// =========================필요한 메시 로드==================================

    ResourceManager::instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");

	// =====================================================================



    // 카메라 생성
    auto cameraObject = ObjectManager::instance()->create_game_object("FreeCamera");
    cameraObject->add_component<FreeCameraScript>();
    //cameraObject->transform()->set_local_position(XMFLOAT3(0.0f, 70.0f, -200.0f));
    cameraObject->transform()->set_local_position(XMFLOAT3(0.0f, 70.0f, -200.0f));
	cameraObject->transform()->set_local_rotation(0.f, 0, 0.f);

    load_scene_from_file("Resource/DDSMapData/ExportedClientData.json", device, commandList);

	// DW설명 : 브루트 소년단 생성 함수 호출
    SpawnBTS(device, commandList);

	// DW설명 : 플레이어 오브젝트 생성
    {
        //auto playerObject = ObjectManager::Instance()->create_game_object("MainPlayer");
        //// MainPlayerScript추가
        //playerObject->add_component<MainPlayerScript>();
        ////// RenderComponent
        //auto renderer = playerObject->add_component<RenderComponent>();

        //auto playerMesh = ResourceManager::Instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");

        //// 재질 및 쉐이더 설정
        //auto material = std::make_shared<GltfMaterial>("test_Material");
        //material->set_shader(Renderer::Instance()->get_shader("gltf"));
        //renderer->set_material(material);

        //// gltf
        //renderer->set_pso_name("gltf");

        //// 위치, 회전 정보
        //playerObject->transform()->set_local_rotation(-90.f, 0.f, 0.f);  
        //playerObject->transform()->set_local_scale({ 200.0f, 200.0f, 200.0f }); 

        //

        //playerObject->transform()->set_local_position(XMFLOAT3(0.0f, 70.0f, -150.0f));
        //ResourceManager::Instance()->upload_pending_meshes(device, commandList);
    }

}

void Chess_Scene::release_upload_buffers()
{
    ResourceManager::instance()->release_upload_buffers();
}

void Chess_Scene::SpawnBTS(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // DW설명 : 인사 애니메이션 오브젝트 생성
    {
        auto hi_brute = ObjectManager::instance()->create_game_object("Hi_animation_brute");
        // GltfAnimationTestScript추가

        hi_brute->add_component<GltfAnimationTestScript>();
        //// RenderComponent
        auto renderer = hi_brute->add_component<RenderComponent>();

        auto hi_brute_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/Animation_BruteHi/bruteHi.gltf", true);
        renderer->set_mesh(hi_brute_Mesh);

        // 재질 및 쉐이더 설정
        std::string material = "skinned_animation_brute";

        ResourceManager::instance()->create_material(material);
        ResourceManager::instance()->set_shader_for_material(material, "skinned");

        // gltf
        renderer->set_pso_name("skinned");

        // 위치, 회전 정보
        hi_brute->transform()->set_local_rotation(0.f, 0.f, 0.f);
        hi_brute->transform()->set_local_scale({ 25.0f, 25.0f, 25.0f });



        hi_brute->transform()->set_local_position(XMFLOAT3(0.0f, 25.0f, -130.0f));
        ResourceManager::instance()->upload_pending_meshes(device, commandList);
    }
    
	for (int i = 0; i < 5; ++i)
    {
        auto hi_brute = ObjectManager::instance()->create_game_object("Dance_animation_brute");
        // GltfAnimationTestScript추가

        hi_brute->add_component<GltfAnimationTestScript>();
        //// RenderComponent
        auto renderer = hi_brute->add_component<RenderComponent>();

        auto hi_brute_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/BruteDance/BruteDance.gltf", true);
        renderer->set_mesh(hi_brute_Mesh);

        // 재질 및 쉐이더 설정
        std::string material = "skinned_Dance_brute";

        ResourceManager::instance()->create_material(material);
        ResourceManager::instance()->set_shader_for_material(material, "skinned");

        // gltf
        renderer->set_pso_name("skinned");

        // 위치, 회전 정보
        hi_brute->transform()->set_local_rotation(90.f,(0.f + 45.f*i), 90.f);
        hi_brute->transform()->set_local_scale({ 25.0f, 25.0f, 25.0f });



        hi_brute->transform()->set_local_position(XMFLOAT3((-100.f + i * 50.f), 50.0f, -80.0f));
        ResourceManager::instance()->upload_pending_meshes(device, commandList);
    }
}

// =================================================================
// [���ŵ� �Լ� ����]
// - Chess_Scene::ProcessInput()
// - Chess_Scene::AnimateObjects()
// - Chess_Scene::Render()
// - Chess_Scene::Collision()
// - Chess_Scene::CreateGraphicsRootSignature()
// - ���...
// �� �Լ����� �����δ� ���� ��� ���ŵ˴ϴ�.
// =================================================================
//
//Chess_Scene::Chess_Scene(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
//{
//	//BuildObjects(pd3dDevice, pd3dCommandList);
//}
//
//Chess_Scene::~Chess_Scene()
//{
//	ReleaseObjects();
//}
//
//void Chess_Scene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
//{
//    // ���콺 ó���� �����
//    if (InputManager::Instance()->GetIsShowCusor())
//    {
//        InputManager::Instance()->ChangeShowCusor();
//    }
//
//    _SignatureNum = 2;
//    _AllRootSignature.resize(_SignatureNum);
//   
//    _AllRootSignature[0] = CreateGraphicsRootSignature(pd3dDevice); // �Ϲ�
//    _AllRootSignature[1] = CreateSkinnedGraphicsRootSignature(pd3dDevice); // GLB
//
//    MakeSrv(pd3dDevice); // Srv ��ũ���� ����
//    MakeDummyBonebuffer(pd3dDevice, pd3dCommandList); // ���� ����
//    // ���̴� ����
//
//    // ���� ������Ʈ ���̴�
//    _AllShaders.push_back(std::make_shared<CObjectsShader>());
//    _AllShaders.back()->CreateShader(pd3dDevice, _AllRootSignature[0].Get());
//
//    _AllShaders.push_back(std::make_shared<GlbShader>());
//    _AllShaders.back()->CreateShader(pd3dDevice, _AllRootSignature[1].Get());
//    // ��� �߾���~
//    _AllShaders.push_back(std::make_shared<DebugShader>());
//    _AllShaders.back()->CreateShader(pd3dDevice, _AllRootSignature[0].Get()); // ����� ���̴��� �Ϲ� ��Ʈ �ñ״�ó ���
//
//    m_pDebugShader = _AllShaders.back().get(); // ��� ������ ������ ����
// 
//    // ī�޶� ����
//    m_ChessCamera = new FreeCamera{};
//    m_pCamera = m_ChessCamera;
//    m_ChessCamera->SetCameraMode(CAMERA_MODE::CAMERA_THIRD_PERSON);
//    m_ChessCamera->SetOffset(5.f);
//    m_ChessCamera->Rotate(30.f,0.f,0.f);
//
//    if (m_pCamera)
//    {
//        m_pCamera->CreateShaderVariables(pd3dDevice, pd3dCommandList);
//    }
//
//    m_ChessCamera->SetPosition(0.f, 5.f, -5.f);
//
//    LoadSceneFromFile("Resource/MapData/ExportedClientData.json", pd3dDevice, pd3dCommandList);
//
//    // ������ ����
//    std::shared_ptr<GameObject> Board{};
//    std::shared_ptr<Mesh> BoardMesh{};
//    float MoveDistance{};
//
//    //for (int i = 0; i < 8; ++i) // ����
//    //{
//    //    for (int j = 0; j < 8; ++j) // ����
//    //    {
//    //        Board = std::make_shared<BoardCube>();
//    //        Board->CreateShaderVariables(pd3dDevice, pd3dCommandList); // ��� ���� ���� ���� �߰�
//    //        BoardMesh = new ReadObjMesh{ pd3dDevice,pd3dCommandList,"Resource/Cube_Normal.obj" };
//    //        if ((j + i) % 2)
//    //        {
//    //            BoardMesh->ChangeColor(pd3dCommandList, 0.941f, 0.851f, 0.710f, 1.f);
//    //        }
//    //        else
//    //        {
//    //            BoardMesh->ChangeColor(pd3dCommandList, 0.710f, 0.533f, 0.388f, 1.f);
//    //        }
//    //        Board->SetMesh(BoardMesh);
//    //       
//    //    
//    //        Board->SetPosition(((Board->m_pMesh->m_Right - Board->m_pMesh->m_Left) * Board->GetSize().x * j),
//    //            -(Board->m_pMesh->m_Top - Board->m_pMesh->m_Bottom) * Board->GetSize().y * 0.5f,
//    //            ((Board->m_pMesh->m_Front - Board->m_pMesh->m_Back) * Board->GetSize().z * i));
//    //        Board->m_PosX = j;
//    //        Board->m_PosY = i;
//    //        ObjectManager::Instance()->PushFloorObject(Board);
//    //    }
//    //}
//
//    Board = std::make_shared<BoardCube>();
//	auto Board_Render = Board->get_component<RenderComponent>();
//  
//    BoardMesh = std::make_shared<ReadGlbMesh>( pd3dDevice,pd3dCommandList,"Resource/MapData/SM_Crate_01.glb", (Scene*)this);
//
//    if (Board_Render)
//    {
//        auto materialShader = std::make_shared<Material_Shader>();
//        Board_Render->set_material(materialShader);
//
//        Board_Render->CreateShaderVariables(pd3dDevice, pd3dCommandList);
//        Board_Render->set_mesh(BoardMesh);
//        Board_Render->set_shader(_AllShaders[1]); // GLB
//        Board_Render->get_material_shader()->set_shader_root_signature(_AllRootSignature[1].Get());
//    }
//	auto BoardObjaect_Transform = Board->get_component<TransformComponent>();
//    if (BoardObjaect_Transform) 
//    {
//        BoardObjaect_Transform->set_position(((Board_Render->get_mesh()->m_Right - Board_Render->get_mesh()->m_Left) * BoardObjaect_Transform->get_size().x),
//            1.0f,
//            ((Board_Render->get_mesh()->m_Front - Board_Render->get_mesh()->m_Back) * BoardObjaect_Transform->get_size().z));
//    }
//    Board->_posX = 0;
//    Board->_posY = 0;
//    ObjectManager::Instance()->PushFloorObject(Board);
//
//    // �𸮾󿡼� ���� FBX �׽�Ʈ
//    _fbxObject = std::make_shared<BoardCube>();
//	auto fbxObject_Render = _fbxObject->get_component<RenderComponent>();
//    _collisionMesh = std::make_shared<ReadFbxMesh>(pd3dDevice,pd3dCommandList,"Resource/Test/TestCollision.fbx");
//
//    if (fbxObject_Render)
//    {
//        fbxObject_Render->CreateShaderVariables(pd3dDevice, pd3dCommandList); // ��� ���� ���� ���� �߰�
//        fbxObject_Render->set_mesh(_collisionMesh);
//    }
//    XMFLOAT3 Scale = XMFLOAT3(0.01f, 0.01f, 0.01f);
//    auto fbxObject_Transform = _fbxObject->get_component<TransformComponent>();
//    if (fbxObject_Transform)
//    {
//        fbxObject_Transform->set_scale(Scale.x, Scale.y, Scale.z);
//        fbxObject_Transform->set_position(((fbxObject_Render->get_mesh()->m_Right - fbxObject_Render->get_mesh()->m_Left) * fbxObject_Transform->get_size().x + 3),
//            0.8f,
//            ((fbxObject_Render->get_mesh()->m_Front - fbxObject_Render->get_mesh()->m_Back) * fbxObject_Transform->get_size().z) + 5);
//    }
//    Board->_posX = 0;
//    Board->_posY = 0;
//    ObjectManager::Instance()->PushFloorObject(_fbxObject);
//
//    // ----------------------------------------------------------------------------------------------------------------------------------------------
//    // collision ����� �ڵ�
//
//    std::shared_ptr<ReadFbxMesh> fbxMesh = dynamic_pointer_cast<ReadFbxMesh>(_collisionMesh);
//
//    if (_collisionMesh)
//    {
//        const auto& collisionPrimitives = _collisionMesh->GetCollisionPrimitives();
//        debugObjects.clear(); // ���� ������ Ŭ����
//
//        // CollisionPrimitive ������ŭ ����� ������Ʈ�� �̸� ����
//        for (const auto& primitive : collisionPrimitives)
//        {
//            // OBB, AABB, Wireframe ������Ʈ 3���� ������ �ϰ� ���Ϳ� �߰�
//            // ��ġ ����� Render �Լ����� �� ������ �����ϹǷ� ���⼭�� ����
//            auto debugOOBBObject = std::make_shared<BoardCube>();
//			auto debugOOBB_Render = debugOOBBObject->get_component<RenderComponent>();
//            if (debugOOBB_Render)
//            {
//                debugOOBB_Render->CreateShaderVariables(pd3dDevice, pd3dCommandList); // ��� ���� ���� ���� �߰�
//                debugOOBB_Render->set_mesh(std::make_shared<DebugCollisionBox>(pd3dDevice, pd3dCommandList, XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)));
//            }
//            debugObjects.push_back(debugOOBBObject);
//
//            auto debugAABBObject = std::make_shared<BoardCube>();
//			auto debugAABB_Render = debugAABBObject->get_component<RenderComponent>();
//            if (debugOOBB_Render)
//            {
//                debugAABB_Render->CreateShaderVariables(pd3dDevice, pd3dCommandList); // ��� ���� ���� ���� �߰�
//                debugAABB_Render->set_mesh(std::make_shared<DebugCollisionBox>(pd3dDevice, pd3dCommandList, XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)));
//            }
//            debugObjects.push_back(debugAABBObject);
//
//            auto debugWireframeObject = std::make_shared<BoardCube>();
//			auto debugWireframe_Render = debugWireframeObject->get_component<RenderComponent>();
//            if (debugWireframe_Render)
//            {
//                debugWireframe_Render->CreateShaderVariables(pd3dDevice, pd3dCommandList); // ��� ���� ���� ���� �߰�
//                debugWireframe_Render->set_mesh(std::make_shared<DebugWireframeMesh>(pd3dDevice, pd3dCommandList, primitive._vertices, primitive._indices, XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)));
//            }
//            debugObjects.push_back(debugWireframeObject);
//        }
//    }
//
//    // ----------------------------------------------------------------------------------------------------------------------------------------------
//
//    //{
//    //    // �÷��̾� ����
//    //    std::shared_ptr<GameObject> Player = std::make_shared<CChess_King>(0, 0);
//    //    Mesh* Chess_Mesh = new ReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Chess_King.obj" };
//    //    Chess_Mesh->ChangeColor(pd3dCommandList, 1.0f, 1.0f, 1.0f, 1.f);
//    //    Player.get()->SetMesh(Chess_Mesh);
//    //    // �̵� �Ÿ� ����
//    //    static_cast<CChess_King*>(Player.get())->SetDistance(MoveDistance);
//    //    Player.get()->SetScale(1.f, 1.f, 1.f);
//    //    // �Ŵ����� �ֱ�
//    //    ObjectManager::Instance()->PushObject(Player);
//    //}
//    //{
//    //    // ���� ����
//    //    std::shared_ptr<GameObject> Other = std::make_shared<OtherPlayer>(7, 7);
//    //    Mesh* Chess_Mesh = new ReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Chess_King.obj" };
//    //    Chess_Mesh->ChangeColor(pd3dCommandList, 0.0f, 0.0f, 0.0f, 1.f);
//    //    Other.get()->SetMesh(Chess_Mesh);
//    //    // �̵� �Ÿ� ����
//    //    static_cast<OtherPlayer*>(Other.get())->SetDistance(MoveDistance);
//    //    Other.get()->SetScale(1.f, 1.f, 1.f);
//    //    // �Ŵ����� �ֱ�
//    //    ObjectManager::Instance()->PushObject(Other);
//    //}
//
//    BuildLightsAndMaterials();
//    CreateShaderVariables(pd3dDevice, pd3dCommandList);
//}
//
//void Chess_Scene::ReleaseObjects()
//{
//	ObjectManager::Instance()->DeleteAll();
//
//    for (auto& iter : _AllShaders)
//    {
//        iter->ReleaseShaderVariables();
//    }
//    _AllShaders.clear();
//
//	if (m_pCamera)
//	{
//		delete m_pCamera;
//		m_pCamera = nullptr;
//	}
//}
//
//void Chess_Scene::ProcessInput(float fElapsedTime)
//{
//    bool IsThirdPerson = (m_ChessCamera->GetCameraMode() == CAMERA_MODE::CAMERA_THIRD_PERSON);
//
//	MainPlayer* Player = nullptr;
//
//    // ESC ������ Ŀ�� Ű�� �� �� �ֵ��� ����
//    if (InputManager::Instance()->IsKeyDown(VK_ESCAPE))
//    {
//        InputManager::Instance()->ChangeShowCusor();
//    }
//
//    m_ChessCamera->ProcessInput(fElapsedTime);
//
//    if (InputManager::Instance()->IsKeyDown('B'))
//    {
//        ToggleBoundingBoxView();
//    }
//
//    std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& Arr = ObjectManager::Instance()->GetAllObject();
//
//    for (std::list<std::shared_ptr<GameObject>>& Objects : Arr) {
//        for (std::shared_ptr<GameObject>& Object : Objects) {
//            if (nullptr != Object)
//            {
//                Player = dynamic_cast<MainPlayer*>(Object.get());
//
//                if (Player) 
//                {
//                    if (IsThirdPerson)
//                    {
//                        Player->process_input(fElapsedTime);
//                    }
//                }
//                else
//                {
//                    Object->process_input(fElapsedTime);
//                }
//
//                Object->update_bounding_box();
//            }
//        }
//    }
//}
//
//void Chess_Scene::AnimateObjects(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList)
//{
//    m_pCamera->Rotate();
//    m_pCamera->Update();
//
//    std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& Arr = ObjectManager::Instance()->GetAllObject();
//    for (std::list<std::shared_ptr<GameObject>>& Objects : Arr) {
//        for (std::shared_ptr<GameObject>& Object : Objects) {
//            if (nullptr != Object)
//            {
//                Object->animate(fTimeElapsed, m_pCamera, pd3dCommandList);
//                Object->update(fTimeElapsed);
//                Object.get()->update_bounding_box();
//            }
//        }
//    }
//
//    std::list<std::shared_ptr<GameObject>>& ObjectList = ObjectManager::Instance()->GetEnemy();
//    for (std::shared_ptr<GameObject>& Object : ObjectList) {
//        if (nullptr != Object)
//        {
//			auto Object_Render = Object->get_component<RenderComponent>();
//			if (Object_Render)
//				Object_Render->get_mesh()->ChangeColor(pd3dCommandList, std::dynamic_pointer_cast<OtherPlayer>(Object)->GetHP() / 100.f,
//                1.f,
//                std::dynamic_pointer_cast<OtherPlayer>(Object)->GetHP() / 100.f,
//                1.f);
//        }
//    }
//
//    Collision(fTimeElapsed);
//    m_ChessCamera->UpdateAnimateCamera(fTimeElapsed);
//}
//
//void Chess_Scene::Render(ID3D12GraphicsCommandList* pd3dCommandList)
//{
//    m_pCamera->Update();
//    m_pCamera->SetViewportsAndScissorRects(pd3dCommandList);
//
//    ObjectManager::Instance()->MakeRenderMap(m_pCamera);
//
//    // �⺻ ��Ʈ �ñ״�ó ���� (�ؽ�ó ���� �Ϲ� ��ü��)
//    {
//        pd3dCommandList->SetGraphicsRootSignature(_AllRootSignature[0].Get());
//        // �⺻ ���̴� PSO
//        _AllShaders[0]->OnPrepareRender(pd3dCommandList);
//
//        // ���� ������ ���� (��� ��ü�� �� ������ ���� ������ ����)
//        UpdateShaderVariables(pd3dCommandList); // ����/���͸��� ������ CPU->GPU ����
//        D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
//        pd3dCommandList->SetGraphicsRootConstantBufferView(3, d3dGpuVirtualAddress);
//        d3dGpuVirtualAddress = m_pd3dcbMaterials->GetGPUVirtualAddress();
//        pd3dCommandList->SetGraphicsRootConstantBufferView(2, d3dGpuVirtualAddress);
//
//        // SRV ��ũ���� �� ���� (��� ��ü�� �� ���� ����)
//        if (_SrvDescriptorHeap)
//        {
//            ID3D12DescriptorHeap* ppd3dDescriptorHeaps[] = { _SrvDescriptorHeap.Get() };
//            pd3dCommandList->SetDescriptorHeaps(_countof(ppd3dDescriptorHeaps), ppd3dDescriptorHeaps);
//        }
//    }
//    size_t ShaderNum{};
//    for (auto const& [shader, objectGroup] : ObjectManager::Instance()->GetRenderMap())
//    {
//        // ���̴� �׷��� �ٲ� �� �� ���� ���¸� ����
//        // ���� �׷��� ù��° ���Ҹ� �������� ����
//		auto objectGroup_Render = objectGroup[0]->get_component<RenderComponent>();
//        if(objectGroup_Render) objectGroup_Render->on_prepare_render(pd3dCommandList); // PSO�� ��Ʈ �ñ״�ó�� ���⼭ ����
//
//
//        // ���� �׷�(���� ���̴� ��� �ϴ� �׷�)�� ��� ������Ʈ�� ������
//        for (const std::shared_ptr<GameObject>& Object : objectGroup)
//        {
//			auto Object_Render = Object->get_component<RenderComponent>();
//
//            if (ShaderNum == 1) // Glb
//            {
//                // ���� ������ ���� (��� ��ü�� �� ������ ���� ������ ����)
//                UpdateShaderVariables(pd3dCommandList); // ����/���͸��� ������ CPU->GPU ����
//                D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
//                pd3dCommandList->SetGraphicsRootConstantBufferView(3, d3dGpuVirtualAddress);
//                d3dGpuVirtualAddress = m_pd3dcbMaterials->GetGPUVirtualAddress();
//                pd3dCommandList->SetGraphicsRootConstantBufferView(2, d3dGpuVirtualAddress);
//
//                // A. �� ��� ��� ���� ���ε� (�ִϸ��̼ǿ�, ���� �۵�X)
//                // ReadGlbMesh�� �ִϸ��̼� �����͸� ����ִ� ��� ������ �ּҸ� ��ȯ�ؾ� �մϴ�.
//
//                auto readGlbMesh = std::dynamic_pointer_cast<ReadGlbMesh>(Object_Render->get_mesh());
//
//                if (readGlbMesh)
//                {
//                    D3D12_GPU_VIRTUAL_ADDRESS boneTransformAddress = readGlbMesh->GetBoneTransformsBufferAddress();
//                    // [����] �ּҰ� ��ȿ�ϸ� ���� ���۸�, �ƴϸ� ���� ���۸� ���ε�
//                    if (boneTransformAddress != 0)
//                    {
//                        pd3dCommandList->SetGraphicsRootConstantBufferView(4, boneTransformAddress);
//                    }
//                    else
//                    {
//                        // m_pDummyBoneBuffer�� Scene�̳� �������� �ϳ��� ������ ������ ����
//                        pd3dCommandList->SetGraphicsRootConstantBufferView(4, GetDummyBoneBufferAddress());
//                    }
//
//                    // B. �ؽ�ó SRV ���̺� ���ε�
//                    // ReadGlbMesh�� �ε� �� ������ SRV�� GPU �ڵ��� ��ȯ�ؾ� �մϴ�.
//                    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = readGlbMesh->GetSrvGpuHandle();
//                    if (textureSrvHandle.ptr != 0)
//                    {
//                        // ��Ʈ �ñ״�ó�� ���ǵ� SRV ���̺� ����(��: 5��)�� �ؽ�ó�� ���ε��մϴ�.
//                        // �� ����(5)�� CreateSkinnedGraphicsRootSignature���� SRV ���̺��� ������ �ε����� ��ġ�ؾ� �մϴ�.
//                        pd3dCommandList->SetGraphicsRootDescriptorTable(5, textureSrvHandle);
//                    }
//                    //Object->Render(pd3dCommandList, m_pCamera); // ������
//                }
//                else
//                {
//                    pd3dCommandList->SetGraphicsRootConstantBufferView(4, GetDummyBoneBufferAddress());
//                }
//            }
//			if (Object_Render) Object_Render->render(pd3dCommandList,m_pCamera);
//        }
//        ++ShaderNum;
//    }
//
//    /*std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& Arr = ObjectManager::Instance()->GetAllObject();
//
//    for (std::list<std::shared_ptr<GameObject>>& Objects : Arr) {
//        for (std::shared_ptr<GameObject>& Object : Objects) {
//            if (nullptr != Object)
//            {
//                Object->Render(pd3dCommandList, m_pCamera);
//            }
//        }
//    }*/
//
//    if (isRenderFbxFileBoundingBoxes && _collisionMesh && _fbxObject)
//    {
//        pd3dCommandList->SetGraphicsRootSignature(_AllRootSignature[0].Get());
//        m_pDebugShader->OnPrepareRender(pd3dCommandList);
//        m_pCamera->UpdateShaderVariables(pd3dCommandList);
//
//        const auto& collisionPrimitives = _collisionMesh->GetCollisionPrimitives();
//        XMMATRIX parentWorld = XMLoadFloat4x4(&_fbxObject->get_component<TransformComponent>()->get_world_matrix());
//
//        for (size_t i = 0; i < collisionPrimitives.size(); ++i)
//        {
//            const auto& primitive = collisionPrimitives[i];
//
//            std::shared_ptr<GameObject>& debugOOBBObject = debugObjects[i * 3 + 0];
//            std::shared_ptr<GameObject>& debugAABBObject = debugObjects[i * 3 + 1];
//            std::shared_ptr<GameObject>& debugWireframeObject = debugObjects[i * 3 + 2];
//
//            // 1. OBB ��ġ ����
//            XMMATRIX oobb_S = XMMatrixScaling(primitive.oobb.Extents.x * 2.0f, primitive.oobb.Extents.y * 2.0f, primitive.oobb.Extents.z * 2.0f);
//            XMVECTOR oobb_quat = XMQuaternionNormalize(XMLoadFloat4(&primitive.oobb.Orientation));
//            XMMATRIX oobb_R = XMMatrixRotationQuaternion(oobb_quat);
//            XMMATRIX oobb_T = XMMatrixTranslation(primitive.oobb.Center.x, primitive.oobb.Center.y, primitive.oobb.Center.z);
//            XMStoreFloat4x4(&debugOOBBObject->get_component<TransformComponent>()->get_world_matrix(), (oobb_S * oobb_R * oobb_T) * parentWorld);
//
//            // 2. AABB ��ġ ����
//            XMMATRIX aabb_S = XMMatrixScaling(primitive.aabb.Extents.x * 2.0f, primitive.aabb.Extents.y * 2.0f, primitive.aabb.Extents.z * 2.0f);
//            XMMATRIX aabb_T = XMMatrixTranslation(primitive.aabb.Center.x, primitive.aabb.Center.y, primitive.aabb.Center.z);
//            XMStoreFloat4x4(&debugAABBObject->get_component<TransformComponent>()->get_world_matrix(), (aabb_S * aabb_T) * parentWorld);
//
//            // 3. ���̾������� ��ġ ����
//            debugWireframeObject->get_component<TransformComponent>()->get_world_matrix() = _fbxObject->get_component<TransformComponent>()->get_world_matrix();
//
//            debugObjects[i * 3 + 0]->get_component<RenderComponent>()->render(pd3dCommandList, m_pCamera); // OBB
//            debugObjects[i * 3 + 1]->get_component<RenderComponent>()->render(pd3dCommandList, m_pCamera); // AABB
//            debugObjects[i * 3 + 2]->get_component<RenderComponent>()->render(pd3dCommandList, m_pCamera); // Wireframe
//        }
//    }
//}
//
//void Chess_Scene::Collision(float fElapsedTime)
//{
//    std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& Arr = ObjectManager::Instance()->GetAllObject();
//
//    for (std::list<std::shared_ptr<GameObject>>& Objects : Arr) {
//        for (std::shared_ptr<GameObject>& Object : Objects) {
//            if (nullptr != Object)
//            {
//                Object->collision(fElapsedTime);
//            }
//        }
//    }
//}
//
//
