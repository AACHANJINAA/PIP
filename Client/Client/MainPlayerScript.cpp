#include "stdafx.h"
#include "MainPlayerScript.h"
#include "GameFramework.h"
#include "InputManager.h"
#include "NetworkManager.h"
#include "RenderComponent.h"
#include "ResourceManager.h"
#include "TransformComponent.h"
#include "TimerManager.h"


void MainPlayerScript::update(float deltaTime)
{
    // --- 이동 입력 처리 및 로컬 위치 업데이트 ---
    auto current_transform = this->transform();
    if (!current_transform) return;

    common::Vec3 move_direction{};
    bool is_moving = false;

    if (InputManager::Instance()->IsKeyPress('W')) {
        move_direction = move_direction + common::Vec3Forward;
        is_moving = true;
    }
    if (InputManager::Instance()->IsKeyPress('S')) {
        move_direction = move_direction + common::Vec3Backward;
        is_moving = true;
    }
    if (InputManager::Instance()->IsKeyPress('D')) {
        move_direction = move_direction + common::Vec3Right;
        is_moving = true;
    }
    if (InputManager::Instance()->IsKeyPress('A')) {
        move_direction = move_direction + common::Vec3Left;
        is_moving = true;
    }

    if (is_moving) {
        move_direction = common::Normalize(move_direction); // 대각선 이동 시 속도가 빨라지지 않도록 정규화
        const float speed = 5.0f; // 이동 속도 (임의의 값, 필요시 조정)
        auto new_pos = current_transform->local_position() + (move_direction * speed * deltaTime);
        current_transform->set_local_position(new_pos);
    }

    // --- 50ms 마다 위치 정보 전송 ---
    _sendTimer += deltaTime;
    if (_sendTimer >= _sendInterval) {
        _sendTimer = 0.f;
        NetworkManager::Instance()->SendMovePacket(current_transform->local_position());
    }
}

void MainPlayerScript::awake()
{
	_renderComponent = this->game_object()->get_component<RenderComponent>().get();
	auto character_mesh = ResourceManager::Instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");
    _renderComponent->set_mesh(character_mesh);
    _renderComponent->set_pso_name("default");
}

void MainPlayerScript::move_pos(common::packet::MOVE_TYPE cmd)
{
    common::Vec3 direction{};
    switch (cmd)
    {
    case common::packet::MOVE_TYPE::MOVE_UP:
        direction = common::Vec3Forward;
        break;
    case common::packet::MOVE_TYPE::MOVE_DOWN:
        direction = common::Vec3Backward;
        break;
    case common::packet::MOVE_TYPE::MOVE_RIGHT:
        direction = common::Vec3Right;
        break;
    case common::packet::MOVE_TYPE::MOVE_LEFT:
        direction = common::Vec3Left;
        break;
    default:
        return; // 정의되지 않은 타입이면 아무것도 하지 않음
    }

    this->game_object()->get_component<TransformComponent>()->set_local_position(
        {
            this->position().x + direction.x * TimerManager::Instance()->GetTimeElapsed(),
            this->position().y + direction.y * TimerManager::Instance()->GetTimeElapsed(),
            this->position().z + direction.z * TimerManager::Instance()->GetTimeElapsed()
        }
	);
	// 서버로 이동 명령 전송은 정해진 타임에 전송되도록 함
}

//MainPlayer::MainPlayer(int x, int y, int z)
//{
//	if(MainPlayer_Trasnform) MainPlayer_Trasnform->set_position(x, y, z);
//}
//
//MainPlayer::~MainPlayer()
//{
//
//}
//
//void MainPlayer::animate(float elapsed_time, Camera* camera, ID3D12GraphicsCommandList* command_list)
//{
//	// SetPosition(GetPosition().x * _MoveDistance, GetPosition().y * _MoveDistance, GetPosition().z * _MoveDistance);
//}
//
//void MainPlayer::collision(float elapsed_time)
//{
//
//}
//
//void MainPlayer::process_input(float elapsed_time)
//{
//	if (InputManager::Instance()->IsKeyDown('W')) {
//		Move_Pos(common::packet::MOVE_TYPE::MOVE_UP);
//	}
//	if (InputManager::Instance()->IsKeyDown('S')) {
//		Move_Pos(common::packet::MOVE_TYPE::MOVE_DOWN);
//	}
//	if (InputManager::Instance()->IsKeyDown('D'))
//	{
//		Move_Pos(common::packet::MOVE_TYPE::MOVE_RIGHT);
//	}
//	if (InputManager::Instance()->IsKeyDown('A'))
//	{
//		Move_Pos(common::packet::MOVE_TYPE::MOVE_LEFT);
//	}
//	if (InputManager::Instance()->IsKeyDown('F'))
//	{
//		ClientPacketManager::Instance()->SendAttackPacket();
//	}
//}
//
//void MainPlayer::Move_Pos(common::packet::MOVE_TYPE Cmd)
//{
//	common::Vec3 direction{};
//	switch (Cmd)
//	{
//		case common::packet::MOVE_TYPE::MOVE_UP:
//			direction = common::Vec3Forward;
//			break;
//		case common::packet::MOVE_TYPE::MOVE_DOWN:
//			direction = common::Vec3Backward;
//			break;
//		case common::packet::MOVE_TYPE::MOVE_RIGHT:
//			direction = common::Vec3Right;
//			break;
//		case common::packet::MOVE_TYPE::MOVE_LEFT:
//			direction = common::Vec3Left;
//			// 서버로 나 위로 이동
//			// 서버는 위치값 계산
//			// 서버는 위치값을 클라이언트로 전송
//			//TODO: common::Vector3 타입(XMFLOAT3임)으로 방향보내기 필요(Normalize 필요)
//			break;
//		case common::packet::MOVE_TYPE::error:
//		break;
//
//	default:
//		break;
//	}
//	ClientPacketManager::Instance()->SendMovePacket(direction);
//}
