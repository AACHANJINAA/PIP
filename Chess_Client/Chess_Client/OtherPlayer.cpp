#include "stdafx.h"
#include "OtherPlayer.h"

OtherPlayer::OtherPlayer(int X, int Y)
{
	_NowX = X;
	_NowY = Y;
}

OtherPlayer::~OtherPlayer()
{

}

void OtherPlayer::Animate(float fTimeElapsed, Camera* pCamera, ID3D12GraphicsCommandList* pd3dCommandList)
{
	SetPosition(_NowX * _MoveDistance, 0.f, _NowY * _MoveDistance);
}

void OtherPlayer::Collision(float fElapsedTime)
{

}

void OtherPlayer::ProcessInput(float fElapsedTime, HWND hWnd, UINT nMessageID, POINT ptOldCursorPos)
{

}
