#include "stdafx.h"
#include "OtherPlayer.h"

OtherPlayer::OtherPlayer(int x, int y, int z)
{
	if (OtherPlayer_Trasnform) OtherPlayer_Trasnform->set_position(x, y, z);
}

OtherPlayer::~OtherPlayer()
{

}

void OtherPlayer::animate(float elapsed_time, Camera* camera, ID3D12GraphicsCommandList* command_list)
{
	if (OtherPlayer_Trasnform)
	{
		OtherPlayer_Trasnform->set_position(OtherPlayer_Trasnform->get_position().x * _MoveDistance,
										    OtherPlayer_Trasnform->get_position().y * _MoveDistance,
										    OtherPlayer_Trasnform->get_position().z * _MoveDistance);
	}
}

void OtherPlayer::collision(float elapsed_time)
{

}

void OtherPlayer::process_input(float elapsed_time)
{

}
