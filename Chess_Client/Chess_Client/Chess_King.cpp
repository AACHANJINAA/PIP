#include "stdafx.h"
#include "Chess_King.h"

CChess_King::CChess_King(int X, int Y)
{
	_NowX = X;
	_NowY = Y;
}

CChess_King::~CChess_King()
{

}

void CChess_King::Animate(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList)
{
	SetPosition(_NowX * _MoveDistance, 0.f, _NowY * _MoveDistance);
}

void CChess_King::Collision(float fElapsedTime)
{

}

void CChess_King::ProcessInput(float fElapsedTime, HWND hWnd, UINT nMessageID, POINT ptOldCursorPos)
{
	if (GetAsyncKeyState(VK_UP) & 0x0001) {
		Move_Pos(chess::packet::MOVE_TYPE::MOVE_UP);
	}

	if (GetAsyncKeyState(VK_DOWN) & 0x0001) {
		Move_Pos(chess::packet::MOVE_TYPE::MOVE_DOWN);
	}

	if (GetAsyncKeyState(VK_RIGHT) & 0x0001)
	{
		Move_Pos(chess::packet::MOVE_TYPE::MOVE_RIGHT);
	}

	if (GetAsyncKeyState(VK_LEFT) & 0x0001)
	{
		Move_Pos(chess::packet::MOVE_TYPE::MOVE_LEFT);
	}
}

void CChess_King::Move_Pos(chess::packet::MOVE_TYPE Cmd)
{
	switch (Cmd)
	{
		case chess::packet::MOVE_TYPE::MOVE_UP:
		case chess::packet::MOVE_TYPE::MOVE_DOWN:
		case chess::packet::MOVE_TYPE::MOVE_RIGHT:
		case chess::packet::MOVE_TYPE::MOVE_LEFT:
			// 서버로 나 위로 이동
			// 서버는 위치값 계산
			// 서버는 위치값을 클라이언트로 전송
			send_move_packet(Cmd);
			break;
		case chess::packet::MOVE_TYPE::error:
		break;

	default:
		break;
	}
}
