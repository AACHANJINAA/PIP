#include "pch.h"
#include "Room.h"
#include "LuaManager.h"
#include "MapDataManager.h"
#include "Player.h"
#include "PacketHandlers.h"
#include "Jolt/Physics/Collision/Shape/HeightFieldShape.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include <random>

namespace PIP::SERVER
{
	constexpr int MAX_ROOM_PLAYERS = 4;

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
		PhysicsInitialize();

		_gridMap.Initialize(-1000, 1000, -1000, 1000, 100);

		for (int i = 0; i < 100; ++i)
		{
			int npcId = _next_npc_id++;
			common::Vec3 randomPos = {
				static_cast<float>(rand() % 200 - 100), 70.0f, static_cast<float>(rand() % 200 - 100)
			};
			
			randomPos = MapDataManager::Instance()->AdjustPositionToGround(randomPos);
			randomPos.y += 5.0f;

			auto npc = std::make_unique<GAME::NPC>(npcId, 1, _room_id, randomPos, 100);

			auto controller = npc->GetComponent<GAME::CharacterControllerComponent>();
			if (controller)
			{
				controller->Initialize(_physicsSystem, 1.8f, 0.5f);
			}

			float randomOffset = (rand() % 200) / 1000.0f; 
			auto scatteredTime = std::chrono::steady_clock::now() - std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(randomOffset));
			npc->SetLastUpdateTime(scatteredTime);
			
