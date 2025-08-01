#pragma once
#include "CommonHeader.h"
#include "Server.h"

namespace chess::server
{
	struct RoomInfo //
	{
		int _room_id;
		uint8_t _player_count; // 방의 현재 인원 수
		// 필요하다면 방 제목, 게임 상태 등 추가 정보 포함 가능
	};
	enum class RoomState : uint8_t
	{
		WAITING,
		PLAYING
	};
	class Room
	{
	public:
		Room(int room_id, int logic_thread_idx);

		void AddPlayer(std::shared_ptr<SESSION> new_player);
		void RemovePlayer(long long player_id);

		void StartGame();
   
		// 방에 있는 모든 플레이어에게 패킷을 전송 (브로드캐스팅)
		void Broadcast(const char* data, size_t size, long long except_id = -1);

		void HandleAttack(std::shared_ptr<SESSION> attacker);




		int GetPlayerCount() const { return _players.size(); }
		int GetRoomId() const { return _room_id; }
		int GetLogicThreadIndex() const { return _logic_thread_idx; }
		RoomState GetRoomState() const { return _room_state; }

		bool IsFull() const { return _players.size() >= _max_players; }
	private:
		int _room_id;
		int _logic_thread_idx; // 이 방을 담당하는 로직 스레드의 인덱스
		uint8_t _max_players;
		RoomState _room_state;

		// 이 방에 속한 플레이어들의 목록
		std::unordered_map<long long, std::shared_ptr<SESSION>> _players;
	};
}
