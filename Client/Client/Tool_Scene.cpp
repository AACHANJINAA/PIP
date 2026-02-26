#include "stdafx.h"
#include "Tool_Scene.h"
#include "ObjectManager.h"
#include "InputManager.h"
#include "SceneManager.h"
#include "ToolCameraScript.h"
#include "ImGuiManager.h"
#include "ResourceManager.h"
#include "ReadGLTFMesh.h"
#include "AnimationComponent.h"
#include "SocketComponenet.h"

void Tool_Scene::build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    SpawnCamera();
}

void Tool_Scene::release_upload_buffers()
{

}

void Tool_Scene::scene_process(float deltaTime)
{
    if (InputManager::instance()->IsKeyDown('T'))
    {
        // 씬 매니저에게 툴 씬으로 넘어가라고 요청합니다.
        SceneManager::instance()->change_scene("ChessScene");
    }
    // ==========================================
    // ImGui 에디터 창 그리기
    // ==========================================
    ImGui::Begin("Socket Weapon Editor");

    ImGui::Text("1. Character Setup");
    ImGui::Separator();

    // 파일 로드 버튼
    if (ImGui::Button("Load Character (glTF)"))
    {
        std::string filePath = OpenFileDialog();
        if (!filePath.empty())
        {
            m_loadedCharacterPath = filePath;

            // 1. 기존에 캐릭터가 있다면 삭제 (메모리 누수 방지)
            if (m_targetCharacter)
            {
                m_targetCharacter->destroy();
                m_targetCharacter.reset();
            }

            // 2. 새 게임 오브젝트 생성
            m_targetCharacter = ObjectManager::instance()->create_game_object("Editor_Character");
            
            // 메시 로드
            auto mesh = ResourceManager::instance()->load_mesh(filePath, true);

            // 3. 렌더 컴포넌트 부착 및 메쉬 로드 (애니메이션 메쉬로 가정)
            m_targetCharacter->add_glTF_conponent_pack(); // 이 함수가 애니메이션과 소켓 컴포넌트 추가함

            // 렌더러에 메시 등록
            auto renderer = m_targetCharacter->get_component<RenderComponent>();
            renderer->set_mesh(mesh);

            // 애니메이션 컴포넌트 기본설정(T_POSE)
            auto animation_renderer = m_targetCharacter->get_component<AnimationComponent>();
            animation_renderer->add_state_mapping(common::packet::OBJECT_STATE::T_POSE, "t_pose", mesh);
            animation_renderer->set_state(common::packet::OBJECT_STATE::T_POSE);

            // 재질설정
            std::string material = "glTF_Test_material";

            ResourceManager::instance()->create_material(material);
            ResourceManager::instance()->set_shader_for_material(material, "skinned");

            // 스키닝 애니메이션 pso 설정     
            renderer->set_pso_name("skinned");

            // 위치, 회전 정보
            m_targetCharacter->transform()->set_local_rotation(0.f, 0.f, 0.f);
            m_targetCharacter->transform()->set_local_scale({ 1.0f, 1.0f, 1.0f });
            m_targetCharacter->transform()->set_local_position(XMFLOAT3(0.0, 0.0f, 0.0f));
           

			////////////////////////////////// 테스트용 하이브루트 생성 (나중에 삭제)
            //{
            //    auto hi_brute = ObjectManager::instance()->create_game_object("SK_MagicConstruct");

            //    // 메쉬 설정
            //    auto hi_brute_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/SK_MagicConstruct/SK_MagicConstruct.gltf", true);
            //    // 메쉬에 맞는 애니메이션 추가
            //    ReadGLTFMesh* gltf_mesh = static_cast<ReadGLTFMesh*>(hi_brute_Mesh.get());
            //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Dodge.gltf");
            //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack03.gltf", "attack");
            //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack02.gltf");
            //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack01.gltf");
            //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack.gltf");
            //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Stun.gltf");
            //    gltf_mesh->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Roar.gltf");

            //    // 렌더 컴포넌트 추가
            //    renderer = hi_brute->add_component<RenderComponent>();
            //    renderer->set_mesh(hi_brute_Mesh);

            //    // 애니메이션 컴포넌트 추가
            //    hi_brute->add_glTF_conponent_pack(); // 이 함수가 애니메이션과 소켓 컴포넌트 추가함

            //    animation_renderer = hi_brute->get_component<AnimationComponent>();
            //    animation_renderer->add_state_mapping(common::packet::OBJECT_STATE::IDLE, "hi_brute_mesh", hi_brute_Mesh);
            //    animation_renderer->add_state_mapping(common::packet::OBJECT_STATE::ATTACK, "attack", hi_brute_Mesh);
            //    animation_renderer->set_state(common::packet::OBJECT_STATE::ATTACK);
            //    // 재질 및 쉐이더 설정
            //    material = "skinned_animation_SK_MagicConstruct";

            //    ResourceManager::instance()->create_material(material);
            //    ResourceManager::instance()->set_shader_for_material(material, "skinned");

            //    // 원하는 무기 붙이기
            //    auto socket_compnenet = hi_brute->get_component<SocketComponenet>();
            //    socket_compnenet->add_connecting("ik_hand_l_sword", "hand_l", "Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf", { 0.0623f, -0.8154f, 0.1643f }, { -10.f,90.f,-179.f }, { 2.f,2.f,2.f });

            //    // 스키닝 애니메이션 pso 설정              
            //    renderer->set_pso_name("skinned");

            //    // 위치, 회전 정보
            //    hi_brute->transform()->set_local_rotation(0.f, 0.f, 0.f);
            //    hi_brute->transform()->set_local_scale({ 1.0f, 1.0f, 1.0f });


            //    hi_brute->transform()->set_local_position(XMFLOAT3(0.0, 0.0f, 0.0f));
            //}



            //// 카메라 앞쪽으로 위치 조정 (필요에 따라 수정)
            //m_targetCharacter->transform()->set_local_position(XMFLOAT3(0.0f, 0.0f, 0.0f));
        }
    }

    // 현재 로드된 파일 이름 출력
    ImGui::Text("Current File: %s", m_loadedCharacterPath.c_str());

    ImGui::End();
}

void Tool_Scene::SpawnCamera()
{
    auto cameraObject = ObjectManager::instance()->create_game_object("ToolCamera");
    cameraObject->add_component<ToolCameraScript>();
    cameraObject->set_layer("Camera");
    cameraObject->transform()->set_local_position(XMFLOAT3(0.0f, 0.5f, -2.0f));
    cameraObject->transform()->set_local_rotation(0.0f, 0.0f, 0.0f);
}

std::string Tool_Scene::OpenFileDialog()
{
    OPENFILENAMEA ofn;
    CHAR szFile[260] = { 0 };

    ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = InputManager::instance()->GetHWnd(); // 게임 창을 부모로 설정
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "glTF / GLB Files\0*.gltf;*.glb\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    // OFN_NOCHANGEDIR: 파일 탐색기가 현재 작업 폴더(경로)를 바꾸지 못하게 막음 (매우 중요!)
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE)
    {
        return std::string(ofn.lpstrFile);
    }
    return "";
}