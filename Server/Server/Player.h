#pragma once
namespace PIP
{
	class Player
	{
	public:
		Player(long long owner_id);
		Player();
	public:
		common::Vec3				_position;
		common::Quat				_rotation;
		std::string					_name;
		short						_hp;
		short						_max_hp;
		short						_level;
		uint32_t					_exp;
		int							_damage;
		common::packet::OBJECT_STATE _state = common::packet::OBJECT_STATE::IDLE;
		JPH::BodyID _physicsBodyID;
	private:
		long long _owner_id; // 이 플레이어의 소유자 세션 ID	
	};
}


