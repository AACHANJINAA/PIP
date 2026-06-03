#include "stdafx.h"
#include "gameobject.h"
#include "QuestNPCScript.h"
#include "ObjectManager.h"
#include "ResourceManager.h"
#include "RenderComponent.h"
#include "AnimationComponent.h"
#include "ReadGLTFMesh.h"
#include "UIManager.h"
#include "UIRenderComponent.h"
#include "CameraComponent.h"
#include "GameFramework.h"
#include "InputManager.h"
#include "NetworkManager.h"
#include "BillboardUIRenderComponent.h"

void QuestNPCScript::init_visual()
{
    auto renderer = game_object()->get_component<RenderComponent>();
    auto animation_component = game_object()->get_component<AnimationComponent>();

    if (!animation_component)
    {
        CERROR("애니메이션 컴포넌트 추가 안됨 확인!");
    }

    // 메쉬 및 애니메이션 로드
    auto T1_Mesh = ResourceManager::instance()->load_mesh("Resource/Character/Bandit_Rd_NPC/Bandit_Rd_NPC.gltf",true);
    auto mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh);
    if (mesh)
    {
        mesh->load_animation_only("Resource/Character/Bandit_Rd_NPC/Animations/A_Hu_F_Idle.gltf", "idle");
    }

    renderer->set_mesh(T1_Mesh);

    if (animation_component)
    {
        animation_component->add_animation("idle", T1_Mesh, "idle");
        animation_component->play("idle");
    }

    // 재질 및 쉐이더 설정
    std::string material = "NPC_Material";
    ResourceManager::instance()->create_material(material);
    ResourceManager::instance()->set_shader_for_material(material, "skinned");

    // skinned
    renderer->set_pso_name("skinned");

    // 위치, 회전, 스케일 설정
    auto trans = transform();
    if (trans)
    {
        trans->set_local_rotation(0.f, 0.f, 0.f);
        trans->set_local_scale({ 1.f, 1.f, 1.f });
    }

	// UI 초기화
    _uiRenderer = UIManager::instance()->ui_component(UILayer::MIDDLE, "F_interaction_UI");
    _uiRenderer->set_size(_uiWidth, _uiHeight);
    UIManager::instance()->set_visible(UILayer::MIDDLE, "F_interaction_UI", false);


	// 퀘스트 마커 UI 초기화
    _markerObject = ObjectManager::instance()->create_game_object(game_object()->name() + "_QuestMarker");
    _markerObject->transform()->set_local_position(transform()->get_world_position());

    _markerRenderer = _markerObject->add_component<BillboardUIRenderComponent>();
	_markerRenderer->set_size(0.8f, 0.8f); // 3D 공간에서의 크기 (2x2)

	// y축 보정 (NPC 중앙에 UI가 뜨도록)
	_uiYOffset = transform()->get_world_scale().y * 1.5f;
    _markerYOffset = transform()->get_world_scale().y * 2.7f; // 마커는 NPC 머리보다 좀 더 위로
    _markerRenderer->set_y_offset(_markerYOffset);
}

void QuestNPCScript::update(float deltaTime)
{
    //NPCScript::update(deltaTime);

    // 상호작용 F키 UI 업데이트
    update_F_interaction_UI(deltaTime);
    update_QuestMarker_UI(deltaTime);

    if (/*_currentAlpha > 0.0f && */InputManager::instance()->IsKeyDown('F'))
    {
        // 나중에 서버와 연결 부
        // 서버에 NPC 상호작용 (서버로부터 부여받은 id() 사용)
        NetworkManager::instance()->SendNPCInteractPacket(id(), 0);
        // 인터랙션 시 대화 중 상태로 전환 (UI 숨기기를 위해)

        // 대화 상태 전환 테스트
        _isTalking = !_isTalking;
    }
}

