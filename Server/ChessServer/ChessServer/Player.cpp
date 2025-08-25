#include "pch.h"
#include "Player.h"

namespace chess
{
	Player::Player(long long owner_id) 
		:
		_position{0.f, 0.f, 0.f},
		_name {"DefaultName" },
		_hp { 100 },
		_max_hp{ 100 },
		_level { 0 },
		_exp { 0 },
		_owner_id{ owner_id }
	{}
	Player::Player()
		: _position{ 0.f, 0.f, 0.f },
		_name{ "InvalidName" },
		_hp{ 0 },
		_max_hp{ 0 },
		_level{0},
		_exp{ 0 },
		_owner_id{ -1 }
	{}
}
