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
