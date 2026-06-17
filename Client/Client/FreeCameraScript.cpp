#include "stdafx.h"
#include "FreeCameraScript.h"

#include <algorithm>
#include "InputManager.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "CameraComponent.h" // CameraComponent 헤더 포함
#include "GameFramework.h"
#include "ObjectManager.h"
#include "LightManager.h"
#include "TargetingComponent.h"
#include "ReadGLTFMesh.h"
#include "TerrainLoader.h"

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
    _isFreeCameraMode = false;
}

void FreeCameraScript::update(float delta_time)
{
 
}

void FreeCameraScript::late_update(float delta_time)
{
    if(_isSinamaticCameraMode)
    {
        // 시네마틱 카메라 모드에서는 입력을 무시합니다.
        return;
	}

    if (InputManager::instance()->IsKeyDown('L')) // 'L' 키 입력 감지 (한 번만)
    {
        _isFreeCameraMode = !_isFreeCameraMode; // 자유 카메라 모드 토글
        // 자유 카메라 모드가 켜지면 커서 숨김, 꺼지면 커서 보임
        InputManager::instance()->ChangeShowCusor();

        transform()->set_camera_rotation_mode(_isFreeCameraMode);
    }

    if (_isFreeCameraMode)
    {
        free_camera_update(delta_time);
    }
    else
    {
        player_camera_update(delta_time);
    }

    // ESC 키가 눌리면 커서를 보이거나 숨깁니다.
    if (InputManager::instance()->IsKeyDown(VK_ESCAPE))
    {
        InputManager::instance()->ChangeShowCusor();
    }

    // [추가] 카메라 쉐이크(Trauma) 처리
    if (_trauma > 0.0f)
    {
        // Trauma 값에 따라 흔들림 정도(Shake) 계산 (제곱 또는 세제곱으로 자연스럽게)
        float shake = _trauma * _trauma;
        
        // Perlin Noise 대신 난수를 사용하여 방향 무작위화
        float offsetX = _maxShakeOffset * shake * (((rand() % 100) / 100.0f) * 2.0f - 1.0f);
        float offsetY = _maxShakeOffset * shake * (((rand() % 100) / 100.0f) * 2.0f - 1.0f);

        auto cam = game_object()->get_component<CameraComponent>();
        if (cam)
        {
            cam->set_shake_offset({ offsetX, offsetY, 0.0f });
        }

        // Trauma 자연 감소 (선형 감소)
        _trauma -= delta_time * 1.5f; // 초당 1.5씩 감소
        if (_trauma < 0.0f) _trauma = 0.0f;
    }
    else
    {
        auto cam = game_object()->get_component<CameraComponent>();
        if (cam)
        {
            cam->set_shake_offset({ 0.0f, 0.0f, 0.0f });
        }
    }
}

void FreeCameraScript::free_camera_update(float delta_time)
{
    process_mouse_input(delta_time);
    process_keyboard_input(delta_time);
}

void FreeCameraScript::player_camera_update(float delta_time)
{
    // 창이 활성화되어 있고 커서가 숨겨진 상태일 때만 입력을 처리합니다.
    if (GameFramework::instance()->m_bIsWindowActive && !InputManager::instance()->GetIsShowCusor())
    {
        auto player = ObjectManager::instance()->find_by_name("MainPlayer");
		bool locked = false;
        if (player)
        {
            auto targeting = player->get_component<TargetingComponent>();
            locked = targeting && targeting->is_locked_on();
        }

        if (locked) {
            // [사용자 의도] 플레이어의 회전값(Yaw)을 카메라에 그대로 복사
            // 플레이어가 적을 보고 있으면 카메라도 플레이어 등 뒤에서 적을 보게 됨
            float playerYaw = player->transform()->local_rotation_euler().y;

            // Pitch는 약간 아래를 내려다보도록 고정 (예: 15도)
            transform()->set_camera_rotate(15.0f, playerYaw + 180.f, 0.0f);
        }
        else {
            // 일반 상태에선 기존 마우스 입력 사용
            process_mouse_input(delta_time);
        }

		// transform()->set_local_position({ 0.f, 0.f, 0.f });
        player_camera_conflict_update(delta_time);
    }
}

