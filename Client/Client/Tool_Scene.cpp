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
    spawn_camera();
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
    spawn_want_mesh();
    view_bones();
    spawn_want_socket_mesh();
    edit_socket_mesh();
    ImGui::End();

    draw_and_pick_bones();
    draw_gizmo();
}

void Tool_Scene::spawn_want_mesh()
{
    ImGui::Begin("Socket Weapon Editor");

    ImGui::Text("1. Character Setup");
    ImGui::Separator();

    // 파일 로드 버튼
    if (ImGui::Button("Load Character (glTF)"))
    {
        std::string filePath = open_file_dialog();
        if (!filePath.empty())
        {
            _loadedCharacterPath = filePath;

            // 1. 기존에 캐릭터가 있다면 삭제 (메모리 누수 방지)
            if (_targetCharacter)
            {
                _targetCharacter->destroy();
                _targetCharacter.reset();
            }

            // 2. 새 게임 오브젝트 생성
            _targetCharacter = ObjectManager::instance()->create_game_object("Editor_Character");

            // 메시 로드
            auto mesh = ResourceManager::instance()->load_mesh(filePath, true);

            // 3. 렌더 컴포넌트 부착 및 메쉬 로드 (애니메이션 메쉬로 가정)
            _targetCharacter->add_glTF_conponent_pack(); // 이 함수가 애니메이션과 소켓 컴포넌트 추가함

            // 렌더러에 메시 등록
            auto renderer = _targetCharacter->get_component<RenderComponent>();
            renderer->set_mesh(mesh);

            // 애니메이션 컴포넌트 기본설정(T_POSE)
            auto animation_renderer = _targetCharacter->get_component<AnimationComponent>();
            animation_renderer->add_state_mapping(common::packet::OBJECT_STATE::T_POSE, "t_pose", mesh);
            animation_renderer->set_state(common::packet::OBJECT_STATE::T_POSE);

            // 재질설정
            std::string material = "glTF_Test_material";

            ResourceManager::instance()->create_material(material);
            ResourceManager::instance()->set_shader_for_material(material, "skinned");

            // 스키닝 애니메이션 pso 설정     
            renderer->set_pso_name("skinned");

            // 위치, 회전 정보
            _targetCharacter->transform()->set_local_rotation(0.f, 0.f, 0.f);
            _targetCharacter->transform()->set_local_scale({ 1.0f, 1.0f, 1.0f });
            _targetCharacter->transform()->set_local_position(XMFLOAT3(0.0, 0.0f, 0.0f));

            // 뼈대 가져오기
            auto gltfMesh = std::dynamic_pointer_cast<ReadGLTFMesh>(mesh);
            if (gltfMesh)
            {
                _boneNames = gltfMesh->get_bone_names();
                _selectedBoneIndex = 0;
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
            //_targetCharacter->transform()->set_local_position(XMFLOAT3(0.0f, 0.0f, 0.0f));
        }
    }

    // 현재 로드된 파일 이름 출력
    ImGui::Text("Current File: %s", _loadedCharacterPath.c_str());
    ImGui::Spacing(); ImGui::Spacing();
}

void Tool_Scene::view_bones()
{
    if (_boneNames.empty() || !_targetCharacter) return;

    ImGui::Text("2. Select Bone");
    ImGui::Separator();

    std::vector<const char*> combo_items;
    for (const auto& name : _boneNames) combo_items.push_back(name.c_str());

    // 콤보박스 값이 변경되었는지 확인
    bool bBoneChanged = ImGui::Combo("Bones", &_selectedBoneIndex, combo_items.data(), combo_items.size());

    // 뼈대 보기 체크박스
    ImGui::Checkbox("Show Debug Bones", &_bShowBones);

    ImGui::Spacing(); ImGui::Spacing();

    // 뼈대 선택이 바뀌었고, 현재 무기가 붙어있다면 즉시 위치를 갱신
    if (bBoneChanged && _weaponMesh)
    {
        _socketPos = { 0.0f, 0.0f, 0.0f };
        _socketRot = { 0.0f, 0.0f, 0.0f };
        _socketScale = { 1.0f, 1.0f, 1.0f };

        auto socketComp = _targetCharacter->get_component<SocketComponenet>();
        if (socketComp)
        {
            socketComp->fix_connecting("ToolSocket", _boneNames[_selectedBoneIndex], _weaponMesh, _socketPos, _socketRot, _socketScale);
        }
    }
}
void Tool_Scene::spawn_want_socket_mesh()
{
    if (_boneNames.empty() || !_targetCharacter) return;

    ImGui::Text("3. Attach Weapon");
    ImGui::Separator();

    if (ImGui::Button("Load Weapon Mesh"))
    {
        std::string weaponPath = open_file_dialog();
        if (!weaponPath.empty())
        {
            _loadedWeaponPath = weaponPath;
            _weaponMesh = ResourceManager::instance()->load_mesh(weaponPath);

            // 새 무기를 로드했으니 수치 초기화
            _socketPos = { 0.0f, 0.0f, 0.0f };
            _socketRot = { 0.0f, 0.0f, 0.0f };
            _socketScale = { 1.0f, 1.0f, 1.0f };

            auto socketComp = _targetCharacter->get_component<SocketComponenet>();
            if (socketComp && _weaponMesh)
            {
                socketComp->add_connecting("ToolSocket", _boneNames[_selectedBoneIndex], _loadedWeaponPath, _socketPos, _socketRot, _socketScale);
            }
        }
    }
    ImGui::Text("Weapon: %s", _loadedWeaponPath.c_str());
    ImGui::Spacing(); ImGui::Spacing();
}

void Tool_Scene::edit_socket_mesh()
{
    if (!_weaponMesh || !_targetCharacter) return;

    ImGui::Text("4. Adjust Socket Transform");
    ImGui::Separator();

    bool bChanged = false;

    if (ImGui::DragFloat3("Position", &_socketPos.x, 0.01f)) bChanged = true;
    if (ImGui::DragFloat3("Rotation", &_socketRot.x, 1.0f)) bChanged = true;
    if (ImGui::DragFloat3("Scale", &_socketScale.x, 0.01f)) bChanged = true;

    if (bChanged)
    {
        auto socketComp = _targetCharacter->get_component<SocketComponenet>();
        if (socketComp)
        {
            socketComp->fix_connecting("ToolSocket", _boneNames[_selectedBoneIndex], _weaponMesh, _socketPos, _socketRot, _socketScale);
        }
    }
}

void Tool_Scene::draw_and_pick_bones()
{
    if (!_targetCharacter || !_bShowBones || _boneNames.empty()) return;

    auto renderComp = _targetCharacter->get_component<RenderComponent>();
    if (!renderComp) return;

    auto gltfMesh = std::dynamic_pointer_cast<ReadGLTFMesh>(renderComp->mesh());
    if (!gltfMesh) return;

    auto mainCam = CameraComponent::get_main();
    if (!mainCam) return;

    // 1. 카메라와 화면 정보 가져오기
    XMMATRIX viewMat = XMLoadFloat4x4(&mainCam->view_matrix());
    XMMATRIX projMat = XMLoadFloat4x4(&mainCam->projection_matrix());
    XMMATRIX viewProj = viewMat * projMat;
    XMMATRIX worldMat = XMLoadFloat4x4(&_targetCharacter->transform()->world_matrix());

    RECT rect;
    GetClientRect(InputManager::instance()->GetHWnd(), &rect);
    float width = static_cast<float>(rect.right - rect.left);
    float height = static_cast<float>(rect.bottom - rect.top);

    POINT mousePos = InputManager::instance()->GetMousePos();
    bool isMouseClicked = InputManager::instance()->IsKeyDown(VK_LBUTTON); // 좌클릭 확인

    float closestDistance = 20.0f; // 클릭 인정 반경 (픽셀 단위)
    int bestBoneIndex = -1;

    // ImGui의 백그라운드 도화지를 가져와서 3D 공간 위에 2D 점을 그림
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // 2. 모든 뼈대를 순회하며 화면 좌표로 변환
    for (int i = 0; i < _boneNames.size(); ++i)
    {
        // 뼈대의 로컬 행렬 가져오기
        XMFLOAT4X4 boneLocal = gltfMesh->get_socket_transform(_boneNames[i]);
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
        ImU32 color = (i == _selectedBoneIndex) ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 0, 200);
        float radius = (i == _selectedBoneIndex) ? 6.0f : 3.0f;
        drawList->AddCircleFilled(ImVec2(screenX, screenY), radius, color);
    }

    // 5. 클릭 처리가 발생했고, 새로운 뼈대가 선택되었다면?
    // (단, ImGui UI 창을 클릭한 게 아닐 때만 3D 클릭으로 인정)
    if (isMouseClicked && bestBoneIndex != -1 && bestBoneIndex != _selectedBoneIndex && !ImGui::GetIO().WantCaptureMouse)
    {
        _selectedBoneIndex = bestBoneIndex;

        if (_weaponMesh)
        {
            _socketPos = { 0.0f, 0.0f, 0.0f };
            _socketRot = { 0.0f, 0.0f, 0.0f };
            _socketScale = { 1.0f, 1.0f, 1.0f };

            auto socketComp = _targetCharacter->get_component<SocketComponenet>();
            if (socketComp)
            {
                socketComp->fix_connecting("ToolSocket", _boneNames[_selectedBoneIndex], _weaponMesh, _socketPos, _socketRot, _socketScale);
            }
        }
    }
}

