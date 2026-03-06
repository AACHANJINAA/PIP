#include "stdafx.h"
#include "Tool_Scene.h"
#include "ObjectManager.h"
#include "InputManager.h"
#include "SceneManager.h"
#include "ToolCameraScript.h"

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
  
    // 원하는 메시 띄우기
    SpawnWantMesh();
    ViewBones();
    SpawnWantSocketMesh();
    EditSocketMesh();
    ImGui::End();

    DrawAndPickBones();
    DrawGizmo();
}

void Tool_Scene::SpawnWantMesh()
{
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

            // 뼈대 가져오기
            auto gltfMesh = std::dynamic_pointer_cast<ReadGLTFMesh>(mesh);
            if (gltfMesh)
            {
                m_boneNames = gltfMesh->get_bone_names();
                m_selectedBoneIndex = 0;
            }


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
    ImGui::Spacing(); ImGui::Spacing();
}

void Tool_Scene::ViewBones()
{
    if (m_boneNames.empty() || !m_targetCharacter) return;

    ImGui::Text("2. Select Bone");
    ImGui::Separator();

    std::vector<const char*> combo_items;
    for (const auto& name : m_boneNames) combo_items.push_back(name.c_str());

    // 콤보박스 값이 변경되었는지 확인
    bool bBoneChanged = ImGui::Combo("Bones", &m_selectedBoneIndex, combo_items.data(), combo_items.size());

    // 뼈대 보기 체크박스
    ImGui::Checkbox("Show Debug Bones", &m_bShowBones);

    ImGui::Spacing(); ImGui::Spacing();

    // 뼈대 선택이 바뀌었고, 현재 무기가 붙어있다면 즉시 위치를 갱신
    if (bBoneChanged && m_weaponMesh)
    {
        m_socketPos = { 0.0f, 0.0f, 0.0f };
        m_socketRot = { 0.0f, 0.0f, 0.0f };
        m_socketScale = { 1.0f, 1.0f, 1.0f };

        auto socketComp = m_targetCharacter->get_component<SocketComponenet>();
        if (socketComp)
        {
            socketComp->fix_connecting("ToolSocket", m_boneNames[m_selectedBoneIndex], m_weaponMesh, m_socketPos, m_socketRot, m_socketScale);
        }
    }
}
void Tool_Scene::SpawnWantSocketMesh()
{
    if (m_boneNames.empty() || !m_targetCharacter) return;

    ImGui::Text("3. Attach Weapon");
    ImGui::Separator();

    if (ImGui::Button("Load Weapon Mesh"))
    {
        std::string weaponPath = OpenFileDialog();
        if (!weaponPath.empty())
        {
            m_loadedWeaponPath = weaponPath;
            m_weaponMesh = ResourceManager::instance()->load_mesh(weaponPath);

            // 새 무기를 로드했으니 수치 초기화
            m_socketPos = { 0.0f, 0.0f, 0.0f };
            m_socketRot = { 0.0f, 0.0f, 0.0f };
            m_socketScale = { 1.0f, 1.0f, 1.0f };

            auto socketComp = m_targetCharacter->get_component<SocketComponenet>();
            if (socketComp && m_weaponMesh)
            {
                socketComp->add_connecting("ToolSocket", m_boneNames[m_selectedBoneIndex], m_loadedWeaponPath, m_socketPos, m_socketRot, m_socketScale);
            }
        }
    }
    ImGui::Text("Weapon: %s", m_loadedWeaponPath.c_str());
    ImGui::Spacing(); ImGui::Spacing();
}

void Tool_Scene::EditSocketMesh()
{
    if (!m_weaponMesh || !m_targetCharacter) return;

    ImGui::Text("4. Adjust Socket Transform");
    ImGui::Separator();

    bool bChanged = false;

    if (ImGui::DragFloat3("Position", &m_socketPos.x, 0.01f)) bChanged = true;
    if (ImGui::DragFloat3("Rotation", &m_socketRot.x, 1.0f)) bChanged = true;
    if (ImGui::DragFloat3("Scale", &m_socketScale.x, 0.01f)) bChanged = true;

    if (bChanged)
    {
        auto socketComp = m_targetCharacter->get_component<SocketComponenet>();
        if (socketComp)
        {
            socketComp->fix_connecting("ToolSocket", m_boneNames[m_selectedBoneIndex], m_weaponMesh, m_socketPos, m_socketRot, m_socketScale);
        }
    }
}