			AddNPC(std::move(npc));
		}
	}

	void Room::EnterPlayer(std::shared_ptr<SESSION> new_player)
	{
		bool wasEmpty = _players.empty(); 
		if (new_player->_player) {
			_gridMap.Add(new_player->_player.get());
		}
		_players.emplace(new_player->_id, new_player);
		new_player->_logic_thread_idx = _logic_thread_idx;
		if (wasEmpty) {
			MYLOG("First player entered Room " << _room_id << ". Waking up NPCs...");
			for (auto& [id, npc] : _npcs) {
				float randomOffset = (rand() % 200) / 1000.0f; 
				auto scatteredTime = std::chrono::steady_clock::now() - std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(randomOffset));
				npc->SetLastUpdateTime(scatteredTime);
			}
		}
	}
	void Room::LeavePlayer(long long player_id)
	{
		packet::SC_PACKET_LEAVE leave_packet;
		leave_packet._type = packet::PacketType::S2C_P_LEAVE;
		leave_packet._size = sizeof(leave_packet);
		leave_packet._id = player_id;
		this->Broadcast(reinterpret_cast<const char*>(&leave_packet), sizeof(leave_packet), player_id);
		_players.erase(player_id);
	}

	void Room::AddNPC(std::unique_ptr<GAME::NPC> npc)
	{
		_gridMap.Add(npc.get());
		_npcs.emplace(npc->GetNpcId(), std::move(npc));
	}

	GAME::NPC* Room::GetNPC(int npc_id)
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
	}


	void Room::UpdatePhysics(float deltaTime, JPH::TempAllocator* tempAllocator)
	{
		if (!_physicsSystem || _players.empty()) return;
		
		_physicsSystem->Update(deltaTime, 1, tempAllocator, _jobSystem);

		for (auto& [id, npc] : _npcs)
		{
			npc->PhysicsUpdate(deltaTime, tempAllocator);
		}
	}

	void Room::UpdateLogics(float deltaTime, JPH::TempAllocator* tempAllocator)
	{
		ProcessJobs();
		
		if (_players.empty()) return;

		// --- 1. AOI 시야 갱신 (Enter/Leave) ---
		for (auto& [pid, session] : _players)
		{
			if (!session || !session->_player) continue;

			// 내 주변 객체들 찾기 (GridMap)
			std::vector<GAME::GameObject*> nearbyObjects;
			_gridMap.GetNearbyObjects(session->_player->GetPosition(), nearbyObjects);

			std::unordered_set<int> currentNearbyIds;

			// 1-1. 시야에 들어온 NPC 처리 (Enter)
			for (auto* obj : nearbyObjects)
			{
				if (auto npc = dynamic_cast<GAME::NPC*>(obj))
				{
					int npcId = npc->GetNpcId();
					currentNearbyIds.insert(npcId);

					// 새로 발견된 NPC라면?
					if (!session->_viewedNpcs.contains(npcId))
					{
						session->_viewedNpcs.insert(npcId);
						SendNpcSpawnToPlayer(session, npc);
					}
				}
			}

			// 1-2. 시야에서 사라진 NPC 처리 (Leave)
			for (auto it = session->_viewedNpcs.begin(); it != session->_viewedNpcs.end(); )
			{
				if (!currentNearbyIds.contains(*it))
				{
					// 더 이상 주변에 없으므로 삭제 패킷 전송
					SendNpcLeaveToPlayer(session, *it);
					it = session->_viewedNpcs.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

		// --- 2. NPC AI 및 물리 업데이트 ---
		_npcSyncTimer += deltaTime;
		bool shouldSync = false;
		if (_npcSyncTimer >= 0.05f) { shouldSync = true; _npcSyncTimer = 0.0f; }

		auto now = std::chrono::steady_clock::now();
		const float HEARTBEAT_INTERVAL = 0.2f;

		for (auto& [id, npc] : _npcs)
		{
			std::chrono::duration<float> elapsed = now - npc->GetLastUpdateTime();
			if (elapsed.count() >= HEARTBEAT_INTERVAL)
			{
				npc->Update(elapsed.count(), tempAllocator);
				npc->SetLastUpdateTime(now);
				auto new_pos = npc->GetPosition();
				
			}
			_gridMap.UpdatePosition(npc.get(), npc->GetPosition());
			// 회전 처리
			common::Vec3 vel = npc->GetVelocity();
			if (vel.x * vel.x + vel.z * vel.z > 0.001f) {
				float angle = std::atan2(vel.x, vel.z);
				DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0, angle, 0);
				common::Vec4 rot;
				XMStoreFloat4((XMFLOAT4*)&rot, q);
				npc->SetRotation(rot);
			}
		}

		if (shouldSync) BroadcastNpcBatch();
	}

	void Room::PushJob(std::function<void()> job)
	{
		_jobQueue.push(std::move(job));
	}
	void Room::ProcessJobs()
	{
		std::function<void()> job;
		while (_jobQueue.try_pop(job))
		{
			if (job) job();
		}
	}

	void Room::SendNpcMovePacket(GAME::NPC* npc)
	{
		if (!npc) return;

		packet::SC_PACKET_NPC_MOVE move_packet_data;
		move_packet_data._type = common::packet::PacketType::S2C_NPC_MOVE;
		move_packet_data._npc_id = npc->GetNpcId();
		move_packet_data._position = npc->GetPosition();
		move_packet_data._velocity = npc->GetVelocity();
		move_packet_data._rotation = npc->GetRotation();
		move_packet_data._state = common::packet::OBJECT_STATE::WALK; 
		move_packet_data._time_stamp = static_cast<uint32_t>(GetTickCount64());

		packet::PacketStream finalStream;
		finalStream << move_packet_data;
		finalStream << npc->GetName();

		auto* final_header = reinterpret_cast<packet::PacketHeader*>(finalStream.mutable_data());
		final_header->_size = static_cast<uint16_t>(finalStream.Size());

		Broadcast(finalStream.constable_data(), finalStream.Size());
	}
	void Room::Broadcast(const char* data, size_t size, long long except_id)
	{
		for (auto& pair : _players)
		{
			if (pair.first == except_id) continue;
			pair.second->do_send(data, size);
		}
	}

	void Room::BroadcastNpcBatch()
	{
		if (_npcs.empty() || _players.empty()) return;

		// 1. 움직인 NPC 수집
		std::vector<GAME::NPC*> dirtyNPCs;
		for (auto& [id, npc] : _npcs) {
			if (npc->IsDirty()) dirtyNPCs.push_back(npc.get());
		}
		if (dirtyNPCs.empty()) return;

		// 2. 플레이어별 전송
		for (auto& [pid, session] : _players)
		{
			if (!session || !session->_player) continue;

			packet::PacketStream stream;
			packet::SC_PACKET_NPC_MOVE_BATCH header;
			header._type = packet::PacketType::S2C_NPC_MOVE_BATCH;
			header._count = 0;
			stream << header;

			int count = 0;
			for (auto* npc : dirtyNPCs)
			{
				// [핵심] 시야 리스트(View List)에 있는 놈만 보낸다!
				if (!session->_viewedNpcs.contains(npc->GetNpcId()))
					continue;

				packet::NPCMoveData data;
				data._npc_id = npc->GetNpcId();
				data._position = npc->GetPosition();
				data._velocity = npc->GetVelocity();
				data._rotation = npc->GetRotation();
				data._state = common::packet::OBJECT_STATE::WALK;
				data._time_stamp = static_cast<uint32_t>(GetTickCount64());

				stream << data;
				count++;

				if (stream.Size() > 3800) {
					auto* h = reinterpret_cast<packet::SC_PACKET_NPC_MOVE_BATCH*>(stream.mutable_data());
					h->_count = count;
					h->_size = (uint16_t)stream.Size();
					session->do_send(stream.constable_data(), stream.Size());
					stream.Clear();
					stream << header;
					count = 0;
				}
			}

			if (count > 0) {
				auto* h = reinterpret_cast<packet::SC_PACKET_NPC_MOVE_BATCH*>(stream.mutable_data());
				h->_count = count;
				h->_size = (uint16_t)stream.Size();
				session->do_send(stream.constable_data(), stream.Size());
			}
		}

		// 3. 클린업
		for (auto* npc : dirtyNPCs) npc->SyncSentData();
	}

	void Room::SendRoomInfoToNewPlayer(std::shared_ptr<SESSION> new_player) {
		// 다른 플레이어 정보만 보냄
		for (auto& pair : _players)
		{
			if (pair.first == new_player->_id) continue;
			auto& other_session = pair.second;
			packet::PacketStream spawn_packet = packet::MakeSpawnPlayerPacket(other_session);
			new_player->do_send(spawn_packet.constable_data(), spawn_packet.Size());
		}

	}

	void Room::SendNpcSpawnToPlayer(const std::shared_ptr<SESSION>& session, const GAME::NPC* npc)
	{
		packet::SC_PACKET_NPC_SPAWN spawn_packet_data;
		spawn_packet_data._type = common::packet::PacketType::S2C_NPC_SPAWN;
		spawn_packet_data._size = 0;
		spawn_packet_data._hp = npc->GetHP();
		spawn_packet_data._npc_id = npc->GetNpcId();
		spawn_packet_data._npc_type = npc->GetNpcType();
		spawn_packet_data._position = npc->GetPosition();
		const std::string& npc_name = npc->GetName();

		packet::PacketStream finalStream;
		finalStream << spawn_packet_data;
		finalStream << npc_name;
		auto* final_header = reinterpret_cast<packet::PacketHeader*>(finalStream.mutable_data());
		final_header->_size = static_cast<uint16_t>(finalStream.Size());

		session->do_send(finalStream.constable_data(), finalStream.Size());
	}
	void Room::SendNpcLeaveToPlayer(const std::shared_ptr<SESSION>& session, int npcId)
	{
		packet::SC_PACKET_NPC_DESPAWN despawn_packet;
		despawn_packet._type = common::packet::PacketType::S2C_NPC_DESPAWN;
		despawn_packet._size = sizeof(despawn_packet);
		despawn_packet._npc_id = npcId;
		session->do_send(reinterpret_cast<const char*>(&despawn_packet), sizeof(despawn_packet));
	}

	void Room::HandleAttack(const std::shared_ptr<SESSION>& attacker) {
		if (attacker == nullptr) return;

		BoundingSphere attackerSphere{ attacker->_player->GetPosition(), 5.0f };
		const int32_t damage = attacker->_player->_damage;

		std::vector<packet::NPCHitInfo> npc_hits;
		std::vector<packet::PlayerHitInfo> player_hits;

		for (auto& [npc_id, npc] : _npcs)
		{
			BoundingSphere npcSphere{ npc->GetPosition(), 2.0f };
			if (attackerSphere.Intersects(npcSphere))
			{
				int32_t new_hp = npc->GetHP() - damage;
				if (new_hp < 0) new_hp = 0;
				npc->SetHP(new_hp);

				npc_hits.emplace_back(npc_id, damage, new_hp);
			}
		}

		for (auto const& [player_id, player_session] : _players)
		{
			if (player_session && player_id != attacker->_id)
			{
				BoundingSphere targetSphere{ player_session->_player->GetPosition(), 2.0f };
				if (attackerSphere.Intersects(targetSphere))
				{
					int32_t new_hp = player_session->_player->_hp - damage;
					if (new_hp < 0) new_hp = 0;
					player_session->_player->_hp = new_hp;

					player_hits.emplace_back(player_id, damage, new_hp);
				}
			}
		}

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
	void Room::Execute_C2S_MOVE(std::shared_ptr<SESSION> session, const common::packet::CS_PACKET_MOVE& move_packet) {
		if (!session || session->_state != SERVER::SESSION_STATE::ST_INGAME) return;

		common::Vec3 targetPos = move_packet._position;
		common::Quat targetRotation = move_packet._rotation;
		common::packet::OBJECT_STATE targetState = move_packet._state;
		common::Vec3 player_extents = { 0.5f, 0.9f, 0.5f };

		if (!MapDataManager::Instance()->IsInsideMap(targetPos.x, targetPos.z))
		{
			packet::SC_PACKET_MOVE correction_packet;
			correction_packet._type = common::packet::PacketType::S2C_P_MOVE;
			correction_packet._size = sizeof(correction_packet);
			correction_packet._id = session->_id;
			correction_packet._position = session->_player->GetPosition();
			correction_packet._rotation = session->_player->GetRotation();
			correction_packet._state = common::packet::OBJECT_STATE::IDLE;

			session->do_send(reinterpret_cast<char*>(&correction_packet), sizeof(correction_packet));
			return;
		}

		float groundHeight = MapDataManager::Instance()->GetGroundHeight(targetPos.x, targetPos.z);
		targetPos.y = groundHeight;

		if (MapDataManager::Instance()->CheckForCollision(targetPos, player_extents))
		{
			packet::SC_PACKET_MOVE correction_packet;
			correction_packet._type = common::packet::PacketType::S2C_P_MOVE;
			correction_packet._size = sizeof(correction_packet);
			correction_packet._id = session->_id;
			correction_packet._position = session->_player->GetPosition(); 
			correction_packet._rotation = session->_player->GetRotation();
			correction_packet._state = common::packet::OBJECT_STATE::IDLE;

			session->do_send(reinterpret_cast<char*>(&correction_packet), sizeof(correction_packet));
		}
		else
		{
			common::Vec3 oldPos = session->_player->GetPosition();

			session->_player->SetPosition(targetPos);
			session->_player->SetRotation(targetRotation);
			session->_player->_state = targetState;

			packet::SC_PACKET_MOVE sync_packet;
			sync_packet._type = common::packet::PacketType::S2C_P_MOVE;
			sync_packet._size = sizeof(sync_packet);
			sync_packet._id = session->_id;
			sync_packet._position = targetPos;
			sync_packet._rotation = targetRotation;
			sync_packet._state = targetState;

			_gridMap.UpdatePosition(session->_player.get(), targetPos);
			
			Broadcast(reinterpret_cast<char*>(&sync_packet), sizeof(sync_packet), session->_id);
		}
	}
	void Room::Execute_C2S_ROOM_ENTER(std::shared_ptr<SESSION> session, const common::packet::CS_PACKET_ENTER_ROOM& enter_packet) {

		if (session->_room_id != -1)
		{
			SERVER::Room* old_room = SERVER::Server::Instance()->GetRoom(session->_room_id);
			if (old_room)
			{
				packet::SC_PACKET_LEAVE leave_packet;
				leave_packet._type = packet::PacketType::S2C_P_LEAVE;
				leave_packet._size = sizeof(leave_packet);
				leave_packet._id = session->_id;
				old_room->Broadcast(reinterpret_cast<const char*>(&leave_packet), sizeof(leave_packet), session->_id);

				old_room->LeavePlayer(session->_id);
			}
		}

		session->_room_id = enter_packet._room_id;
		session->_state = SERVER::SESSION_STATE::ST_INGAME;
		session->_logic_thread_idx = GetLogicThreadIndex();
		common::Vec3 spawnPos{ 0, 10, 10 };
		session->_player->SetPosition(MapDataManager::Instance()->AdjustPositionToGround(spawnPos));
		session->_player->_level = 1;
		session->_player->_hp = 100;
		session->_player->_exp = 0;


		MYLOG("[EnterRoom] Session " << session->_id << " updated. New Room: " << session->_room_id << ", Pos: (0, 0, -150)");

		packet::SC_PACKET_ENTER_ROOM_ACK ack_packet;
		ack_packet._type = packet::PacketType::S2C_P_ENTER_ROOM_ACK;
		ack_packet._size = sizeof(ack_packet);
		ack_packet._room_id = enter_packet._room_id;
		ack_packet._success = true;
		packet::PacketStream ack_stream;
		ack_stream << ack_packet;
		session->do_send(ack_stream.constable_data(), ack_stream.Size());
		MYLOG("[EnterRoom] Sent ENTER_ROOM_ACK(success) to session " << session->_id);

		SendRoomInfoToNewPlayer(session);

		packet::PacketStream self_spawn_stream = packet::MakeSpawnPlayerPacket(session);
		session->do_send(self_spawn_stream.mutable_data(), self_spawn_stream.Size());

		Broadcast(self_spawn_stream.constable_data(), self_spawn_stream.Size(), session->_id);
		MYLOG("[EnterRoom] Broadcasted SPAWN_PLAYER of new session " << session->_id << " to other players in room " << GetRoomId());

		EnterPlayer(session);
	}
	void Room::CreatePhysicsTerrain() {
		const auto& terrainData = MapDataManager::Instance()->GetTerrainData();
		const auto& info = terrainData.GetInfo();
		const auto& heightMap = terrainData.GetHeightData();

		JPH::HeightFieldShapeSettings settings;
		settings.mOffset = JPH::Vec3(info.min_x, 0.0f, info.min_z);

		float dx = (info.max_x - info.min_x) / (info.width - 1);
		float dz = (info.max_z - info.min_z) / (info.height - 1);
		settings.mScale = JPH::Vec3(dx, 1.0f, dz);
		settings.mSampleCount = static_cast<JPH::uint32>(info.width);

		settings.mHeightSamples.resize(heightMap.size());
		for (size_t i = 0; i < heightMap.size(); ++i) {
			settings.mHeightSamples[i] = heightMap[i];
		}

		auto result = settings.Create();
		if (result.HasError()) return;

		JPH::BodyCreationSettings bodySettings(result.Get(), JPH::RVec3(0, 0, 0), JPH::Quat::sIdentity(),
			JPH::EMotionType::Static, Layers::NON_MOVING);

		JPH::BodyInterface& bodyInterface = _physicsSystem->GetBodyInterface();
		JPH::Body* terrainBody = bodyInterface.CreateBody(bodySettings);

		_terrainBodyID = terrainBody->GetID();
		bodyInterface.AddBody(_terrainBodyID, JPH::EActivation::DontActivate);

		JPH::RRayCast ray;
		ray.mOrigin = JPH::Vec3(0, 100, 0); 
		ray.mDirection = JPH::Vec3(0, -200, 0); 

		JPH::RayCastResult ray_result;
		if (_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, ray_result)) {
			float hitY = ray.mOrigin.GetY() + ray.mDirection.GetY() * ray_result.mFraction;
			MYLOG("Physics Terrain Test Success! Height at (0,0): " << hitY);
		}
		else {
			MYERROR("Physics Terrain NOT FOUND! Ray missed.");
		}
	}
	void Room::PhysicsInitialize() {
		_jobSystem = new JPH::JobSystemSingleThreaded(JPH::cMaxPhysicsJobs);

		_physicsSystem = new JPH::PhysicsSystem();

		const JPH::uint cMaxBodies = 1024;
		const JPH::uint cNumBodyMutexes = 0;
		const JPH::uint cMaxBodyPairs = 1024;
		const JPH::uint cMaxContactConstraints = 1024;

		_physicsSystem->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
			_bpLayerInterface, _objVsBpLayerFilter, _objLayerPairFilter);

		_physicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

		CreatePhysicsTerrain();
	}
}
