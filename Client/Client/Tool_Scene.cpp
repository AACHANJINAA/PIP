#include "stdafx.h"
#include "Tool_Scene.h"
#include "ObjectManager.h"
#include "InputManager.h"
#include "SceneManager.h"
#include "ToolCameraScript.h"
#include "ImGuiManager.h"
#include "ResourceManager.h"

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

            // 3. 렌더 컴포넌트 부착 및 메쉬 로드 (애니메이션 메쉬로 가정)
            auto renderComp = m_targetCharacter->add_component<RenderComponent>();
            renderComp->set_pso_name("skinned"); // 애니메이션 셰이더 이름에 맞게 수정

            // ResourceManager를 통해 메쉬 로드 
            // (주의: 파일 탐색기로 얻은 절대 경로를 바로 넘깁니다)
            auto mesh = ResourceManager::instance()->load_mesh(filePath, true, "null_name");
            renderComp->set_mesh(mesh);

            // 카메라 앞쪽으로 위치 조정 (필요에 따라 수정)
            m_targetCharacter->transform()->set_local_position(XMFLOAT3(0.0f, 0.0f, 0.0f));
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
    cameraObject->transform()->set_local_position(XMFLOAT3(0.0f, 10.0f, -10.0f));
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