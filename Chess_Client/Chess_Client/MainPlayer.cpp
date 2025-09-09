#include "stdafx.h"
#include "MainPlayer.h"
#include "ClientPacketManager.h"

MainPlayer::MainPlayer(int x, int y, int z)
{
	SetPosition(x, y, z);
}

MainPlayer::~MainPlayer()
{

}

void MainPlayer::Animate(float fTimeElapsed, Camera* pCamera, ID3D12GraphicsCommandList* pd3dCommandList)
{
	// SetPosition(GetPosition().x * _MoveDistance, GetPosition().y * _MoveDistance, GetPosition().z * _MoveDistance);
}

void MainPlayer::Collision(float fElapsedTime)
{

}

void MainPlayer::ProcessInput(float fElapsedTime, HWND hWnd, UINT nMessageID, POINT ptOldCursorPos)
{
	if (GetAsyncKeyState(VK_UP) & 0x0001) {
		Move_Pos(common::packet::MOVE_TYPE::MOVE_UP);
	}

	if (GetAsyncKeyState(VK_DOWN) & 0x0001) {
		Move_Pos(common::packet::MOVE_TYPE::MOVE_DOWN);
	}

	if (GetAsyncKeyState(VK_RIGHT) & 0x0001)
	{
		Move_Pos(common::packet::MOVE_TYPE::MOVE_RIGHT);
	}

	if (GetAsyncKeyState(VK_LEFT) & 0x0001)
	{
		Move_Pos(common::packet::MOVE_TYPE::MOVE_LEFT);
	}
	
	if (GetAsyncKeyState('F') & 0x0001)
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
