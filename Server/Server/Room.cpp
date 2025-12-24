#include "pch.h"
#include "Room.h"
#include "LuaManager.h"
#include "MapDataManager.h"
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
		for (int i = 0; i < 10; ++i)
		{
			// NPC ID는 플레이어와 겹치지 않도록 높은 수에서 시작 (AIManager에서 관리)
			int npcId = _next_npc_id++;
			common::Vec3 randomPos = {
				static_cast<float>(rand() % 200 - 100), 70.0f, static_cast<float>(rand() % 200 - 100)
			};
			randomPos = MapDataManager::Instance()->AdjustPositionToGround(randomPos);
			auto npc = std::make_unique<NPC>(npcId, 1, _room_id, randomPos, 100);
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
			spawn_packet_data._hp = npc->GetHP();
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
		if (attacker == nullptr) return;

		// 이부분이 공격타입에 따라서 공격범위 같은게 바뀌는 곳일 것 같음
		BoundingSphere attackerSphere{ attacker->_player._position, 5.0f };
		const int32_t damage = attacker->_player._damage;


		std::vector<packet::NPCHitInfo> npc_hits;
		std::vector<packet::PlayerHitInfo> player_hits;

		// NPC 공격 판정
		for (auto& [npc_id, npc] : _npcs)
		{
			BoundingSphere npcSphere{ npc->GetPosition(), 2.0f };
			if (attackerSphere.Intersects(npcSphere))
			{
				int32_t new_hp = npc->GetHP() - damage;
				if (new_hp < 0) new_hp = 0;
				npc->SetHP(new_hp);

				MYLOG("[ROOM ATTACK] " << attacker->_id << " attacks NPC " << npc_id
						<< "new_hp: " << new_hp);

				npc_hits.emplace_back(npc_id, damage, new_hp);
			}
		}

		// 다른 플레이어 공격 판정
		for (auto const& [player_id, player_session] : _players)
		{
			if (player_session && player_id != attacker->_id)
			{
				BoundingSphere targetSphere{ player_session->_player._position, 2.0f };
				if (attackerSphere.Intersects(targetSphere))
				{
					int32_t new_hp = player_session->_player._hp - damage;
					if (new_hp < 0) new_hp = 0;
					player_session->_player._hp = new_hp;

					MYLOG("[ROOM ATTACK] Player:" << attacker->_id << " attacks Player:" << player_id << "'s HP: " << new_hp);
					player_hits.emplace_back(player_id, damage, new_hp);
				}
			}
		}

		// NPC 공격 결과 브로드캐스팅
		if (!npc_hits.empty())
		{
			packet::PacketStream stream;
			packet::SC_PACKET_NPC_ATTACK packet;
			packet._type = packet::PacketType::S2C_P_NPC_ATTACK;
			packet._attacker_id = attacker->_id;
			packet._hit_count = static_cast<uint8_t>(npc_hits.size());

			stream << packet;
			for (const auto& hit : npc_hits)
			{
				stream << hit;
			}

			auto* final_packet = reinterpret_cast<packet::PacketHeader*>(stream.mutable_data());
			final_packet->_size = static_cast<uint16_t>(stream.Size());

			Broadcast(stream.constable_data(), stream.Size());
		}

		// 플레이어 공격 결과 브로드캐스팅
		if (!player_hits.empty())
		{
			packet::PacketStream stream;
			packet::SC_PACKET_PLAYER_ATTACK header;
			header._type = packet::PacketType::S2C_P_PLAYER_ATTACK;
			header._attacker_id = attacker->_id;
			header._hit_count = static_cast<uint8_t>(player_hits.size());

			stream << header;
			for (const auto& hit : player_hits)
			{
				stream << hit;
			}

			auto* final_header = reinterpret_cast<packet::PacketHeader*>(stream.mutable_data());
			final_header->_size = static_cast<uint16_t>(stream.Size());

			Broadcast(stream.constable_data(), stream.Size());
		}
	}

	void Room::UpdateNPC(int npcId)
	{
		NPC* npc = GetNPC(npcId);
		if (not npc)
		{
			MYERROR("npc not found!!");
			return;
		}
		// 랜덤이동
		/*common::Vec3 oldPos = npc->GetPosition();
		common::Vec3 newPos = oldPos;
		newPos.x += static_cast<float>(_npcURD(_gen)) * 10.0f;
		newPos.z += static_cast<float>(_npcURD(_gen)) * 10.0f;*/

		common::Vec3 oldPos = npc->GetPosition();
		float deltaTime = 0.2f; // 200ms 마다 업데이트 되므로
		npc->UpdateAI(0.2f);
		common::Vec3 currPos = npc->GetPosition();

		common::Vec3 velocity;
		velocity.x = (currPos.x - oldPos.x) / deltaTime;
		velocity.y = (currPos.y - oldPos.y) / deltaTime;
		velocity.z = (currPos.z - oldPos.z) / deltaTime;

		if (velocity.x != 0 || velocity.z != 0) {
			// atan2 등을 이용해 Y축 회전각 계산 가능




			// npc->SetRotation(...);
			npc->SetVelocity(velocity);

		}

		auto newPos = npc->GetPosition();
		// TODO: 맵 경계나 벽 충돌 체크 로직 추가 필요
		newPos = MapDataManager::Instance()->AdjustPositionToGround(newPos);
		npc->SetPosition(newPos);

		const std::string& npc_name = npc->GetName();

		packet::SC_PACKET_NPC_MOVE move_packet_data;
		move_packet_data._type = common::packet::PacketType::S2C_NPC_MOVE;
		move_packet_data._size = 0; // 임시
		move_packet_data._npc_id = npcId;
		move_packet_data._position = newPos;
		move_packet_data._velocity = npc->GetVelocity();
		move_packet_data._rotation = npc->GetRotation();
		move_packet_data._time_stamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count());

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

