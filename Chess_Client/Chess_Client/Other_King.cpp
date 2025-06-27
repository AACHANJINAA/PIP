#include "stdafx.h"
#include "Other_King.h"

COther_King::COther_King(int X, int Y)
{
	_NowX = X;
	_NowY = Y;
}

COther_King::~COther_King()
{

}

void COther_King::Animate(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList)
{
	SetPosition(_NowX * _MoveDistance, 0.f, _NowY * _MoveDistance);
}

void COther_King::Collision(float fElapsedTime)
{

}

void COther_King::ProcessInput(float fElapsedTime, HWND hWnd, UINT nMessageID, POINT ptOldCursorPos)
{

}

void COther_King::Move_Pos(CommandType Cmd)
{
	switch (Cmd)
	{
	case CommandType::MOVE_UP:
		if (_NowY < 7)
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
