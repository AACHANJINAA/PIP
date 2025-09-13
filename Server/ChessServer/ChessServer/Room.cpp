#include "pch.h"
#include "Room.h"
#include "AIManager.h"
#include "Player.h"
#include "PacketHandlers.h"
#include "Timer.h"

namespace PIP::server
{
	constexpr int MAX_ROOM_PLAYERS = 4; // 최대 플레이어 수

	Room::Room(int room_id, int logic_thread_idx)
		: _room_id{ room_id }, _logic_thread_idx{ logic_thread_idx }, _max_players{ MAX_ROOM_PLAYERS }, _room_state{ RoomState::WAITING }
	{
		MYLOG("Room " << _room_id << " created. Assigned to Logic Thread " << _logic_thread_idx << " Max Players: " << static_cast<int>(_max_players));
	}

	void Room::Initialize()
	{
		for (int i = 0; i < 5; ++i)
		{
			// NPC ID는 플레이어와 겹치지 않도록 높은 수에서 시작 (AIManager에서 관리)
			int npcId = AIManager::Instance()->GetNewNpcId();
			common::Vec3 randomPos = { static_cast<float>(rand() % 50), 4.0f, static_cast<float>(rand() % 50)
			};

			auto npc = std::make_unique<NPC>(npcId, 1, _room_id, randomPos);
			AddNPC(std::move(npc));

			// 생성된 NPC의 AI를 1초 뒤에 처음으로 실행하도록 타이머에 등록
			// AI 로직은 AIManager에 위임하고, Room과 NPC 정보를 넘겨줍니다.
			Timer::Instance()->AddTimerJob(std::chrono::milliseconds(10), [this, npcId]() {
				AIManager::Instance()->UpdateNPC(this, npcId);
			}); 
		}
	}

	void Room::EnterPlayer(std::shared_ptr<SESSION> new_player)
	{
		_players.emplace(new_player->_id, new_player);
		SendRoomInfoToNewPlayer(new_player);
	}
	void Room::LeavePlayer(long long player_id)
	{
		_players.erase(player_id);
	}

	void Room::AddNPC(std::unique_ptr<NPC> npc)
	{
		_npcs.emplace(npc->GetNpcId(), std::move(npc));
	}

	NPC* Room::GetNPC(int npc_id)
	{
		auto it = _npcs.find(npc_id);
		if (it == _npcs.end())
		{
			return nullptr;
		}
		return it->second.get();
	}

	void Room::StartGame()
	{
		_room_state = RoomState::PLAYING;
		MYLOG("Room " << _room_id << " is now in PLAYING state with " << GetPlayerCount() << " players.");

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
		for (auto& pair : _players)
		{
			if (pair.first == except_id) continue;
			pair.second->do_send(data, size);
		}
	}

	void Room::SendRoomInfoToNewPlayer(std::shared_ptr<SESSION> new_player)
	{
		// 1. 방에 이미 있던 다른 플레이어들의 정보를 새 플레이어에게 전송
		for (auto& pair : _players)
		{
			if (pair.first == new_player->_id) continue;

			auto& other_player_session = pair.second;
			packet::PacketStream spawn_packet = packet::MakeSpawnPlayerPacket(other_player_session);
			new_player->do_send(spawn_packet.constable_data(), spawn_packet.Size());
		}

		// 2. 방에 있는 모든 NPC들의 정보를 새 플레이어에게 전송
		for (auto& pair : _npcs)
		{
			NPC* npc = pair.second.get();
			common::packet::SC_PACKET_NPC_SPAWN spawnPacket;
			spawnPacket._size = sizeof(spawnPacket);
			spawnPacket._type = common::packet::PacketType::S2C_NPC_SPAWN;
			spawnPacket._npc_id = npc->GetNpcId();
			spawnPacket._npc_type = npc->GetNpcType();
			spawnPacket._position = npc->GetPosition();
			new_player->do_send(reinterpret_cast<const char*>(&spawnPacket), sizeof(spawnPacket));
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
			int target_x = static_cast<int>(attacker->GetPlayer()->_position.x + dx[i]);
			int target_y = static_cast<int>(attacker->GetPlayer()->_position.y + dy[i]);

			// 맵 경계 체크는 핸들러에서 이미 했을 수 있지만, 여기서도 한번 더 하는 것이 안전합니다.
			// (지금은 생략)

			// 방 내부의 플레이어 목록(_players)을 순회하며 공격 대상을 찾습니다.
			std::shared_ptr<SESSION> target_session = nullptr;
			for (auto const& [player_id, session] : _players)
			{
				if (session && session->_id != attacker->_id &&
					session->GetPlayer()->_position.x == target_x && session->GetPlayer()->_position.y == target_y)
				{
					target_session = session;
					break;
				}
			}

			if (target_session)
			{
				// 데미지 계산 (임시로 10)
				int16_t damage = 10;
				target_session->GetPlayer()->_hp -= damage;
				int32_t new_hp = target_session->GetPlayer()->_hp;
				if (new_hp < 0) { new_hp = 0; }

				MYLOG("[ROOM ATTACK] " << attacker->_id << " attacks " << target_session->_id << ". HP: " << new_hp);

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

