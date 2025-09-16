#include "stdafx.h"
#include "OtherPlayer.h"

OtherPlayer::OtherPlayer(int x, int y, int z)
{
	set_position(x, y, z);
}

OtherPlayer::~OtherPlayer()
{

}

void OtherPlayer::animate(float elapsed_time, Camera* camera, ID3D12GraphicsCommandList* command_list)
{
	set_position(position().x * _MoveDistance, position().y * _MoveDistance, position().z * _MoveDistance);
}

void OtherPlayer::collision(float elapsed_time)
{

}

void OtherPlayer::process_input(float elapsed_time)
{

}
