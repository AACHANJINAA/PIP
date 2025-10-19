#include "stdafx.h"
#include "Chess_Scene.h"

#include "FreeCameraScript.h"
#include "ObjectManager.h"
#include "GameObject.h"

#include "MainPlayerScript.h"
#include "TransformComponent.h"
#include "RenderComponent.h"
#include "Mesh.h" // 메시 클래스가 필요할 수 있음
#include "ResourceManager.h"
// #include "PlayerScript.h" // 앞으로 만들 스크립트들

#include "GltfTestScript.h"

#include "GltfMaterial.h"   
#include "TextureManager.h" 
#include "Renderer.h"       

void Chess_Scene::build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{

    // --- 기존 코드 (카메라, 플레이어, 맵 등 생성) ---
    auto cameraObject = ObjectManager::Instance()->create_game_object("FreeCamera");
    cameraObject->add_component<FreeCameraScript>();
    cameraObject->transform()->set_local_position(XMFLOAT3(0.0f, 70.0f, -200.0f));

	load_scene_from_file("Resource/DDSMapData/ExportedClientData.json", device, commandList);
    ResourceManager::Instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");
    //auto playerObject = ObjectManager::Instance()->create_game_object("MainPlayer");
    //
    // // 1. RenderComponent를 먼저 추가합니다.
    // auto renderer = playerObject->add_component<RenderComponent>();
    //
    // // 2. 메시를 로드하고 렌더러에 설정합니다.
    // auto playerMesh = ResourceManager::Instance()->load_mesh("Resource/Character/Untitled.gltf");
    // renderer->set_mesh(playerMesh);
    // renderer->set_pso_name("gltf");
    //
    //// 3. 재질과 텍스처를 설정합니다.
    //auto material = std::make_shared<GltfMaterial>("player_material");
    // material->set_shader(Renderer::Instance()->get_shader("gltf"));
    // auto texture = TextureManager::Instance()->load_texture(
    //         "Resource/Character/tripo_image_bcd92247-4811-4638-8423-8e0b70612c6a_0.dds",
    //         commandList
    //);
    // if (texture)
    //     {
    //         material->set_texture(texture, 0);
    //     }
    // renderer->set_material(material);
    //
    //// 4. MainPlayerScript를 추가합니다. (이제 렌더링 설정은 하지 않음)
    //playerObject->add_component<MainPlayerScript>();
    //
    //// 5. 초기 위치를 설정합니다.
    //playerObject->transform()->set_local_position(XMFLOAT3(0.0f, 70.0f, -150.0f));

    /*auto playerObject = ObjectManager::Instance()->create_game_object("MainPlayer");
    playerObject->add_component<MainPlayerScript>();

    auto mapObject = ObjectManager::Instance()->create_game_object("Crate");
    auto map_renderer = mapObject->add_component<RenderComponent>();
    map_renderer->set_mesh(ResourceManager::Instance()->load_mesh("Resource/MapData/SM_Crate_01.glb"));
    map_renderer->set_pso_name("skinned");
    mapObject->transform()->set_local_position(XMFLOAT3(0.0f, 0.0f, 5.0f));*/

    //// --- [테스트용 큐브 추가] ---
    //auto testCubeObject = ObjectManager::Instance()->create_game_object("TestCube");
    //
    //// 1. 렌더 컴포넌트 추가
    //auto cube_renderer = testCubeObject->add_component<RenderComponent>();
    //
    //// 2. 리소스 매니저를 통해 큐브 메쉬 로드 및 설정
    //cube_renderer->set_mesh(ResourceManager::Instance()->load_mesh("Resource/Cube_Normal.obj"));
    //
    //// 3. .obj 파일이므로 "default" 셰이더(PSO)를 사용하도록 설정
    //cube_renderer->set_pso_name("debug");
    //
    //// 4. 큐브 위치 설정 (카메라에 잘 보이도록)
    //testCubeObject->transform()->set_local_position(XMFLOAT3(0.0f, 0.0f, 6.0f));
    //testCubeObject->transform()->set_local_scale({2.0f, 2.0f, 2.0f}); // 크기를 키워 잘 보이게 함



    //// --- [테스트용 캐논 추가] --- <- 하나씩 띄워볼때 이거쓰기
    //{
    //    auto test_cannon_object = ObjectManager::Instance()->create_game_object("TestCannon");

    //    // 1. 렌더 컴포넌트 추가
    //    auto cannon_renderer = test_cannon_object->add_component<RenderComponent>();
    //    test_cannon_object->add_component<GltfTestScript>();
    //    // 2. 리소스 매니저를 통해 큐브 메쉬 로드 및 설정
    //    cannon_renderer->set_mesh(ResourceManager::Instance()->load_mesh("Resource/DDSMapData/Meshes/old_cannon.gltf"));

    //    // gltf
    //    cannon_renderer->set_pso_name("gltf");

    //    // 위치
    //    test_cannon_object->transform()->set_local_position(XMFLOAT3(3.0f, 0.0f, 6.0f));
    //    test_cannon_object->transform()->set_local_scale({ 2.0f, 2.0f, 2.0f }); // 크기를 키워 잘 보이게 함
    //}
   // {
        //auto test_cannon_object = ObjectManager::Instance()->create_game_object("TestCannon");

    //    // 1. 렌더 컴포넌트 추가
    //    auto cannon_renderer = test_cannon_object->add_component<GltfRenderComponent>();

    //    // 2. 리소스 매니저를 통해 큐브 메쉬 로드 및 설정
    //    cannon_renderer->set_mesh(ResourceManager::Instance()->load_mesh("Resource/DDSMapData/Meshes/old_cannon.gltf"));

    //    // gltf
    //    cannon_renderer->set_pso_name("gltf");

    //    // 위치
    //    test_cannon_object->transform()->set_local_position(XMFLOAT3(0.0f, 0.0f, 6.0f));
    //    test_cannon_object->transform()->set_local_scale({ 2.0f, 2.0f, 2.0f }); // 크기를 키워 잘 보이게 함
    //}

 //   // [추가] 씬에 카메라를 생성합니다.
 //   auto cameraObject = ObjectManager::Instance()->create_game_object("FreeCamera");
 //   cameraObject->add_component<FreeCameraScript>(); // 스크립트가 CameraComponent를 자동으로 추가하고 초기화합니다.
	//cameraObject->transform()->set_local_position(XMFLOAT3(0.0f, 5.0f, -10.0f));
 //   // --- 조명 생성 (예시) ---
	//// 조명은 보통 보이지 않으므로 RenderComponent가 필요 없습니다.
 //   //auto lightObject = ObjectManager::Instance()->create_game_object("DirectionalLight");
 //   //lightObject->add_component<LightComponent>(); // TODO: LightComponent를 만든다면 부착

 //   // --- 플레이어 생성 ---
 //   auto playerObject = ObjectManager::Instance()->create_game_object("MainPlayer");
 //   playerObject->add_component<MainPlayerScript>(); // 스크립트를 부착하면 awake()에서 모든 설정이 자동으로 이루어집니다.

	//// --- 맵 오브젝트 생성 (JSON 또는 파일 로딩 로직이 여기로 올 수 있습니다) ---
	//auto mapObject = ObjectManager::Instance()->create_game_object("Crate");
 //   auto renderer = mapObject->add_component<RenderComponent>();

 //   renderer->set_mesh(ResourceManager::Instance()->load_mesh("Resource/MapData/SM_Crate_01.glb"));
 //   renderer->set_pso_name("skinned"); // GLB 파일이므로 skinned PSO 사용

 //   mapObject->transform()->set_local_position(XMFLOAT3(0.0f, 0.0f, 5.0f));
}

