#include "stdafx.h"
#include "ToolCameraScript.h"
#include "InputManager.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "CameraComponent.h" // CameraComponent 헤더 포함
#include "GameFramework.h"
#include "ObjectManager.h"
#include "LightManager.h"

ToolCameraScript::ToolCameraScript()
    : _moveSpeed(10.0f), _rotationSpeed(15.0f), _cameraComponent(nullptr)
{
}

// 역할: 이 스크립트가 활성화될 때, 필요한 CameraComponent를 확인하고 설정합니다.
void ToolCameraScript::awake()
{
    // RequireComponent 시스템 덕분에 get_component는 항상 성공합니다.
     // 또한 CameraComponent는 생성자에서 스스로 초기화까지 완료합니다.
     // 따라서 awake에서는 그저 포인터를 캐싱해두기만 하면 됩니다.
    _cameraComponent = game_object()->get_component<CameraComponent>().get();
}

void ToolCameraScript::update(float delta_time)
{

}

void ToolCameraScript::late_update(float delta_time)
{
    if (InputManager::instance()->IsKeyDown('L')) // 'L' 키 입력 감지 (한 번만)
    {
        _isFreeCameraMode = !_isFreeCameraMode; // 자유 카메라 모드 토글
        // 자유 카메라 모드가 켜지면 커서 숨김, 꺼지면 커서 보임
        InputManager::instance()->ChangeShowCusor();
    }

    // 창이 활성화되어 있고 커서가 숨겨진 상태일 때만 입력을 처리합니다.
    if (GameFramework::instance()->m_bIsWindowActive && !InputManager::instance()->GetIsShowCusor())
    {
        process_mouse_input(delta_time);

        if (_isFreeCameraMode)
        {
            process_keyboard_input(delta_time);
        }
        else
        {

        }
    }

    // ESC 키를 누르면 커서를 보이거나 숨깁니다.
    if (InputManager::instance()->IsKeyDown(VK_ESCAPE))
    {
        InputManager::instance()->ChangeShowCusor();
    }
}

// 역할 이전 (from FreeCamera::ProcessInput - mouse part)
void ToolCameraScript::process_mouse_input(float delta_time)
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
void ToolCameraScript::process_keyboard_input(float delta_time)
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
    if (InputManager::instance()->IsKeyPress('Q'))
    {
        move_direction.y += 1.0f;
    }
    if (InputManager::instance()->IsKeyPress('E'))
    {
        move_direction.y -= 1.0f;
    }
    if (InputManager::instance()->IsKeyPress('J'))
    {
        Light* sun = LightManager::instance()->get_light(0); // 첫 번째 라이트 (태양)
        if (sun)
        {
            // 태양 방향을 Y축 기준으로 회전 (시계 방향)
            float angle = delta_time * 0.5f; // 회전 속도
            float x = sun->m_vDirection.x * cos(angle) + sun->m_vDirection.z * sin(angle);
            float z = -sun->m_vDirection.x * sin(angle) + sun->m_vDirection.z * cos(angle);
            sun->m_vDirection.x = x;
            sun->m_vDirection.z = z;

            LightManager::instance()->update(); // 변경사항 GPU로 전송

            // 디버그 출력
            char buf[256];
            sprintf_s(buf, "Sun Direction: %.2f, %.2f, %.2f\n",
                sun->m_vDirection.x, sun->m_vDirection.y, sun->m_vDirection.z);
            OutputDebugStringA(buf);
        }
    }

    if (InputManager::instance()->IsKeyPress('K'))
    {
        Light* sun = LightManager::instance()->get_light(0);
        if (sun)
        {
            // 태양 방향을 Y축 기준으로 회전 (반시계 방향)
            float angle = -delta_time * 0.5f;
            float x = sun->m_vDirection.x * cos(angle) + sun->m_vDirection.z * sin(angle);
            float z = -sun->m_vDirection.x * sin(angle) + sun->m_vDirection.z * cos(angle);
            sun->m_vDirection.x = x;
            sun->m_vDirection.z = z;

            LightManager::instance()->update();

            char buf[256];
            sprintf_s(buf, "Sun Direction: %.2f, %.2f, %.2f\n",
                sun->m_vDirection.x, sun->m_vDirection.y, sun->m_vDirection.z);
            OutputDebugStringA(buf);
        }
    }


    if (Vector3::Length(move_direction) > 0.0f)
    {
        XMFLOAT3 normalized_direction = Vector3::Normalize(move_direction);
        XMFLOAT3 new_position = Vector3::Add(trans->local_position(), normalized_direction, _moveSpeed * delta_time);
        trans->set_local_position(new_position);
    }
}