void Tool_Scene::draw_gizmo()
{
    // 1. 기본 체크: 데이터가 없으면 실행 안 함
    if (!_targetCharacter || !_weaponMesh || _boneNames.empty()) return;

    auto mainCam = CameraComponent::get_main();
    if (!mainCam) return;

    // -----------------------------------------------------------
    // [UI 및 렌더링 영역 설정]
    // -----------------------------------------------------------
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::BeginFrame();

    // 화면 전체를 덮는 투명한 도화지 사용 (ImGui 창 영역에 잘리지 않음)
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::SetOrthographic(false);

    // 'Y' 키 모드 전환
    static bool yPressed = false;
    if (InputManager::instance()->IsKeyDown('Y')) {
        if (!yPressed) {
            if (_currentGizmoOperation == ImGuizmo::TRANSLATE) _currentGizmoOperation = ImGuizmo::ROTATE;
            else if (_currentGizmoOperation == ImGuizmo::ROTATE) _currentGizmoOperation = ImGuizmo::SCALE;
            else _currentGizmoOperation = ImGuizmo::TRANSLATE;
            yPressed = true;
        }
    }
    else { yPressed = false; }

    // -----------------------------------------------------------
    // [행렬 준비] Transpose 없이 XMFLOAT4X4 메모리 구조를 그대로 사용
    // -----------------------------------------------------------
    XMFLOAT4X4 viewF = mainCam->view_matrix();
    XMFLOAT4X4 projF = mainCam->projection_matrix();

    // 부모 정보 계산 (뼈대 로컬 * 캐릭터 월드)
    auto renderComp = _targetCharacter->get_component<RenderComponent>();
    auto gltfMesh = std::dynamic_pointer_cast<ReadGLTFMesh>(renderComp->mesh());

    XMFLOAT4X4 charWorldF = _targetCharacter->transform()->world_matrix();
    XMMATRIX charWorld = XMLoadFloat4x4(&charWorldF);

    XMFLOAT4X4 boneLocalF = gltfMesh->get_socket_transform(_boneNames[_selectedBoneIndex]);
    XMMATRIX boneLocal = XMLoadFloat4x4(&boneLocalF);

    XMMATRIX parentWorld = boneLocal * charWorld;

    // 무기 현재 로컬 행렬 계산
    XMMATRIX weaponLocal = XMMatrixScaling(_socketScale.x, _socketScale.y, _socketScale.z) *
        XMMatrixRotationRollPitchYaw(XMConvertToRadians(_socketRot.x),
            XMConvertToRadians(_socketRot.y),
            XMConvertToRadians(_socketRot.z)) *
        XMMatrixTranslation(_socketPos.x, _socketPos.y, _socketPos.z);

    // 무기 최종 월드 행렬 (결과를 XMFLOAT4X4에 담음)
    XMMATRIX weaponWorld = weaponLocal * parentWorld;
    XMFLOAT4X4 modelF;
    XMStoreFloat4x4(&modelF, weaponWorld);

    // -----------------------------------------------------------
    // [조작 및 역산] ImGuizmo 포인터 매핑 및 부모 영향력 제거
    // -----------------------------------------------------------
    if (ImGuizmo::Manipulate((float*)&viewF, (float*)&projF, _currentGizmoOperation, ImGuizmo::LOCAL, (float*)&modelF))
    {
        // 1. 조작된 행렬 읽어오기
        XMMATRIX newWeaponWorld = XMLoadFloat4x4(&modelF);

        // 2. 역산: 부모 행렬 제거 (NewLocal = NewWorld * Inverse(ParentWorld))
        XMMATRIX invParentWorld = XMMatrixInverse(nullptr, parentWorld);
        XMMATRIX newWeaponLocal = newWeaponWorld * invParentWorld;

        // 3. 분해를 위해 다시 XMFLOAT4X4에 저장
        XMFLOAT4X4 localF;
        XMStoreFloat4x4(&localF, newWeaponLocal);

        // 4. 추출 (ImGuizmo는 Degree 단위로 추출해 줌)
        float resPos[3], resRot[3], resScale[3];
        ImGuizmo::DecomposeMatrixToComponents((float*)&localF, resPos, resRot, resScale);

        _socketPos = { resPos[0], resPos[1], resPos[2] };
        _socketRot = { resRot[0], resRot[1], resRot[2] };
        _socketScale = { resScale[0], resScale[1], resScale[2] };

        auto socketComp = _targetCharacter->get_component<SocketComponenet>();
        if (socketComp) {
            socketComp->fix_connecting("ToolSocket", _boneNames[_selectedBoneIndex], _weaponMesh, _socketPos, _socketRot, _socketScale);
        }
    }

    // 상태 표시 UI
    const char* modeStr = (_currentGizmoOperation == ImGuizmo::TRANSLATE) ? "TRANSLATE" :
        (_currentGizmoOperation == ImGuizmo::ROTATE) ? "ROTATE" : "SCALE";
    ImGui::GetForegroundDrawList()->AddText(ImVec2(20, 20), IM_COL32(0, 255, 0, 255), modeStr);
}

void Tool_Scene::spawn_camera()
{
    auto cameraObject = ObjectManager::instance()->create_game_object("ToolCamera");
    cameraObject->add_component<ToolCameraScript>();
    cameraObject->set_layer("Camera");
    cameraObject->transform()->set_local_position(XMFLOAT3(0.0f, 0.5f, -2.0f));
    cameraObject->transform()->set_local_rotation(0.0f, 0.0f, 0.0f);
}

std::string Tool_Scene::open_file_dialog()
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