void Chess_Scene::release_upload_buffers()
{
    ResourceManager::Instance()->release_upload_buffers();
}

// =================================================================
// [제거된 함수 구현]
// - Chess_Scene::ProcessInput()
// - Chess_Scene::AnimateObjects()
// - Chess_Scene::Render()
// - Chess_Scene::Collision()
// - Chess_Scene::CreateGraphicsRootSignature()
// - 등등...
// 위 함수들의 구현부는 이제 모두 제거됩니다.
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
//    // 마우스 처음에 숨기기
//    if (InputManager::Instance()->GetIsShowCusor())
//    {
//        InputManager::Instance()->ChangeShowCusor();
//    }
//
//    _SignatureNum = 2;
//    _AllRootSignature.resize(_SignatureNum);
//   
//    _AllRootSignature[0] = CreateGraphicsRootSignature(pd3dDevice); // 일반
//    _AllRootSignature[1] = CreateSkinnedGraphicsRootSignature(pd3dDevice); // GLB
//
//    MakeSrv(pd3dDevice); // Srv 디스크립터 생성
//    MakeDummyBonebuffer(pd3dDevice, pd3dCommandList); // 더미 생성
//    // 셰이더 생성
//
//    // 기존 오브젝트 셰이더
//    _AllShaders.push_back(std::make_shared<CObjectsShader>());
//    _AllShaders.back()->CreateShader(pd3dDevice, _AllRootSignature[0].Get());
//
//    _AllShaders.push_back(std::make_shared<GlbShader>());
//    _AllShaders.back()->CreateShader(pd3dDevice, _AllRootSignature[1].Get());
//    // 대원 잘쓸게~
//    _AllShaders.push_back(std::make_shared<DebugShader>());
//    _AllShaders.back()->CreateShader(pd3dDevice, _AllRootSignature[0].Get()); // 디버그 셰이더는 일반 루트 시그니처 사용
//
//    m_pDebugShader = _AllShaders.back().get(); // 멤버 변수에 포인터 저장
// 
//    // 카메라 생성
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
//    // 보드판 생성
//    std::shared_ptr<GameObject> Board{};
//    std::shared_ptr<Mesh> BoardMesh{};
//    float MoveDistance{};
//
//    //for (int i = 0; i < 8; ++i) // 세로
//    //{
//    //    for (int j = 0; j < 8; ++j) // 가로
//    //    {
//    //        Board = std::make_shared<BoardCube>();
//    //        Board->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가
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
//    // 언리얼에서 뽑은 FBX 테스트
//    _fbxObject = std::make_shared<BoardCube>();
//	auto fbxObject_Render = _fbxObject->get_component<RenderComponent>();
//    _collisionMesh = std::make_shared<ReadFbxMesh>(pd3dDevice,pd3dCommandList,"Resource/Test/TestCollision.fbx");
//
//    if (fbxObject_Render)
//    {
//        fbxObject_Render->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가
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
//    // collision 디버깅 코드
//
//    std::shared_ptr<ReadFbxMesh> fbxMesh = dynamic_pointer_cast<ReadFbxMesh>(_collisionMesh);
//
//    if (_collisionMesh)
//    {
//        const auto& collisionPrimitives = _collisionMesh->GetCollisionPrimitives();
//        debugObjects.clear(); // 이전 데이터 클리어
//
//        // CollisionPrimitive 개수만큼 디버그 오브젝트를 미리 생성
//        for (const auto& primitive : collisionPrimitives)
//        {
//            // OBB, AABB, Wireframe 오브젝트 3개를 생성만 하고 벡터에 추가
//            // 위치 계산은 Render 함수에서 매 프레임 수행하므로 여기서는 안함
//            auto debugOOBBObject = std::make_shared<BoardCube>();
//			auto debugOOBB_Render = debugOOBBObject->get_component<RenderComponent>();
//            if (debugOOBB_Render)
//            {
//                debugOOBB_Render->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가
//                debugOOBB_Render->set_mesh(std::make_shared<DebugCollisionBox>(pd3dDevice, pd3dCommandList, XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)));
//            }
//            debugObjects.push_back(debugOOBBObject);
//
//            auto debugAABBObject = std::make_shared<BoardCube>();
//			auto debugAABB_Render = debugAABBObject->get_component<RenderComponent>();
//            if (debugOOBB_Render)
//            {
//                debugAABB_Render->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가
//                debugAABB_Render->set_mesh(std::make_shared<DebugCollisionBox>(pd3dDevice, pd3dCommandList, XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)));
//            }
//            debugObjects.push_back(debugAABBObject);
//
//            auto debugWireframeObject = std::make_shared<BoardCube>();
//			auto debugWireframe_Render = debugWireframeObject->get_component<RenderComponent>();
//            if (debugWireframe_Render)
//            {
//                debugWireframe_Render->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가
//                debugWireframe_Render->set_mesh(std::make_shared<DebugWireframeMesh>(pd3dDevice, pd3dCommandList, primitive._vertices, primitive._indices, XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)));
//            }
//            debugObjects.push_back(debugWireframeObject);
//        }
//    }
//
//    // ----------------------------------------------------------------------------------------------------------------------------------------------
//
//    //{
//    //    // 플레이어 생성
//    //    std::shared_ptr<GameObject> Player = std::make_shared<CChess_King>(0, 0);
//    //    Mesh* Chess_Mesh = new ReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Chess_King.obj" };
//    //    Chess_Mesh->ChangeColor(pd3dCommandList, 1.0f, 1.0f, 1.0f, 1.f);
//    //    Player.get()->SetMesh(Chess_Mesh);
//    //    // 이동 거리 설정
//    //    static_cast<CChess_King*>(Player.get())->SetDistance(MoveDistance);
//    //    Player.get()->SetScale(1.f, 1.f, 1.f);
//    //    // 매니저에 넣기
//    //    ObjectManager::Instance()->PushObject(Player);
//    //}
//    //{
//    //    // 상대방 생성
//    //    std::shared_ptr<GameObject> Other = std::make_shared<OtherPlayer>(7, 7);
//    //    Mesh* Chess_Mesh = new ReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Chess_King.obj" };
//    //    Chess_Mesh->ChangeColor(pd3dCommandList, 0.0f, 0.0f, 0.0f, 1.f);
//    //    Other.get()->SetMesh(Chess_Mesh);
//    //    // 이동 거리 설정
//    //    static_cast<OtherPlayer*>(Other.get())->SetDistance(MoveDistance);
//    //    Other.get()->SetScale(1.f, 1.f, 1.f);
//    //    // 매니저에 넣기
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
//    // ESC 누르면 커서 키고 끌 수 있도록 수정
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
//    // 기본 루트 시그니처 설정 (텍스처 없는 일반 객체용)
//    {
//        pd3dCommandList->SetGraphicsRootSignature(_AllRootSignature[0].Get());
//        // 기본 셰이더 PSO
//        _AllShaders[0]->OnPrepareRender(pd3dCommandList);
//
//        // 전역 데이터 설정 (모든 객체가 이 조명과 재질 정보를 공유)
//        UpdateShaderVariables(pd3dCommandList); // 조명/머터리얼 데이터 CPU->GPU 복사
//        D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
//        pd3dCommandList->SetGraphicsRootConstantBufferView(3, d3dGpuVirtualAddress);
//        d3dGpuVirtualAddress = m_pd3dcbMaterials->GetGPUVirtualAddress();
//        pd3dCommandList->SetGraphicsRootConstantBufferView(2, d3dGpuVirtualAddress);
//
//        // SRV 디스크립터 힙 설정 (모든 객체가 이 힙을 공유)
//        if (_SrvDescriptorHeap)
//        {
//            ID3D12DescriptorHeap* ppd3dDescriptorHeaps[] = { _SrvDescriptorHeap.Get() };
//            pd3dCommandList->SetDescriptorHeaps(_countof(ppd3dDescriptorHeaps), ppd3dDescriptorHeaps);
//        }
//    }
//    size_t ShaderNum{};
//    for (auto const& [shader, objectGroup] : ObjectManager::Instance()->GetRenderMap())
//    {
//        // 셰이더 그룹이 바뀔 때 한 번만 상태를 설정
//        // 같은 그룹의 첫번째 원소를 기준으로 설정
//		auto objectGroup_Render = objectGroup[0]->get_component<RenderComponent>();
//        if(objectGroup_Render) objectGroup_Render->on_prepare_render(pd3dCommandList); // PSO와 루트 시그니처를 여기서 설정
//
//
//        // 현재 그룹(같은 셰이더 사용 하는 그룹)의 모든 오브젝트를 렌더링
//        for (const std::shared_ptr<GameObject>& Object : objectGroup)
//        {
//			auto Object_Render = Object->get_component<RenderComponent>();
//
//            if (ShaderNum == 1) // Glb
//            {
//                // 전역 데이터 설정 (모든 객체가 이 조명과 재질 정보를 공유)
//                UpdateShaderVariables(pd3dCommandList); // 조명/머터리얼 데이터 CPU->GPU 복사
//                D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
//                pd3dCommandList->SetGraphicsRootConstantBufferView(3, d3dGpuVirtualAddress);
//                d3dGpuVirtualAddress = m_pd3dcbMaterials->GetGPUVirtualAddress();
//                pd3dCommandList->SetGraphicsRootConstantBufferView(2, d3dGpuVirtualAddress);
//
//                // A. 뼈 행렬 상수 버퍼 바인딩 (애니메이션용, 아직 작동X)
//                // ReadGlbMesh가 애니메이션 데이터를 담고있는 상수 버퍼의 주소를 반환해야 합니다.
//
//                auto readGlbMesh = std::dynamic_pointer_cast<ReadGlbMesh>(Object_Render->get_mesh());
//
//                if (readGlbMesh)
//                {
//                    D3D12_GPU_VIRTUAL_ADDRESS boneTransformAddress = readGlbMesh->GetBoneTransformsBufferAddress();
//                    // [수정] 주소가 유효하면 실제 버퍼를, 아니면 더미 버퍼를 바인딩
//                    if (boneTransformAddress != 0)
//                    {
//                        pd3dCommandList->SetGraphicsRootConstantBufferView(4, boneTransformAddress);
//                    }
//                    else
//                    {
//                        // m_pDummyBoneBuffer는 Scene이나 렌더러가 하나쯤 가지고 있으면 좋음
//                        pd3dCommandList->SetGraphicsRootConstantBufferView(4, GetDummyBoneBufferAddress());
//                    }
//
//                    // B. 텍스처 SRV 테이블 바인딩
//                    // ReadGlbMesh가 로딩 시 생성한 SRV의 GPU 핸들을 반환해야 합니다.
//                    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = readGlbMesh->GetSrvGpuHandle();
//                    if (textureSrvHandle.ptr != 0)
//                    {
//                        // 루트 시그니처에 정의된 SRV 테이블 슬롯(예: 5번)에 텍스처를 바인딩합니다.
//                        // 이 숫자(5)는 CreateSkinnedGraphicsRootSignature에서 SRV 테이블을 설정한 인덱스와 일치해야 합니다.
//                        pd3dCommandList->SetGraphicsRootDescriptorTable(5, textureSrvHandle);
//                    }
//                    //Object->Render(pd3dCommandList, m_pCamera); // 디버깅용
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
//            // 1. OBB 위치 갱신
//            XMMATRIX oobb_S = XMMatrixScaling(primitive.oobb.Extents.x * 2.0f, primitive.oobb.Extents.y * 2.0f, primitive.oobb.Extents.z * 2.0f);
//            XMVECTOR oobb_quat = XMQuaternionNormalize(XMLoadFloat4(&primitive.oobb.Orientation));
//            XMMATRIX oobb_R = XMMatrixRotationQuaternion(oobb_quat);
//            XMMATRIX oobb_T = XMMatrixTranslation(primitive.oobb.Center.x, primitive.oobb.Center.y, primitive.oobb.Center.z);
//            XMStoreFloat4x4(&debugOOBBObject->get_component<TransformComponent>()->get_world_matrix(), (oobb_S * oobb_R * oobb_T) * parentWorld);
//
//            // 2. AABB 위치 갱신
//            XMMATRIX aabb_S = XMMatrixScaling(primitive.aabb.Extents.x * 2.0f, primitive.aabb.Extents.y * 2.0f, primitive.aabb.Extents.z * 2.0f);
//            XMMATRIX aabb_T = XMMatrixTranslation(primitive.aabb.Center.x, primitive.aabb.Center.y, primitive.aabb.Center.z);
//            XMStoreFloat4x4(&debugAABBObject->get_component<TransformComponent>()->get_world_matrix(), (aabb_S * aabb_T) * parentWorld);
//
//            // 3. 와이어프레임 위치 갱신
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
