#include "stdafx.h"
#include "MainPlayer.h"
#include "ClientPacketManager.h"
#include "InputManager.h"

MainPlayer::MainPlayer(int x, int y, int z)
{
	set_position(x, y, z);
}

MainPlayer::~MainPlayer()
{

}

void MainPlayer::animate(float elapsed_time, Camera* camera, ID3D12GraphicsCommandList* command_list)
{
	// SetPosition(GetPosition().x * _MoveDistance, GetPosition().y * _MoveDistance, GetPosition().z * _MoveDistance);
}

void MainPlayer::collision(float elapsed_time)
{

}

void MainPlayer::process_input(float elapsed_time)
{
	if (InputManager::Instance()->IsKeyDown('W')) {
		Move_Pos(common::packet::MOVE_TYPE::MOVE_UP);
	}
	if (InputManager::Instance()->IsKeyDown('S')) {
		Move_Pos(common::packet::MOVE_TYPE::MOVE_DOWN);
	}
	if (InputManager::Instance()->IsKeyDown('D'))
	{
		Move_Pos(common::packet::MOVE_TYPE::MOVE_RIGHT);
	}
	if (InputManager::Instance()->IsKeyDown('A'))
	{
		Move_Pos(common::packet::MOVE_TYPE::MOVE_LEFT);
	}
	if (InputManager::Instance()->IsKeyDown('F'))
	{
		ClientPacketManager::Instance()->SendAttackPacket();
	}
}

void MainPlayer::Move_Pos(common::packet::MOVE_TYPE Cmd)
{
	common::Vec3 direction{};
	switch (Cmd)
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
			// 서버로 나 위로 이동
			// 서버는 위치값 계산
			// 서버는 위치값을 클라이언트로 전송
			//TODO: common::Vector3 타입(XMFLOAT3임)으로 방향보내기 필요(Normalize 필요)
			break;
		case common::packet::MOVE_TYPE::error:
		break;

	default:
		break;
	}
	ClientPacketManager::Instance()->SendMovePacket(direction);
}
