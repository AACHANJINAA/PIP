#include "stdafx.h"
#include "OtherPlayer.h"

OtherPlayer::OtherPlayer(int x, int y, int z)
{
	if (OtherPlayer_Trasnform) OtherPlayer_Trasnform->SetPosition(x, y, z);
}

OtherPlayer::~OtherPlayer()
{

}

void OtherPlayer::Animate(float fTimeElapsed, Camera* pCamera, ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (OtherPlayer_Trasnform)
	{
		OtherPlayer_Trasnform->SetPosition(OtherPlayer_Trasnform->GetPosition().x * _MoveDistance,
										   OtherPlayer_Trasnform->GetPosition().y * _MoveDistance,
										   OtherPlayer_Trasnform->GetPosition().z * _MoveDistance);
	}
}

void OtherPlayer::Collision(float fElapsedTime)
{

}

void OtherPlayer::ProcessInput(float fElapsedTime)
{

}
