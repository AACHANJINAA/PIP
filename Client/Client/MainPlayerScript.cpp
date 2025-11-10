#include "stdafx.h"
#include "MainPlayerScript.h"
#include "GameFramework.h"
#include "InputManager.h"
#include "NetworkManager.h"
#include "RenderComponent.h"
#include "Renderer.h"
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

    if (InputManager::instance()->IsKeyPress('W')) {
        move_direction = Vector3::Add(move_direction, common::Vec3Forward);
        is_moving = true;
    }
    if (InputManager::instance()->IsKeyPress('S')) {
        move_direction = Vector3::Add(move_direction,common::Vec3Backward);
        is_moving = true;
    }
    if (InputManager::instance()->IsKeyPress('D')) {
        move_direction = Vector3::Add(move_direction ,common::Vec3Right);
        is_moving = true;
    }
    if (InputManager::instance()->IsKeyPress('A')) {
        move_direction = Vector3::Add(move_direction ,common::Vec3Left);
        is_moving = true;
    }
    // 나중에 점프로 변경
    if (InputManager::instance()->IsKeyPress('Q')) {
        move_direction = Vector3::Add(move_direction ,common::Vec3Up);
        is_moving = true;
    }
    if (InputManager::instance()->IsKeyPress('E')) {
        move_direction = Vector3::Add(move_direction ,common::Vec3Down);
        is_moving = true;
	}


    if (is_moving) {
        move_direction = common::Normalize(move_direction); // 대각선 이동 시 속도가 빨라지지 않도록 정규화
        const float speed = 5.0f; // 이동 속도 (임의의 값, 필요시 조정)
        auto new_pos = Vector3::Add(current_transform->local_position() ,Vector3::ScalarProduct(move_direction, speed * deltaTime));
        current_transform->set_local_position(new_pos);
    }

    // --- 50ms 마다 위치 정보 전송 ---
    _sendTimer += deltaTime;
    if (_sendTimer >= _sendInterval) {
        _sendTimer = 0.f;
        NetworkManager::instance()->SendMovePacket(current_transform->local_position());
    }
}

void MainPlayerScript::awake()
{
	_renderComponent = game_object()->add_component<RenderComponent>().get();
    auto playerMesh = ResourceManager::instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");

    // 재질 및 쉐이더 설정
    _renderComponent->set_mesh(playerMesh);

	// ResourceManager을 통해 재질 생성 및 셰이더 할당
    std::string material_name = "player_material";
	ResourceManager::instance()->create_material(material_name);
	ResourceManager::instance()->set_shader_for_material(material_name, "gltf");

    // gltf
    _renderComponent->set_pso_name("gltf");

    // 위치, 회전 정보
    transform()->set_local_rotation(-90.f, 0.f, 0.f);
    transform()->set_local_scale({ 200.0f, 200.0f, 200.0f });
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
            this->position().x + direction.x * TimerManager::instance()->GetTimeElapsed(),
            this->position().y + direction.y * TimerManager::instance()->GetTimeElapsed(),
            this->position().z + direction.z * TimerManager::instance()->GetTimeElapsed()
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