void Tool_Scene::DrawAndPickBones()
{
    if (!m_targetCharacter || !m_bShowBones || m_boneNames.empty()) return;

    auto renderComp = m_targetCharacter->get_component<RenderComponent>();
    if (!renderComp) return;

    auto gltfMesh = std::dynamic_pointer_cast<ReadGLTFMesh>(renderComp->mesh());
    if (!gltfMesh) return;

    auto mainCam = CameraComponent::get_main();
    if (!mainCam) return;

    // 1. 카메라와 화면 정보 가져오기
    XMMATRIX viewMat = XMLoadFloat4x4(&mainCam->view_matrix());
    XMMATRIX projMat = XMLoadFloat4x4(&mainCam->projection_matrix());
    XMMATRIX viewProj = viewMat * projMat;
    XMMATRIX worldMat = XMLoadFloat4x4(&m_targetCharacter->transform()->world_matrix());

    RECT rect;
    GetClientRect(InputManager::instance()->GetHWnd(), &rect);
    float width = static_cast<float>(rect.right - rect.left);
    float height = static_cast<float>(rect.bottom - rect.top);

    POINT mousePos = InputManager::instance()->GetMousePos();
    bool isMouseClicked = InputManager::instance()->IsKeyDown(VK_LBUTTON); // 좌클릭 확인

    float closestDistance = 20.0f; // 클릭 인정 반경 (픽셀 단위)
    int bestBoneIndex = -1;

    // ImGui의 백그라운드 도화지를 가져와서 3D 공간 위에 2D 점을 그립니다.
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // 2. 모든 뼈대를 순회하며 화면 좌표로 변환
    for (int i = 0; i < m_boneNames.size(); ++i)
    {
        // 뼈대의 로컬 행렬 가져오기
        XMFLOAT4X4 boneLocal = gltfMesh->get_socket_transform(m_boneNames[i]);
        XMMATRIX boneMat = XMLoadFloat4x4(&boneLocal);

        // 뼈대의 최종 월드 위치 계산 (Bone Local * Character World)
        XMMATRIX finalBoneMat = boneMat * worldMat;
        XMVECTOR boneWorldPos = finalBoneMat.r[3]; // 행렬의 4번째 행이 Position

        // 월드 좌표를 2D 화면 좌표로 투영 (Projection)
        XMVECTOR screenPosVec = XMVector3Project(boneWorldPos, 0, 0, width, height, 0.0f, 1.0f, projMat, viewMat, worldMat);

        // Z값이 1.0보다 크거나 0보다 작으면 카메라 뒤에 있는 것이므로 무시
        if (XMVectorGetZ(screenPosVec) < 0.0f || XMVectorGetZ(screenPosVec) > 1.0f) continue;

        float screenX = XMVectorGetX(screenPosVec);
        float screenY = XMVectorGetY(screenPosVec);

        // 3. 마우스 피킹 (가장 가까운 뼈대 찾기)
        float distToMouse = sqrt(pow(screenX - mousePos.x, 2) + pow(screenY - mousePos.y, 2));
        if (distToMouse < closestDistance)
        {
            closestDistance = distToMouse;
            bestBoneIndex = i;
        }

        // 4. 화면에 뼈대 그리기 (선택된 뼈대는 빨간색 크게, 나머지는 노란색 작게)
        ImU32 color = (i == m_selectedBoneIndex) ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 0, 200);
        float radius = (i == m_selectedBoneIndex) ? 6.0f : 3.0f;
        drawList->AddCircleFilled(ImVec2(screenX, screenY), radius, color);
    }

    // 5. 클릭 처리가 발생했고, 새로운 뼈대가 선택되었다면?
    // (단, ImGui UI 창을 클릭한 게 아닐 때만 3D 클릭으로 인정)
    if (isMouseClicked && bestBoneIndex != -1 && bestBoneIndex != m_selectedBoneIndex && !ImGui::GetIO().WantCaptureMouse)
    {
        m_selectedBoneIndex = bestBoneIndex;

        if (m_weaponMesh)
        {
            m_socketPos = { 0.0f, 0.0f, 0.0f };
            m_socketRot = { 0.0f, 0.0f, 0.0f };
            m_socketScale = { 1.0f, 1.0f, 1.0f };

            auto socketComp = m_targetCharacter->get_component<SocketComponenet>();
            if (socketComp)
            {
                socketComp->fix_connecting("ToolSocket", m_boneNames[m_selectedBoneIndex], m_weaponMesh, m_socketPos, m_socketRot, m_socketScale);
            }
        }
    }
}

