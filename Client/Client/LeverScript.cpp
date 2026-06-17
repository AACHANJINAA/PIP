#include "stdafx.h"
#include "gameobject.h"
#include "LeverScript.h"
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
#include "../Common/Packet.h"

void LeverScript::awake()
{
    auto renderer = game_object()->get_component<RenderComponent>();
    auto animation_component = game_object()->get_component<AnimationComponent>();

    if (!animation_component)
    {
        CERROR("애니메이션 컴포넌트 추가 안됨 확인!");
    }

    // 메쉬 및 애니메이션 로드 (레버 파일 기준)
    auto T1_Mesh = ResourceManager::instance()->load_mesh("Resource/Lever/Lever.gltf", true);
    auto mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(T1_Mesh);
    if (mesh)
    {
        mesh->load_animation_only("Resource/Lever/Animation/Lever_UP.gltf", "Lever_UP");
    }

    renderer->set_mesh(T1_Mesh);

    if (animation_component)
    {
        animation_component->add_animation("Lever_UP", T1_Mesh, "Lever_UP");
        animation_component->play("Lever_UP", false, 0.f); // 기본 상태
    }

    // skinned
    renderer->set_pso_name("skinned");

    // 위치, 회전, 스케일 설정은 Main_Scene에서 지정한 값을 유지합니다.
    auto trans = transform();

    // UI 초기화 (레버마다 개별 UI 객체 생성하여 깜빡임 충돌 방지)
    if(game_object()->name() == "Lever1")
    {
		_uiName = "Lever_interact_ui_1";
        _uiRenderer = UIManager::instance()->ui_component(UILayer::MIDDLE, "Lever_interact_ui_1");
    }
    else if(game_object()->name() == "Lever0")
    {
		_uiName = "Lever_interact_ui_0";
        _uiRenderer = UIManager::instance()->ui_component(UILayer::MIDDLE, "Lever_interact_ui_0");
    }
    else
    {
         CERROR("레버 이름이 예상과 다릅니다. UI 초기화 실패!");
         return;
	}
    _uiRenderer->set_size(_uiWidth, _uiHeight);
    UIManager::instance()->set_visible(UILayer::MIDDLE, _uiName, false);

    // y축 보정 (레버의 y축 높이와 정확히 일치하도록 0으로 설정)
    _uiYOffset = 0.0f;
}

void LeverScript::update(float deltaTime)
{
    // 상호작용 F키 UI 업데이트
    update_F_interaction_UI(deltaTime);

    auto mainPlayer = ObjectManager::instance()->find_object("MainPlayer");
    if (!mainPlayer) return;

    auto playerPos = mainPlayer->transform()->get_world_position();
    auto leverPos = transform()->get_world_position();
    
    DirectX::XMVECTOR vPos = DirectX::XMLoadFloat3(&playerPos);
    DirectX::XMVECTOR vLever = DirectX::XMLoadFloat3(&leverPos);
    DirectX::XMVECTOR distVec = DirectX::XMVector3Length(DirectX::XMVectorSubtract(vPos, vLever));
    float distance = DirectX::XMVectorGetX(distVec);

    bool isClose = (distance <= _interactionDistance);

    // 가까이 있고 아직 상호작용 안 했고 F키를 눌렀다면
    if (isClose && !_isInteracted && InputManager::instance()->IsKeyDown('F'))
    {
        // 1. 서버에 상호작용 패킷 전송 (ActionID::Common::INTERACT)
        // MainPlayer 위치와 회전값을 전송합니다 (유저 요청 참고)
        common::Vec3 playerLogicalPos = { playerPos.x, playerPos.y, playerPos.z };
        //auto playerRot = mainPlayer->transform()->get_world_rotation();
        
        // 각도는 안중요함
        common::Quat playerLogicalRot = { 0.f,0.f,0.f,0.f };

        NetworkManager::instance()->SendActionPacket(common::packet::ActionID::Common::INTERACT, -1, playerLogicalPos, playerLogicalRot);
    }
}

void LeverScript::interact()
{
    _isInteracted = true;
    auto animation_component = game_object()->get_component<AnimationComponent>();
    if (animation_component)
    {
        animation_component->play("Lever_UP", false, 0.5f);
    }
}

void LeverScript::update_F_interaction_UI(float deltaTime)
{
    auto mainPlayer = ObjectManager::instance()->find_object("MainPlayer");
    if (!mainPlayer || !_uiRenderer) return;

    auto camera = CameraComponent::get_main();
    if (!camera) return;

    auto playerPos = mainPlayer->transform()->get_world_position();
    auto leverPos = transform()->get_world_position();
    
    DirectX::XMVECTOR vPos = DirectX::XMLoadFloat3(&playerPos);
    DirectX::XMVECTOR vLever = DirectX::XMLoadFloat3(&leverPos);
    DirectX::XMVECTOR distVec = DirectX::XMVector3Length(DirectX::XMVectorSubtract(vPos, vLever));
    float distance = DirectX::XMVectorGetX(distVec);

    bool isClose = (distance <= _interactionDistance); // 가까운지? 가까우면 true, 멀면 false

    if (isClose)
    {
        _currentAlpha += _fadeSpeed * deltaTime;
        if (_currentAlpha > 1.0f) _currentAlpha = 1.0f;
        UIManager::instance()->set_visible(UILayer::MIDDLE, _uiName, true);
    }
    else
    {
        _currentAlpha -= _fadeSpeed * deltaTime;
        if (_currentAlpha < 0.0f) {
            _currentAlpha = 0.0f;
            UIManager::instance()->set_visible(UILayer::MIDDLE, _uiName, false);
            return; 
        }
    }

    if (_isInteracted)
    {
        // 상호작용 후에는 상호작용 F키 UI를 항상 보이지 않도록 설정
        UIManager::instance()->set_visible(UILayer::MIDDLE, _uiName, false);
    }
    else
    {
        UIManager::instance()->set_visible(UILayer::MIDDLE, _uiName, true);
    }

    if (UIManager::instance()->is_visible(UILayer::MIDDLE, _uiName))
    {
        auto projMat = DirectX::XMLoadFloat4x4(&camera->projection_matrix());
        auto viewMat = DirectX::XMLoadFloat4x4(&camera->view_matrix());
        auto worldMat = DirectX::XMMatrixIdentity();

        float screenWidth = static_cast<float>(FRAME_BUFFER_WIDTH);
        float screenHeight = static_cast<float>(FRAME_BUFFER_HEIGHT);

        DirectX::XMFLOAT3 uiWorldPos = leverPos;
        // UI가 레버 중앙에 뜨도록 y축으로 약간 올려줌 (필요시 조절)
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
