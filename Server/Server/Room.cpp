#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "PacketHandlers.h"

namespace PIP::server
{
	constexpr int MAX_ROOM_PLAYERS = 4; // 최대 플레이어 수

	std::random_device Room::_rd {};
	std::mt19937 Room::_gen{ _rd() };
	std::uniform_real_distribution<> Room::_npcURD{ -1.0, 1.0 };
	Room::Room(int room_id, int logic_thread_idx)
		: _room_id{ room_id }, _logic_thread_idx{ logic_thread_idx }, _max_players{ MAX_ROOM_PLAYERS }, _room_state{ RoomState::WAITING }
	{
		MYLOG("Room " << _room_id << " created. Assigned to Logic Thread " << _logic_thread_idx << " Max Players: " << static_cast<int>(_max_players));
	}

	void Room::Initialize()
	{
		for (int i = 0; i < 100; ++i)
		{
			// NPC ID는 플레이어와 겹치지 않도록 높은 수에서 시작 (AIManager에서 관리)
			int npcId = _next_npc_id++;
			common::Vec3 randomPos = {
				static_cast<float>(rand() % 200 - 100), 70.0f, static_cast<float>(rand() % 200 - 100)
			};

			auto npc = std::make_unique<NPC>(npcId, 1, _room_id, randomPos);
			AddNPC(std::move(npc));

			// 생성된 NPC의 AI를 1초 뒤에 처음으로 실행하도록 타이머에 등록
			Server::Instance()->AddTimerJob(_logic_thread_idx, std::chrono::milliseconds(200), [this, npcId]()
			{
				UpdateNPC(npcId);
			});
		}
	}

	void Room::EnterPlayer(std::shared_ptr<SESSION> new_player)
	{
		_players.emplace(new_player->_id, new_player);
		new_player->_logic_thread_idx = _logic_thread_idx;
	}
	void Room::LeavePlayer(long long player_id)
	{
		// 다른 클라이언트에게 퇴장 사실을 알림
		packet::SC_PACKET_LEAVE leave_packet;
		leave_packet._type = packet::PacketType::S2C_P_LEAVE;
		leave_packet._size = sizeof(leave_packet);
		leave_packet._id = player_id;
		this->Broadcast(reinterpret_cast<const char*>(&leave_packet), sizeof(leave_packet), player_id);
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
		for (auto& val : _npcs | std::views::values)
		{
			NPC* npc = val.get();
			const std::string& npc_name = npc->GetName();

			packet::SC_PACKET_NPC_SPAWN spawn_packet_data;
			spawn_packet_data._type = common::packet::PacketType::S2C_NPC_SPAWN;
			spawn_packet_data._size = 0; // 임시 크기, 나중에 덮어씀
			spawn_packet_data._npc_id = npc->GetNpcId();
			spawn_packet_data._npc_type = npc->GetNpcType();
			spawn_packet_data._position = npc->GetPosition();

			packet::PacketStream finalStream;
			finalStream << spawn_packet_data; // 1. 구조체를 스트림에 쓴다
			finalStream << npc_name;          // 2. 이름(가변 데이터)을 스트림에 쓴다

			// 3. 최종 크기를 계산하여 패킷 헤더에 덮어쓴다
			auto* final_header = reinterpret_cast<packet::PacketHeader*>(finalStream.mutable_data());
			final_header->_size = static_cast<uint16_t>(finalStream.Size());

			new_player->do_send(finalStream.constable_data(), finalStream.Size());
		}
	}

	void Room::HandleAttack(std::shared_ptr<SESSION> attacker)
	{
		//TODO: float로 바뀐 게임 좌표에 맞게 수정 필요
		if (attacker == nullptr) return;

		
		BoundingSphere attackerSphere { attacker->GetPlayer()->_position, 5.0f};

		// 방 내부의 플레이어 목록(_players)을 순회하며 공격 대상을 찾습니다.
		std::shared_ptr<SESSION> target_session = nullptr;
		for (auto const& [player_id, player] : _players)
		{
			BoundingSphere targetSphere{ player->GetPlayer()->_position, 5.0f };
			if (player && player->_id != attacker->_id && attackerSphere.Intersects(targetSphere))
			{
				player->GetPlayer()->_hp -= 10;
			}
		}

		for (auto& [npc_id, npc] : _npcs)
		{
			BoundingSphere npcSphere { npc->GetPosition(), 5.0f};
			if (attackerSphere.Intersects(npcSphere))
			{
				MYLOG("[ROOM ATTACK] " << attacker->_id << " attacks NPC " << npc->GetNpcId());
				npc->SetHP(npc->GetHP() - 10.0f);
			}
		}
		//// 데미지 계산 (임시로 10)
		//int16_t damage = 10;
		//target_session->GetPlayer()->_hp -= damage;
		//int32_t new_hp = target_session->GetPlayer()->_hp;
		//if (new_hp < 0) { new_hp = 0; }

		//MYLOG("[ROOM ATTACK] " << attacker->_id << " attacks " << target_session->_id << ". HP: " << new_hp);

		//// 공격 결과 패킷 생성
		//packet::SC_PACKET_ATTACK attackPacket;
		//attackPacket._type = packet::PacketType::S2C_P_ATTACK;
		//attackPacket._size = sizeof(attackPacket);
		//attackPacket._attacker_id = attacker->_id;
		//attackPacket._target_id = target_session->_id;
		//attackPacket._damage = damage;
		//attackPacket._target_current_hp = new_hp;

		//// 방 전체에 공격 결과 브로드캐스팅
		//Broadcast(reinterpret_cast<const char*>(&attackPacket), sizeof(attackPacket));
			
		
	}

	void Room::UpdateNPC(int npcId)
	{
		NPC* npc = GetNPC(npcId);
		if (!npc)
		{
			return;
		}
		// 랜덤이동
		common::Vec3 oldPos = npc->GetPosition();
		common::Vec3 newPos = oldPos;
		newPos.x += static_cast<float>(_npcURD(_gen)) * 10.0f;
		newPos.z += static_cast<float>(_npcURD(_gen)) * 10.0f;

		// TODO: 맵 경계나 벽 충돌 체크 로직 추가 필요
		npc->SetPosition(newPos);

		const std::string& npc_name = npc->GetName();

		packet::SC_PACKET_NPC_MOVE move_packet_data;
		move_packet_data._type = common::packet::PacketType::S2C_NPC_MOVE;
		move_packet_data._size = 0; // 임시
		move_packet_data._npc_id = npcId;
		move_packet_data._position = newPos;

		packet::PacketStream finalStream;
		finalStream << move_packet_data;
		finalStream << npc_name;

		// 최종 크기를 계산하여 패킷 헤더에 덮어쓰기
		auto* final_header = reinterpret_cast<packet::PacketHeader*>(finalStream.mutable_data());
		final_header->_size = static_cast<uint16_t>(finalStream.Size());

		Broadcast(finalStream.constable_data(), finalStream.Size());


		// 다음 업데이트 예약
		Server::Instance()->AddTimerJob(_logic_thread_idx,std::chrono::milliseconds(200),[this, npcId]()
		{
			this->UpdateNPC(npcId);
		});
	}
}

