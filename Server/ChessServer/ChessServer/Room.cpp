#include "pch.h"
#include "Room.h"

namespace chess::server
{
	Room::Room(int room_id, int logic_thread_idx)
		: _room_id{ room_id }, _logic_thread_idx{ logic_thread_idx }
	{
		LOG("Room " << _room_id << " created. Assigned to Logic Thread " << _logic_thread_idx);
	}
	void Room::AddPlayer(std::shared_ptr<SESSION> player)
	{
		if (player == nullptr) return;
		
		_players.insert({ player->_id, player });
		LOG("Player " << player->_id << " added to Room " << _room_id);
		
		// TODO: 방에 있는 다른 플레이어들에게 새로 들어온 유저의 정보를 알리는 패킷 전송
		// TODO: 새로 들어온 플레이어에게 방에 있던 다른 유저들의 정보를 알리는 패킷 전송
	}
	void Room::RemovePlayer(long long player_id)
	{
		auto it = _players.find(player_id);
		if (it != _players.end())
		{
			_players.erase(it);
			LOG("Player " << player_id << " removed from Room " << _room_id);

			// TODO: 방에 남은 플레이어들에게 나간 유저의 정보를 알리는 패킷 전송
		}
	}
	void Room::Broadcast(const char* data, size_t size, long long except_id)
	{
		for (auto const& [player_id, player_session] : _players)
		{
			if (player_session && player_id != except_id)
			{
				player_session->do_send(data, size);
			}
		}
	}
}

