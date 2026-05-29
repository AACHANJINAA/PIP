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

void QuestNPCScript::awake()
{
    auto renderer = game_object()->get_component<RenderComponent>();
    auto animation_component = game_object()->get_component<AnimationComponent>();

    if (!animation_component)
    {
        CERROR("애니메이션 컴포넌트 추가 안됨 튜플 확인!");
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
        trans->set_local_position(DirectX::XMFLOAT3(-215.27f, 6.59f, -366.41f));
    }

	// UI 초기화
    _uiRenderer = UIManager::instance()->ui_component(UILayer::MIDDLE, "F_interaction_UI");
    _uiRenderer->set_size(_uiWidth, _uiHeight);
    UIManager::instance()->set_visible(UILayer::MIDDLE, "F_interaction_UI", false);

	// y축 보정 (NPC 중앙에 UI가 뜨도록)
	_uiYOffset = transform()->get_world_scale().y * 1.5f;
}

void QuestNPCScript::update(float deltaTime)
{
    // 상호작용 F키 UI 업데이트
    update_F_interaction_UI(deltaTime);
}

void QuestNPCScript::update_F_interaction_UI(float deltaTime)
{
    auto mainPlayer = ObjectManager::instance()->find_object("MainPlayer");

    if (!mainPlayer || !_uiRenderer) 
    {
        return;
    }

    auto playerPos = mainPlayer->transform()->get_world_position();
    auto npcPos = transform()->get_world_position();

    DirectX::XMVECTOR pVec = DirectX::XMLoadFloat3(&playerPos);
    DirectX::XMVECTOR nVec = DirectX::XMLoadFloat3(&npcPos);
    DirectX::XMVECTOR distVec = DirectX::XMVector3Length(DirectX::XMVectorSubtract(pVec, nVec));
    float distance = DirectX::XMVectorGetX(distVec);

	bool isClose = (distance <= _interactionDistance); // 가까운지? 가까우면 true, 멀면 false

    if (isClose)
    {
        _currentAlpha += _fadeSpeed * deltaTime;
        if (_currentAlpha > 1.0f) 
        {
            _currentAlpha = 1.0f;
        }
        UIManager::instance()->set_visible(UILayer::MIDDLE, "F_interaction_UI", true);
    }
    else
    {
        _currentAlpha -= _fadeSpeed * deltaTime;
        if (_currentAlpha < 0.0f) 
        {
            _currentAlpha = 0.0f;
            UIManager::instance()->set_visible(UILayer::MIDDLE, "F_interaction_UI", false);
        }
    }

    if (_isTalking)
    {
		// 대화 중에는 상호작용 F키 UI를 항상 보이지 않도록 설정
        UIManager::instance()->set_visible(UILayer::MIDDLE, "F_interaction_UI", false);
    }

    if (_currentAlpha <= 0.0f)
    {
        _uiRenderer->set_color(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f));
        return;
    }

    _uiRenderer->set_color(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, _currentAlpha));

    auto camera = CameraComponent::get_main();
    if (camera) 
    {
        auto projMat = DirectX::XMLoadFloat4x4(&camera->projection_matrix());
        auto viewMat = DirectX::XMLoadFloat4x4(&camera->view_matrix());
        auto worldMat = DirectX::XMMatrixIdentity();

        float screenWidth = static_cast<float>(GameFramework::instance()->get_window_width());
        float screenHeight = static_cast<float>(GameFramework::instance()->get_window_height());

        DirectX::XMFLOAT3 uiWorldPos = npcPos;
		// UI가 NPC 중앙에 뜨도록 y축으로 약간 올려줌 (필요시 조절)
        uiWorldPos.y += _uiYOffset;

        DirectX::XMVECTOR worldPosVec = DirectX::XMLoadFloat3(&uiWorldPos);
        DirectX::XMVECTOR screenPosVec = DirectX::XMVector3Project(worldPosVec, 0, 0, screenWidth, screenHeight, 0.0f, 1.0f, projMat, viewMat, worldMat);

        float screenX = DirectX::XMVectorGetX(screenPosVec);
        float screenY = DirectX::XMVectorGetY(screenPosVec);
        float screenZ = DirectX::XMVectorGetZ(screenPosVec);

        if (screenZ < 0.0f || screenZ > 1.0f)
        {
            _uiRenderer->set_color(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f));
        }
        else
        {
            _uiRenderer->set_screen_position(screenX - (_uiWidth / 2.0f), screenY - (_uiHeight / 2.0f));
        }
    }
}

