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
#include "GameFramework.h"

void Tool_Scene::build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    SceneManager::instance()->build_skybox(device, commandList,
        "Resource/SkyBox/",
        "farmland/farmland_skybox.dds",
        "farmland/farmland_specular.dds",
        "farmland/farmland_diffuse.txt",
        "BRDF.dds");

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
            GameFramework::instance()->WaitForGpuComplete();

            auto device = GameFramework::instance()->device().Get();
            auto cmdQueue = GameFramework::instance()->command_queue().Get();
            auto cmdAlloc = GameFramework::instance()->command_allocator().Get();
            auto cmdList = GameFramework::instance()->command_list().Get();

            cmdAlloc->Reset();
            cmdList->Reset(cmdAlloc, nullptr);
            // 메시 로드
            auto mesh = ResourceManager::instance()->load_mesh(filePath, true);

            // 3. 렌더 컴포넌트 부착 및 메쉬 로드 (애니메이션 메쉬로 가정)
            _targetCharacter->add_glTF_conponent_pack(); // 이 함수가 애니메이션과 소켓 컴포넌트 추가함

            // 렌더러에 메시 등록
            auto renderer = _targetCharacter->get_component<RenderComponent>();
            renderer->set_mesh(mesh);

            // 애니메이션 컴포넌트 기본설정(T_POSE)
            auto animation_renderer = _targetCharacter->get_component<AnimationComponent>();
            animation_renderer->add_animation("t_pose", mesh);
            animation_renderer->play("t_pose");

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
            cmdList->Close();
            ID3D12CommandList* ppCommandLists[] = { cmdList };
            cmdQueue->ExecuteCommandLists(1, ppCommandLists);
            GameFramework::instance()->WaitForGpuComplete();
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
    bool bBoneChanged = ImGui::Combo(
        "Bones", 
        &_selectedBoneIndex, 
        combo_items.data(), 
        static_cast<int>(combo_items.size()));

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

    if (_weaponMesh)
    {
        ImGui::Text("Current Weapon: %s", _loadedWeaponPath.c_str());
		ImGui::Spacing(); ImGui::Spacing();
    }

    ImGui::Text("3. Attach Weapon");
    ImGui::Separator();

    if (ImGui::Button("Load Weapon Mesh"))
    {
        std::string weaponPath = open_file_dialog();
        if (_weaponMesh)
        {
            // 기존에 붙어있던 무기가 있다면 소켓에서 제거
            auto socketComp = _targetCharacter->get_component<SocketComponenet>();
            if (socketComp)
            {
                socketComp->delete_connecting("ToolSocket");
            }
            // 메모리 해제
			_weaponMesh.reset();
        }
        GameFramework::instance()->WaitForGpuComplete();

        auto device = GameFramework::instance()->device().Get();
        auto cmdQueue = GameFramework::instance()->command_queue().Get();
        auto cmdAlloc = GameFramework::instance()->command_allocator().Get();
        auto cmdList = GameFramework::instance()->command_list().Get();

        cmdAlloc->Reset();
        cmdList->Reset(cmdAlloc, nullptr);

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

        cmdList->Close();
        ID3D12CommandList* ppCommandLists[] = { cmdList };
        cmdQueue->ExecuteCommandLists(1, ppCommandLists);
        GameFramework::instance()->WaitForGpuComplete();
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
    XMMATRIX worldMat = XMLoadFloat4x4(&_targetCharacter->transform()->world_matrix());

    RECT rect;
    GetClientRect(InputManager::instance()->GetHWnd(), &rect);
    float width = static_cast<float>(rect.right - rect.left);
    float height = static_cast<float>(rect.bottom - rect.top);

    POINT mousePos = InputManager::instance()->GetMousePos();
    bool isMouseClicked = InputManager::instance()->IsKeyDown(VK_LBUTTON);

    float closestDistance = 20.0f;
    int bestBoneIndex = -1;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // =========================================================================
    // [추가됨] 언리얼 스타일 팔면체(Octahedron) 뼈대 그리기
    // =========================================================================
    for (int i = 0; i < _boneNames.size(); ++i)
    {
        std::string childName = _boneNames[i];
        std::string parentName = gltfMesh->get_parent_bone_name(childName);

        // 부모가 없으면(루트) 그릴 선이 없으므로 패스
        if (parentName.empty()) continue;

        // 부모와 자식의 월드 위치 계산
        XMFLOAT4X4 pLocal = gltfMesh->get_socket_transform(parentName);
        XMFLOAT4X4 cLocal = gltfMesh->get_socket_transform(childName);

        XMMATRIX pMat = XMLoadFloat4x4(&pLocal) * worldMat;
        XMMATRIX cMat = XMLoadFloat4x4(&cLocal) * worldMat;

        XMVECTOR pPos = pMat.r[3];
        XMVECTOR cPos = cMat.r[3];

        // 뼈대 방향과 길이 계산
        XMVECTOR dirVec = XMVectorSubtract(cPos, pPos);
        XMVECTOR lengthVec = XMVector3Length(dirVec);
        float length = XMVectorGetX(lengthVec);

        if (length < 0.001f) continue; // 너무 짧은 뼈는 무시

        // 방향 벡터 정규화 및 기저 벡터(Right, Up) 생성
        XMVECTOR forward = XMVectorDivide(dirVec, lengthVec);
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        if (abs(XMVectorGetY(forward)) > 0.99f) up = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f); // 수직 예외 처리
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
        up = XMVector3Cross(forward, right);

        // 뼈대 두께 및 팔면체의 가장 넓은 부분 위치 설정
        float thickness = std::max(0.01f, length * 0.15f); // 길이의 15%를 두께로
        float baseOffset = length * 0.2f;                  // 부모로부터 20% 지점이 가장 넓음

        // 팔면체의 6개 정점 3D 로컬 좌표 계산
        XMVECTOR V[6];
        V[0] = pPos;                                                           // 부모 위치 (시작점)
        V[1] = cPos;                                                           // 자식 위치 (끝점)
        V[2] = pPos + forward * baseOffset + right * thickness;                // 우측
        V[3] = pPos + forward * baseOffset - right * thickness;                // 좌측
        V[4] = pPos + forward * baseOffset + up * thickness;                   // 상단
        V[5] = pPos + forward * baseOffset - up * thickness;                   // 하단

        // 3D 정점을 2D 화면 좌표로 투영 (Project)
        ImVec2 screenPts[6];
        bool outOfScreen = false;
        for (int v = 0; v < 6; ++v)
        {
            XMVECTOR s = XMVector3Project(V[v], 0, 0, width, height, 0.0f, 1.0f, projMat, viewMat, XMMatrixIdentity());
            if (XMVectorGetZ(s) < 0.0f || XMVectorGetZ(s) > 1.0f) outOfScreen = true; // 카메라 뒤에 있으면 그리지 않음
            screenPts[v] = ImVec2(XMVectorGetX(s), XMVectorGetY(s));
        }

        if (outOfScreen) continue;

        // 팔면체를 구성하는 8개의 삼각형 인덱스 배열
        int faces[8][3] = {
            {0, 2, 4}, {0, 4, 3}, {0, 3, 5}, {0, 5, 2}, // 부모 쪽 피라미드
            {1, 4, 2}, {1, 3, 4}, {1, 5, 3}, {1, 2, 5}  // 자식 쪽 피라미드
        };

        // 색상 설정 (선택된 뼈의 부모/자식이면 붉은색, 아니면 회백색)
        bool isRelatedToSelected = (i == _selectedBoneIndex || gltfMesh->get_bone_index_by_name(parentName) == _selectedBoneIndex);
        ImU32 fillColor = isRelatedToSelected ? IM_COL32(200, 50, 50, 180) : IM_COL32(180, 180, 180, 100);
        ImU32 edgeColor = isRelatedToSelected ? IM_COL32(255, 100, 100, 255) : IM_COL32(50, 50, 50, 200);

        // ImGui DrawList로 2D 삼각형 그리기
        for (int f = 0; f < 8; ++f)
        {
            drawList->AddTriangleFilled(screenPts[faces[f][0]], screenPts[faces[f][1]], screenPts[faces[f][2]], fillColor);
            drawList->AddTriangle(screenPts[faces[f][0]], screenPts[faces[f][1]], screenPts[faces[f][2]], edgeColor, 1.0f);
        }
    }
    // =========================================================================

    // 기존의 노란색/빨간색 관절 점 그리기 및 피킹 로직 (기존 코드 그대로 유지)
    for (int i = 0; i < _boneNames.size(); ++i)
    {
        XMFLOAT4X4 boneLocal = gltfMesh->get_socket_transform(_boneNames[i]);
        XMMATRIX boneMat = XMLoadFloat4x4(&boneLocal);
        XMMATRIX finalBoneMat = boneMat * worldMat;
        XMVECTOR boneWorldPos = finalBoneMat.r[3];

        XMVECTOR screenPosVec = XMVector3Project(boneWorldPos, 0, 0, width, height, 0.0f, 1.0f, projMat, viewMat, worldMat);

        if (XMVectorGetZ(screenPosVec) < 0.0f || XMVectorGetZ(screenPosVec) > 1.0f) continue;

        float screenX = XMVectorGetX(screenPosVec);
        float screenY = XMVectorGetY(screenPosVec);

        float distToMouse = sqrtf(powf(screenX - mousePos.x, 2) + powf(screenY - mousePos.y, 2));
        if (distToMouse < closestDistance)
        {
            closestDistance = distToMouse;
            bestBoneIndex = i;
        }

        ImU32 color = (i == _selectedBoneIndex) ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 0, 200);
        float radius = (i == _selectedBoneIndex) ? 6.0f : 3.0f;
        drawList->AddCircleFilled(ImVec2(screenX, screenY), radius, color);
    }

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
    // [해결 1] 피킹 오프셋 해결: ImGui 메인 뷰포트 영역 사용
    // 윈도우 창의 테두리나 타이틀 바 등에 의한 미세한 오차를 없애기 위해,
    // ImGui가 인식하는 정확한 3D 작업 영역을 가져와 기즈모에 맵핑합니다.
    // -----------------------------------------------------------
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuizmo::BeginFrame();

    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(viewport->WorkPos.x, viewport->WorkPos.y, viewport->WorkSize.x, viewport->WorkSize.y);
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

    // -----------------------------------------------------------
    // [해결 2] 회전 축 꼬임 해결: ImGuizmo 전용 Recompose 함수 사용
    // DirectX의 회전 조립 방식(Pitch, Yaw, Roll)과 ImGuizmo의 분해 방식이 달라
    // 축이 꼬이는 현상을 막기 위해, 조립할 때도 ImGuizmo의 수학을 사용합니다.
    // -----------------------------------------------------------
    float fPos[3] = { _socketPos.x, _socketPos.y, _socketPos.z };
    float fRot[3] = { _socketRot.x, _socketRot.y, _socketRot.z };
    float fScale[3] = { _socketScale.x, _socketScale.y, _socketScale.z };

    XMFLOAT4X4 weaponLocalF;
    ImGuizmo::RecomposeMatrixFromComponents(fPos, fRot, fScale, (float*)&weaponLocalF);
    XMMATRIX weaponLocal = XMLoadFloat4x4(&weaponLocalF);

    // 무기 최종 월드 행렬 (결과를 XMFLOAT4X4에 담음)
    XMMATRIX weaponWorld = weaponLocal * parentWorld;
    XMFLOAT4X4 modelF;
    XMStoreFloat4x4(&modelF, weaponWorld);

    // -----------------------------------------------------------
    // [조작 및 역산] ImGuizmo::LOCAL 모드 유지
    // 객체의 로컬 축을 기준으로 기즈모가 함께 회전하도록 LOCAL 모드를 씁니다.
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
    const char* modeStr = (_currentGizmoOperation == ImGuizmo::TRANSLATE) ? "TRANSLATE (LOCAL)" :
        (_currentGizmoOperation == ImGuizmo::ROTATE) ? "ROTATE (LOCAL)" : "SCALE (LOCAL)";
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