void Tool_Scene::DrawGizmo()
{
    // 1. 기본 체크: 데이터가 없으면 실행 안 함
    if (!m_targetCharacter || !m_weaponMesh || m_boneNames.empty()) return;

    auto mainCam = CameraComponent::get_main();
    if (!mainCam) return;

    // -----------------------------------------------------------
    // [설정] 기즈모 초기화 및 영역 설정
    // -----------------------------------------------------------
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    // [핵심] 기즈모가 창에 가려지지 않게 화면 맨 앞에 그리도록 강제 설정
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::BeginFrame();

    // 'Y' 키 모드 전환 (IsKeyDown은 매 프레임 호출되므로 한 번만 눌리게 하려면 엔진에 IsKeyPressed 기능이 필요하지만, 일단 현재 구조 유지)
    static bool yPressed = false;
    if (InputManager::instance()->IsKeyDown('Y')) {
        if (!yPressed) {
            if (m_currentGizmoOperation == ImGuizmo::TRANSLATE) m_currentGizmoOperation = ImGuizmo::ROTATE;
            else if (m_currentGizmoOperation == ImGuizmo::ROTATE) m_currentGizmoOperation = ImGuizmo::SCALE;
            else m_currentGizmoOperation = ImGuizmo::TRANSLATE;
            yPressed = true;
        }
    }
    else { yPressed = false; }

    // -----------------------------------------------------------
    // [행렬 준비] DX12(Row) -> ImGuizmo(Column) 변환
    // -----------------------------------------------------------
    // 카메라 행렬 가져오기 및 Transpose
    XMMATRIX viewMat = XMMatrixTranspose(XMLoadFloat4x4(&mainCam->view_matrix()));
    XMMATRIX projMat = XMMatrixTranspose(XMLoadFloat4x4(&mainCam->projection_matrix()));

    // 부모 정보 계산 (캐릭터 월드 * 뼈대 로컬)
    auto renderComp = m_targetCharacter->get_component<RenderComponent>();
    auto gltfMesh = std::dynamic_pointer_cast<ReadGLTFMesh>(renderComp->mesh());
    XMMATRIX charWorld = XMLoadFloat4x4(&m_targetCharacter->transform()->world_matrix());

	XMFLOAT4X4 boneLocalFloat = gltfMesh->get_socket_transform(m_boneNames[m_selectedBoneIndex]);
    XMMATRIX boneLocal = XMLoadFloat4x4(&boneLocalFloat);

    // ParentWorld = 뼈대 * 캐릭터 (SocketComponenet 연산 순서와 일치)
    XMMATRIX parentWorld = boneLocal * charWorld;

    // 현재 무기의 로컬 SRT를 행렬로 생성
    XMMATRIX weaponLocal = XMMatrixScaling(m_socketScale.x, m_socketScale.y, m_socketScale.z) *
        XMMatrixRotationRollPitchYaw(XMConvertToRadians(m_socketRot.x),
            XMConvertToRadians(m_socketRot.y),
            XMConvertToRadians(m_socketRot.z)) *
        XMMatrixTranslation(m_socketPos.x, m_socketPos.y, m_socketPos.z);

    // 기즈모에게 넘겨줄 "최종 월드 행렬"을 만들고 Column-Major로 변환
    XMMATRIX weaponWorld = weaponLocal * parentWorld;
    float modelMatrix[16];
    XMStoreFloat4x4((XMFLOAT4X4*)modelMatrix, XMMatrixTranspose(weaponWorld));

    // -----------------------------------------------------------
    // [조작] 기즈모 그리기 및 결과 역산
    // -----------------------------------------------------------
    if (ImGuizmo::Manipulate((float*)&viewMat, (float*)&projMat, m_currentGizmoOperation, ImGuizmo::WORLD, modelMatrix))
    {
        // 1. 조작된 월드 행렬을 다시 DX용(Row-Major)으로 복구
        XMMATRIX newWeaponWorld = XMMatrixTranspose(XMLoadFloat4x4((XMFLOAT4X4*)modelMatrix));

        // 2. [수학 지옥 탈출] NewLocal = NewWorld * Inverse(ParentWorld)
        // 이 역산 과정에서 부모의 스케일과 회전 영향력이 완벽하게 제거됩니다.
        XMMATRIX invParentWorld = XMMatrixInverse(nullptr, parentWorld);
        XMMATRIX newWeaponLocal = newWeaponWorld * invParentWorld;

        // 3. ImGuizmo 함수를 이용해 SRT 추출 (Column-Major 행렬 전달 필요)
        float resPos[3], resRot[3], resScale[3];
        XMMATRIX transposeForDecompose = XMMatrixTranspose(newWeaponLocal);
        ImGuizmo::DecomposeMatrixToComponents((float*)&transposeForDecompose, resPos, resRot, resScale);

        // 4. 추출된 데이터를 우리 변수에 대입 (ImGuizmo는 Degree로 뱉음)
        m_socketPos = { resPos[0], resPos[1], resPos[2] };
        m_socketRot = { resRot[0], resRot[1], resRot[2] };
        m_socketScale = { resScale[0], resScale[1], resScale[2] };

        // 5. 소켓 컴포넌트에 즉시 반영
        auto socketComp = m_targetCharacter->get_component<SocketComponenet>();
        if (socketComp) {
            socketComp->fix_connecting("ToolSocket", m_boneNames[m_selectedBoneIndex], m_weaponMesh, m_socketPos, m_socketRot, m_socketScale);
        }
    }

    // 상태 표시 텍스트
    const char* modeStr = (m_currentGizmoOperation == ImGuizmo::TRANSLATE) ? "TRANSLATE" :
        (m_currentGizmoOperation == ImGuizmo::ROTATE) ? "ROTATE" : "SCALE";
    ImGui::GetForegroundDrawList()->AddText(ImVec2(20, 20), IM_COL32(0, 255, 0, 255), modeStr);
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