void QuestNPCScript::update_F_interaction_UI(float deltaTime)
{
    auto mainPlayer = ObjectManager::instance()->find_object("MainPlayer");
    if (!mainPlayer || ! _uiRenderer) return;

    auto camera = CameraComponent::get_main();
    if (!camera) return;

    auto playerPos = mainPlayer->transform()->get_world_position();
    auto npcPos = transform()->get_world_position();
    
    DirectX::XMVECTOR vPos = DirectX::XMLoadFloat3(&playerPos);
    DirectX::XMVECTOR vNpc = DirectX::XMLoadFloat3(&npcPos);
    DirectX::XMVECTOR distVec = DirectX::XMVector3Length(DirectX::XMVectorSubtract(vPos, vNpc));
    float distance = DirectX::XMVectorGetX(distVec);

	bool isClose = (distance <= _interactionDistance); // 가까운지? 가까우면 true, 멀면 false

    if (isClose)
    {
        _currentAlpha += _fadeSpeed * deltaTime;
        if (_currentAlpha > 1.0f) _currentAlpha = 1.0f;
        UIManager::instance()->set_visible(UILayer::MIDDLE, "F_interaction_UI", true);
    }
    else
    {
        _currentAlpha -= _fadeSpeed * deltaTime;
        if (_currentAlpha < 0.0f) {
            _currentAlpha = 0.0f;
            UIManager::instance()->set_visible(UILayer::MIDDLE, "F_interaction_UI", false);
            return; 
        }
    }

    if (_isTalking)
    {
		// 대화 중에는 상호작용 F키 UI를 항상 보이지 않도록 설정
        UIManager::instance()->set_visible(UILayer::MIDDLE, "F_interaction_UI", false);
    }
    else
    {
        UIManager::instance()->set_visible(UILayer::MIDDLE, "F_interaction_UI", true);
    }

    if (UIManager::instance()->is_visible(UILayer::MIDDLE, "F_interaction_UI"))
    {
        auto projMat = DirectX::XMLoadFloat4x4(&camera->projection_matrix());
        auto viewMat = DirectX::XMLoadFloat4x4(&camera->view_matrix());
        auto worldMat = DirectX::XMMatrixIdentity();

        float screenWidth = static_cast<float>(GameFramework::instance()->get_window_width());
        float screenHeight = static_cast<float>(GameFramework::instance()->get_window_height());

        DirectX::XMFLOAT3 uiWorldPos = npcPos;
		// UI가 NPC 중앙에 뜨도록 y축으로 약간 올려줌 (필요시 조절)
        uiWorldPos.y += _uiYOffset;

        DirectX::XMVECTOR uiWorldPosVec = DirectX::XMLoadFloat3(&uiWorldPos);
        DirectX::XMVECTOR uiScreenPosVec = DirectX::XMVector3Project(uiWorldPosVec, 0, 0, screenWidth, screenHeight, 0.0f, 1.0f, projMat, viewMat, worldMat);

        float screenX = DirectX::XMVectorGetX(uiScreenPosVec);
        float screenY = DirectX::XMVectorGetY(uiScreenPosVec);
        float screenZ = DirectX::XMVectorGetZ(uiScreenPosVec);

        if (screenZ < 0.0f || screenZ > 1.0f)
        {
            _uiRenderer->set_color(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f));
        }
        else
        {
            _uiRenderer->set_screen_position(screenX - (_uiWidth / 2.0f), screenY - (_uiHeight / 2.0f));
            _uiRenderer->set_color(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, _currentAlpha));
        }
    }
}

void QuestNPCScript::update_QuestMarker_UI(float deltaTime)
{
    if (!_markerRenderer) return;
	auto mainPlayer = ObjectManager::instance()->find_object("MainPlayer");
    auto camera = CameraComponent::get_main();
    if (!camera || !mainPlayer) return;

    auto playerPos = mainPlayer->transform()->get_world_position();
    auto npcPos = transform()->get_world_position();
    
    DirectX::XMVECTOR vPos = DirectX::XMLoadFloat3(&playerPos);
    DirectX::XMVECTOR vNpc = DirectX::XMLoadFloat3(&npcPos);
    DirectX::XMVECTOR distVec = DirectX::XMVector3Length(DirectX::XMVectorSubtract(vPos, vNpc));
    float distance = DirectX::XMVectorGetX(distVec);

    // 퀘스트 상태 확인
    const auto* quest_info = NetworkManager::instance()->get_quest(1); // 1번 퀘스트 고정
    common::packet::QuestState state = common::packet::QuestState::NONE;
    if (quest_info) 
    {
        state = quest_info->_state;
    }

    // 퀘스트 상태에 따른 마커 텍스처와 색상 변경
    if (state == common::packet::QuestState::NONE) {
        _markerRenderer->set_texture("Resource/UI/Quest_Exclamation_UI.png");
        _markerRenderer->set_color(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f)); // 흰색 느낌표
    }
    else if (state == common::packet::QuestState::IN_PROGRESS) {
        _markerRenderer->set_texture("Resource/UI/Quest_Question_UI.png"); // 진행 중
        _markerRenderer->set_color(DirectX::XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f)); // 회색 물음표
    }
    else if (state == common::packet::QuestState::COMPLETED) {
        _markerRenderer->set_texture("Resource/UI/Quest_Question_UI.png"); // 완료 가능
        _markerRenderer->set_color(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f)); // 원래의 흰색/밝은 물음표
    }
    
    // 거리가 멀거나 대화 중이면 알파값 조절 (서서히 사라짐/나타남)
    if (_isTalking || state == common::packet::QuestState::REWARDED) {
        // 이미 UIRenderer에서 사용하는 _currentAlpha를 공유할 수도 있지만, 별도의 알파 변수를 쓰는 것이 안전할 수 있습니다. 
        // 여기서는 기존 _currentAlpha 로직 대신 바로 숨깁니다. (혹은 _markerRenderer가 별도의 알파 변수를 가져도 됩니다.)
        _markerRenderer->set_alpha(0.0f);
    } else {
        _markerRenderer->set_alpha(1.0f);
    }
}
