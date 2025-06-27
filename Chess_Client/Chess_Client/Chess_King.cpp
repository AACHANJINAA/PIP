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
		Move_Pos(CommandType::MOVE_UP);
	}

	if (GetAsyncKeyState(VK_DOWN) & 0x0001) {
		Move_Pos(CommandType::MOVE_DOWN);
	}

	if (GetAsyncKeyState(VK_RIGHT) & 0x0001) {
		Move_Pos(CommandType::MOVE_RIGHT);
	}

	if (GetAsyncKeyState(VK_LEFT) & 0x0001) {
		Move_Pos(CommandType::MOVE_LEFT);
	}
}

void CChess_King::Move_Pos(CommandType Cmd)
{
	switch (Cmd)
	{
	case CommandType::MOVE_UP:
		if(_NowY < 7)
		{
			++_NowY;
		}
		break;


	case CommandType::MOVE_DOWN:
		if (_NowY > 0)
		{
			--_NowY;
		}
		break;


	case CommandType::MOVE_RIGHT:
		if (_NowX < 7)
		{
			++_NowX;
		}
		break;


	case CommandType::MOVE_LEFT:
		if (_NowX > 0)
		{
			--_NowX;
		}
		break;


	case CommandType::CONNECT:
		break;

	case CommandType::DISCONNECT:
		break;

	case CommandType::error:
		break;

	default:
		break;
	}
}
