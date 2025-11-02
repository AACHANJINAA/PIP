#include "stdafx.h"
#include "FreeCameraScript.h"
#include "InputManager.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "CameraComponent.h" // CameraComponent 헤더 포함
#include "GameFramework.h"
#include "ObjectManager.h"

FreeCameraScript::FreeCameraScript()
    : _moveSpeed(10.0f), _rotationSpeed(15.0f), _cameraComponent(nullptr)
{
}

// 역할: 이 스크립트가 활성화될 때, 필요한 CameraComponent를 확인하고 설정합니다.
void FreeCameraScript::awake()
{
    // RequireComponent 시스템 덕분에 get_component는 항상 성공합니다.
     // 또한 CameraComponent는 생성자에서 스스로 초기화까지 완료합니다.
     // 따라서 awake에서는 그저 포인터를 캐싱해두기만 하면 됩니다.
    _cameraComponent = game_object()->get_component<CameraComponent>().get();
}

void FreeCameraScript::update(float delta_time)
{
    // 창이 활성화되어 있고 커서가 숨겨진 상태일 때만 입력을 처리합니다.
    if (GameFramework::instance()->m_bIsWindowActive && !InputManager::instance()->GetIsShowCusor())
    {
        process_mouse_input(delta_time);

        // 플레이어가 생성된 후라면? -> DW설명 : 플레이어가 바로 생성되는 것이 아니기 때문에 이렇게 해주어야 함
        if (nullptr != ObjectManager::instance()->find_by_name("MainPlayer").get())
        {
			auto player = ObjectManager::instance()->find_by_name("MainPlayer");
            // 카메라 위치를 플레이어 위치로 동기화합니다.
            if (player && player->transform())
            {
                XMFLOAT3 playerPos = player->transform()->position();
                transform()->set_local_position(XMFLOAT3{playerPos.x, playerPos.y + player->transform()->get_world_scale().y * 0.5f, playerPos.z});
				transform()->move_forward(_thirdPersonOffsetDistance_back);
				transform()->move_up(_thirdPersonOffsetDistance_top);
			}
        }
        else
        {
            process_keyboard_input(delta_time);
        }
    }

    // ESC 키를 누르면 커서를 보이거나 숨깁니다.
    if (InputManager::instance()->IsKeyDown(VK_ESCAPE))
    {
        InputManager::instance()->ChangeShowCusor();
    }
}

// 역할 이전 (from FreeCamera::ProcessInput - mouse part)
void FreeCameraScript::process_mouse_input(float delta_time)
{
    POINT mouse_delta = InputManager::instance()->GetMouseDelta();

    if (mouse_delta.x != 0 || mouse_delta.y != 0)
    {
        float yaw = static_cast<float>(mouse_delta.x) * delta_time * _rotationSpeed;
        float pitch = static_cast<float>(mouse_delta.y) * delta_time * _rotationSpeed;

        transform()->camera_rotate(pitch, yaw, 0.0f);
    }
}

// 역할 이전 (from FreeCamera::ProcessInput - keyboard part)
void FreeCameraScript::process_keyboard_input(float delta_time)
{
    TransformComponent* trans = transform();
    if (!trans) return;

    XMFLOAT3 move_direction = { 0.0f, 0.0f, 0.0f };
   

    if (InputManager::instance()->IsKeyPress(VK_ADD))
    {
        _moveSpeed += 2.0f;
    }
    if (InputManager::instance()->IsKeyPress(VK_SUBTRACT))
    {
        _moveSpeed -= 2.0f;
    }

    if (InputManager::instance()->IsKeyPress('W'))
    {
        move_direction = Vector3::Add(move_direction, trans->forward());
    }
    if (InputManager::instance()->IsKeyPress('S'))
    {
        move_direction = Vector3::Add(move_direction, Vector3::ScalarProduct(trans->forward(), -1.0f, false));
    }
    if (InputManager::instance()->IsKeyPress('D'))
    {
        move_direction = Vector3::Add(move_direction, trans->right());
    }
    if (InputManager::instance()->IsKeyPress('A'))
    {
        move_direction = Vector3::Add(move_direction, Vector3::ScalarProduct(trans->right(), -1.0f, false));
    }
    if (InputManager::instance()->IsKeyPress(VK_SPACE))
    {
        move_direction.y += 1.0f;
    }
    if (InputManager::instance()->IsKeyPress(VK_LCONTROL))
    {
        move_direction.y -= 1.0f;
    }
    
           
    if (Vector3::Length(move_direction) > 0.0f)
    {
        XMFLOAT3 normalized_direction = Vector3::Normalize(move_direction);
        XMFLOAT3 new_position = Vector3::Add(trans->local_position(), normalized_direction, _moveSpeed * delta_time);
        trans->set_local_position(new_position);
    }
}