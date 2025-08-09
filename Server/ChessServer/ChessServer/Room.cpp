#include "pch.h"
#include "Room.h"

#include "PacketHandlers.h"

namespace chess::server
{
	constexpr int MAX_ROOM_PLAYERS = 4; // 최대 플레이어 수

	Room::Room(int room_id, int logic_thread_idx)
		: _room_id{ room_id }, _logic_thread_idx{ logic_thread_idx }, _max_players{ MAX_ROOM_PLAYERS }, _room_state{ RoomState::WAITING }
	{
		LOG("Room " << _room_id << " created. Assigned to Logic Thread " << _logic_thread_idx << " Max Players: " << static_cast<int>(_max_players));
	}
	void Room::AddPlayer(std::shared_ptr<SESSION> new_player)
	{
		if (new_player == nullptr) return;
		_players.insert({ new_player->_id, new_player });
		LOG("Player " << new_player->_id << " added to Room " << _room_id << ". Total: " << _players.size());
		
		if (_room_state == RoomState::WAITING)
		{
			StartGame();
		}
	}
	void Room::RemovePlayer(long long player_id)
	{
		auto it = _players.find(player_id);
		if (it != _players.end())
		{
			_players.erase(it);
			LOG("Player " << player_id << " removed from Room " << _room_id << ". Total: " << _players.size());
		}
	}

	void Room::StartGame()
	{
		_room_state = RoomState::PLAYING;
		LOG("Room " << _room_id << " is now in PLAYING state with " << GetPlayerCount() << " players.");

		// TODO: 게임 시작 패킷을 방에 있는 모든 플레이어에게 전송
		// 예: packet::SC_PACKET_GAME_START packet;
		// packet._type = ...
		// packet._size = ...
		// packet.who_is_white_player_id = ...
		// packet.who_is_black_player_id = ...
		// Broadcast(...);
	}

	void Room::Broadcast(const char* data, size_t size, long long except_id)
	{
		packet::PacketHeader* header = reinterpret_cast<packet::PacketHeader*>(const_cast<char*>(data));

		LOG("[Room::Broadcast] Room " << _room_id << " broadcasting packet type " << static_cast<int
		>(header->_type) << ". Except ID: " << except_id);

		for (auto const& [player_id, player_session] : _players)
		{
			if (player_session && player_id != except_id)
			{
				player_session->do_send(data, size);
				LOG("[Room::Broadcast]   -> Sent to player ID: " << player_id);
			}
		}
	}

	void Room::SendAllPlayersInfoToNewPlayer(std::shared_ptr<SESSION> new_player)
	{
		for (auto const& [player_id, existing_player] : _players)
		{
		    if (existing_player)
		    {
		    	packet::PacketStream spawn_stream = packet::MakeSpawnPlayerPacket(existing_player);
		    	new_player->do_send(spawn_stream.mutable_data(), spawn_stream.Size());
		    	LOG("[Room] Sent SPAWN_PLAYER of " << player_id << " to new session " << new_player->_id);
		    }
		 }
	}

	void Room::HandleAttack(std::shared_ptr<SESSION> attacker)
	{
		if (attacker == nullptr) return;

		// 4방향 좌표 (상, 하, 좌, 우)
		int dx[] = { 0, 0, -1, 1 };
		int dy[] = { 1, -1, 0, 0 };

		for (int i = 0; i < 4; ++i)
		{
			int target_x = attacker->_x + dx[i];
			int target_y = attacker->_y + dy[i];

			// 맵 경계 체크는 핸들러에서 이미 했을 수 있지만, 여기서도 한번 더 하는 것이 안전합니다.
			// (지금은 생략)

			// 방 내부의 플레이어 목록(_players)을 순회하며 공격 대상을 찾습니다.
			std::shared_ptr<SESSION> target_session = nullptr;
			for (auto const& [player_id, session] : _players)
			{
				if (session && session->_id != attacker->_id &&
					session->_x == target_x && session->_y == target_y)
				{
					target_session = session;
					break;
				}
			}

			if (target_session)
			{
				// 데미지 계산 (임시로 10)
				int16_t damage = 10;
				target_session->_hp -= damage;
				int32_t new_hp = target_session->_hp;
				if (new_hp < 0) { new_hp = 0; }

				LOG("[ROOM ATTACK] " << attacker->_id << " attacks " << target_session->_id << ". HP: " << new_hp);

				// 공격 결과 패킷 생성
				packet::SC_PACKET_ATTACK attackPacket;
				attackPacket._type = packet::PacketType::S2C_P_ATTACK;
				attackPacket._size = sizeof(attackPacket);
				attackPacket._attacker_id = attacker->_id;
				attackPacket._target_id = target_session->_id;
				attackPacket._damage = damage;
				attackPacket._target_current_hp = new_hp;

				// 방 전체에 공격 결과 브로드캐스팅
				Broadcast(reinterpret_cast<const char*>(&attackPacket), sizeof(attackPacket));
			}
		}
	}
}

