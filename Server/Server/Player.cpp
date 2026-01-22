#include "pch.h"
#include "Player.h"


namespace PIP::GAME
{
	Player::Player(long long owner_id) 
		:
		_name {"DefaultName" },
		_hp { 100 },
		_max_hp{ 100 },
		_level { 0 },
		_exp { 0 },
		_damage{ 10 },
		GameObject((int)owner_id),
		_owner_id{ owner_id }
	{
		// 1. Transform 추가
		AddComponent<GAME::TransformComponent>();

		// 2. 물리 컨트롤러 추가 (플레이어도 이제 물리 적용!)
		AddComponent<GAME::CharacterControllerComponent>();

		SetName("Player_" + std::to_string(owner_id));
	}
}
