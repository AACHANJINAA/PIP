#include "stdafx.h"
#include "MainPlayerScript.h"
#include "GameFramework.h"
#include "InputManager.h"
#include "NetworkManager.h"
#include "RenderComponent.h"
#include "ResourceManager.h"


void MainPlayerScript::update(float deltaTime)
{
    // --- 기존 MainPlayer::process_input의 로직이 여기로 완전히 이전되었습니다 ---
    if (InputManager::Instance()->IsKeyDown('W')) {
        move_pos(common::packet::MOVE_TYPE::MOVE_UP);
    }
    if (InputManager::Instance()->IsKeyDown('S')) {
        move_pos(common::packet::MOVE_TYPE::MOVE_DOWN);
    }
    if (InputManager::Instance()->IsKeyDown('D')) {
        move_pos(common::packet::MOVE_TYPE::MOVE_RIGHT);
    }
    if (InputManager::Instance()->IsKeyDown('A')) {
        move_pos(common::packet::MOVE_TYPE::MOVE_LEFT);
    }
    if (InputManager::Instance()->IsKeyDown('F')) {
        NetworkManager::Instance()->SendAttackPacket();
    }
}

void MainPlayerScript::awake()
{
    // --- 이 스크립트가 부착된 GameObject에 필요한 모든 설정을 여기서 수행 ---

    //auto renderer = game_object()->add_component<RenderComponent>();
    // 렏더러 컴포넌트 멤버변수로 들고 있어도 좋다.

    // 3. 컴포넌트들을 설정합니다.
    /*std::shared_ptr<Mesh> Chess_Mesh = std::make_shared<ReadFbxMesh>(
        GameFramework::Instance()->device().Get(),
        GameFramework::Instance()->command_list().Get(),
        "Resource/Test/testfbx_texture_included.fbx");*/
    //auto playerMesh = ResourceManager::Instance()->load_mesh("Resource/Test/testfbx_texture_included.fbx");
   // renderer->set_mesh(playerMesh);
   // renderer->set_pso_name("default"); // 사용할 PSO 이름 지정

    //TODO: 임시로 큐브 렌더 되는지 확인하기 위해서 주석 처리

    // 4. Transform을 통해 초기 위치 등을 설정합니다.
    transform()->set_local_position(XMFLOAT3(5.0f, 0.0f, 0.0f));
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
    // 계산된 방향으로 이동 패킷을 서버에 전송합니다.
    NetworkManager::Instance()->SendMovePacket(direction);
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
