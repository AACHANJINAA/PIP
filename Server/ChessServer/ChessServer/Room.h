#pragma once
#include "Server.h"

namespace chess::server
{
	
	enum class RoomState : uint8_t
	{
		WAITING,
		PLAYING
	};
	class SESSION;
	class Room
	{
	public:
		Room(int room_id, int logic_thread_idx);

		void AddPlayer(std::shared_ptr<SESSION> new_player);
		void RemovePlayer(long long player_id);

		void StartGame();
   
		// 방에 있는 모든 플레이어에게 패킷을 전송 (브로드캐스팅)
		void Broadcast(const char* data, size_t size, long long except_id = -1);

		void SendAllPlayersInfoToNewPlayer(std::shared_ptr<SESSION> new_player);

		void HandleAttack(std::shared_ptr<SESSION> attacker);

		size_t GetPlayerCount() const { return _players.size(); }
		int GetRoomId() const { return _room_id; }
		int GetLogicThreadIndex() const { return _logic_thread_idx; }
		RoomState GetRoomState() const { return _room_state; }

		bool IsFull() const { return static_cast<uint8_t>(_players.size()) >= _max_players; }
		bool CheckForCollision(Vector3 target_pos, Vector3 player_extents);
		//const std::unordered_map<long long, std::shared_ptr<SESSION>>& GetPlayers() const { return _players; }
	private:
		int _room_id;
		int _logic_thread_idx; // 이 방을 담당하는 로직 스레드의 인덱스
		uint8_t _max_players;
		RoomState _room_state;

		// 이 방에 속한 플레이어들의 목록
		std::unordered_map<long long, std::shared_ptr<SESSION>> _players;
	};
}
