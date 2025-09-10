#include "stdafx.h"
#include "OtherPlayer.h"

OtherPlayer::OtherPlayer(int x, int y, int z)
{
	SetPosition(x, y, z);
}

OtherPlayer::~OtherPlayer()
{

}

void OtherPlayer::Animate(float fTimeElapsed, Camera* pCamera, ID3D12GraphicsCommandList* pd3dCommandList)
{
	SetPosition(GetPosition().x * _MoveDistance, GetPosition().y * _MoveDistance, GetPosition().z * _MoveDistance);
}

void OtherPlayer::Collision(float fElapsedTime)
{

}

void OtherPlayer::ProcessInput(float fElapsedTime)
{

}