void FreeCameraScript::player_camera_conflict_update(float delta_time)
{
    auto player = ObjectManager::instance()->find_by_name("MainPlayer");
    // 플레이어가 생성된 후라면? -> DW설명 : 플레이어가 바로 생성되는 것이 아니기 때문에 이렇게 해주어야 함
    if (player && player->transform())
    {
        // 1. 레이캐스트 시작점 (플레이어의 머리)
        XMFLOAT3 playerPos = player->transform()->position();
        XMFLOAT3 rayStart = XMFLOAT3{ playerPos.x, playerPos.y + (player->transform()->get_world_scale().y * 1.5f), playerPos.z };

        // 2. 카메라가 원래 있어야 할 이상적인 위치 계산
        XMFLOAT3 camForward = transform()->forward();
        XMFLOAT3 camUp = transform()->up();

        XMVECTOR vRayStart = XMLoadFloat3(&rayStart);
        XMVECTOR vCamForward = XMLoadFloat3(&camForward);
        XMVECTOR vCamUp = XMLoadFloat3(&camUp);

        XMVECTOR vIdealPos = vRayStart + (vCamForward * (_thirdPersonOffsetDistance_back - _dynamicZoomOffset)) + (vCamUp * _thirdPersonOffsetDistance_top);

        // 3. 레이 방향 및 최대 길이 계산
        XMVECTOR vRayDir = XMVector3Normalize(vIdealPos - vRayStart);
        float maxDistance = XMVectorGetX(XMVector3Length(vIdealPos - vRayStart));

        float minHitDistance = maxDistance; // 가장 가까운 충돌 거리를 저장
        bool bHit = false;

        // 4. ObjectManager를 순회하며 Mesh의 OBB와 레이캐스트 충돌 검사
        const auto& allGameObjects = ObjectManager::instance()->get_all_game_objects();
        for (const auto& obj : allGameObjects)
        {
            if (!obj || !obj->is_enable() || obj->is_destroyed() ||
                obj->is_in_layer("Player") || obj->is_in_layer("OtherPlayer") || obj->is_in_layer("Enemy") || obj->name() == "Camera" || obj->name() == "TestMesh"
                || obj->name() == "QuestNPC")
            {
                continue;
            }

            // 지형(Landscape, Floor)은 OBB 검사에서 제외
            if (obj->name().find("Landscape") != std::string::npos ||
                obj->name().find("Floor") != std::string::npos)
            {
                continue;
            }

            auto renderComp = obj->get_component<RenderComponent>();
            if (renderComp && renderComp->mesh())
            {
                float hitDist = 0.0f;
                XMMATRIX worldMatrix = XMLoadFloat4x4(&obj->transform()->world_matrix());

                auto gltfMesh = std::dynamic_pointer_cast<ReadGLTFMesh>(renderComp->mesh());
                if (gltfMesh)
                {
                    // 프리미티브 단위 정밀 검사
                    if (gltfMesh->intersects_ray(vRayStart, vRayDir, worldMatrix, hitDist))
                    {
                        if (hitDist < minHitDistance)
                        {
                            minHitDistance = (hitDist <= 0.0f) ? 0.01f : hitDist;
                            bHit = true;
                        }
                    }
                }
                else
                {
                    // 일반 단일 박스 검사
                    BoundingOrientedBox worldOBB;
                    renderComp->mesh()->bounding_box().Transform(worldOBB, worldMatrix);
                    if (worldOBB.Intersects(vRayStart, vRayDir, hitDist))
                    {
                        if (hitDist >= 0.0f && hitDist < minHitDistance)
                        {
                            minHitDistance = (hitDist == 0.0f) ? 0.01f : hitDist;
                            bHit = true;
                        }
                    }
                }
            }
        }

        // 5. 지형(Terrain) 높이 정밀 충돌 검사 (Ray Marching)
        float stepSize = 0.01f;
        float currentDist = 0.0f;
        float terrainSkinWidth = 0.01f; // 바닥 여백

        XMFLOAT3 startPos;
        XMStoreFloat3(&startPos, vRayStart);
        XMFLOAT3 dir;
        XMStoreFloat3(&dir, vRayDir);

        while (currentDist <= minHitDistance)
        {
            float checkX = startPos.x + dir.x * currentDist;
            float checkY = startPos.y + dir.y * currentDist;
            float checkZ = startPos.z + dir.z * currentDist;

            float groundHeight = TerrainLoader::get_height_anywhere(checkX, checkZ);

            if (checkY < groundHeight + terrainSkinWidth)
            {
                minHitDistance = std::max(0.0f, currentDist - 0.01f);
                bHit = true;
                break;
            }

            currentDist += stepSize;
        }

        // 6. 최종 위치 결정 및 즉시 적용 (보간 완전 제거)
        float skinWidth = 0.01f; // 일반 오브젝트 벽 여백
        if (bHit)
        {
            minHitDistance = std::max(0.0f, minHitDistance - skinWidth);
        }

        XMVECTOR vFinalPos = vRayStart + (vRayDir * minHitDistance);

        XMFLOAT3 finalPos;
        XMStoreFloat3(&finalPos, vFinalPos);
        transform()->set_local_position(finalPos);
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
        if(_moveSpeed > 1.f)
        {
            _moveSpeed -= 2.0f;
			_moveSpeed = std::max(_moveSpeed, 1.f);
		}
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