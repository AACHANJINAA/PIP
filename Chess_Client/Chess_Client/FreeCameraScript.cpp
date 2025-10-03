#include "stdafx.h"
#include "FreeCameraScript.h"
#include "InputManager.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "CameraComponent.h" // CameraComponent 헤더 포함
#include "GameFramework.h"

FreeCameraScript::FreeCameraScript()
    : _moveSpeed(10.0f), _rotationSpeed(0.3f), _cameraComponent(nullptr)
{
}

// 역할: 이 스크립트가 활성화될 때, 필요한 CameraComponent를 확인하고 설정합니다.
void FreeCameraScript::awake()
{
    // 1. 같은 게임오브젝트에 CameraComponent가 있는지 확인합니다.
    _cameraComponent = game_object()->get_component<CameraComponent>().get();

    // 2. 만약 CameraComponent가 없다면, 새로 추가합니다.
    if (!_cameraComponent)
    {
        _cameraComponent = game_object()->add_component<CameraComponent>().get();
    }

    // 3. CameraComponent의 초기화 함수를 호출하여 상수 버퍼 등을 생성합니다.
    if (_cameraComponent)
    {
        _cameraComponent->initialize();
    }
    else
    {
        CERROR("FreeCameraScript::awake: Failed to get or create CameraComponent.");
    }
}

void FreeCameraScript::update(float delta_time)
{
    // 창이 활성화되어 있고 커서가 숨겨진 상태일 때만 입력을 처리합니다.
    if (GameFramework::Instance()->m_bIsWindowActive && !InputManager::Instance()->GetIsShowCusor())
    {
        process_mouse_input();
        process_keyboard_input(delta_time);
    }

    // ESC 키를 누르면 커서를 보이거나 숨깁니다.
    if (InputManager::Instance()->IsKeyDown(VK_ESCAPE))
    {
        InputManager::Instance()->ChangeShowCusor();
    }
}

// 역할 이전 (from FreeCamera::ProcessInput - mouse part)
void FreeCameraScript::process_mouse_input()
{
    POINT mouse_delta = InputManager::Instance()->GetMouseDelta();

    if (mouse_delta.x != 0 || mouse_delta.y != 0)
    {
        float yaw = static_cast<float>(mouse_delta.x) * _rotationSpeed;
        float pitch = static_cast<float>(mouse_delta.y) * _rotationSpeed;

        transform()->rotate(pitch, yaw, 0.0f);
    }
}

// 역할 이전 (from FreeCamera::ProcessInput - keyboard part)
void FreeCameraScript::process_keyboard_input(float delta_time)
{
    TransformComponent* trans = transform();
    if (!trans) return;

    XMFLOAT3 move_direction = { 0.0f, 0.0f, 0.0f };

    if (InputManager::Instance()->IsKeyPress('W'))
    {
        move_direction = Vector3::Add(move_direction, trans->forward());
    }
    if (InputManager::Instance()->IsKeyPress('S'))
    {
        move_direction = Vector3::Add(move_direction, Vector3::ScalarProduct(trans->forward(), -1.0f,
            false));
    }
    if (InputManager::Instance()->IsKeyPress('D'))
    {
        move_direction = Vector3::Add(move_direction, trans->right());
    }
    if (InputManager::Instance()->IsKeyPress('A'))
    {
        move_direction = Vector3::Add(move_direction, Vector3::ScalarProduct(trans->right(), -1.0f,
            false));
    }
    if (InputManager::Instance()->IsKeyPress(VK_SPACE))
    {
        move_direction.y += 1.0f;
    }
    if (InputManager::Instance()->IsKeyPress(VK_LCONTROL))
    {
        move_direction.y -= 1.0f;
    }

    if (Vector3::Length(move_direction) > 0.0f)
    {
        XMFLOAT3 normalized_direction = Vector3::Normalize(move_direction);
        XMFLOAT3 new_position = Vector3::Add(trans->local_position(), normalized_direction, _moveSpeed *
            delta_time);
        trans->set_local_position(new_position);
    }
}