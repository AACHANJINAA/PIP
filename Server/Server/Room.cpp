#include "pch.h"
#include "Room.h"
#include "LuaManager.h"
#include "MapDataManager.h"
#include "Player.h"
#include "PacketHandlers.h"
#include "Jolt/Physics/Collision/Shape/HeightFieldShape.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include <random>

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
		PhysicsInitialize();

		for (int i = 0; i < 100; ++i)
		{
			int npcId = _next_npc_id++;
			common::Vec3 randomPos = {
				static_cast<float>(rand() % 200 - 100), 70.0f, static_cast<float>(rand() % 200 - 100)
			};
			randomPos = MapDataManager::Instance()->AdjustPositionToGround(randomPos);
			auto npc = std::make_unique<NPC>(npcId, 1, _room_id, randomPos, 100);
			AddNPC(std::move(npc));

			//// 생성된 NPC의 AI를 1초 뒤에 처음으로 실행하도록 타이머에 등록
			//Server::Instance()->AddTimerJob(_logic_thread_idx, std::chrono::milliseconds(200), [this, npcId]()
			//{
			//	UpdateNPC(npcId);
			//});
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

	void Room::Update(float deltaTime)
	{
		ProcessJobs();		// 1. 플레이어 이동 패킷 등 네트워크 명령을 먼저 다 처리
		UpdatePhysics();	// 2. 바뀐 위치를 바탕으로 물리 충돌 해결
		UpdateAI(deltaTime);// 3. NPC AI 업데이트
	}

	void Room::PushJob(std::function<void()> job)
	{
		_jobQueue.push(std::move(job));
	}
	void Room::ProcessJobs()
	{
		std::function<void()> job;
		// 큐가 빌 때까지 모든 명령을 현재 프레임에서 소화
		while (_jobQueue.try_pop(job))
		{
			if (job) job();
		}
	}
	void Room::UpdatePhysics()
	{
		if (!_physicsSystem) return;

		// 고정 스텝 60fps (16.6ms) 업데이트
		_physicsSystem->Update(1.0f / 60.0f, 1, _tempAllocator, _jobSystem);
	}

	

	void Room::UpdateAI(float deltaTime)
	{
		// 기존에 타이머로 돌던 NPC 업데이트를 여기서 일괄 처리
		for (auto& [id, npc] : _npcs)
		{
			common::Vec3 oldPos = npc->GetPosition();
			npc->UpdateAI(deltaTime);
			common::Vec3 currPos = npc->GetPosition();

			// 속도 계산
			common::Vec3 velocity = {
				(currPos.x - oldPos.x) / deltaTime,
				(currPos.y - oldPos.y) / deltaTime,
				(currPos.z - oldPos.z) / deltaTime
			};
			npc->SetVelocity(velocity);

			// 지형 보정 (기존 로직 유지)
			common::Vec3 newPos = MapDataManager::Instance()->AdjustPositionToGround(npc->GetPosition());
			npc->SetPosition(newPos);

			// [중요] 여기서 바뀐 위치를 체크하고 바로 Broadcast 하거나,
			// 바뀐 리스트만 모았다가 한 번에 보낼 수 있습니다.
			// 플레이어 시야 반경 안에 있는 npc만 보내도록, 맵 공간을 구획화하고 
			SendNpcMovePacket(npc.get());
		}
	}
	void Room::SendNpcMovePacket(NPC* npc)
	{
		if (!npc) return;

		packet::SC_PACKET_NPC_MOVE move_packet_data;
		move_packet_data._type = common::packet::PacketType::S2C_NPC_MOVE;
		move_packet_data._npc_id = npc->GetNpcId();
		move_packet_data._position = npc->GetPosition();
		move_packet_data._velocity = npc->GetVelocity();
		move_packet_data._rotation = npc->GetRotation();
		move_packet_data._state = common::packet::OBJECT_STATE::WALK; // 일단 임시
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

	void Room::Execute_C2S_MOVE(std::shared_ptr<SESSION> session, const common::packet::CS_PACKET_MOVE& move_packet)
	{
		// 1. 유효성 검사
		if (!session || session->_state != server::SESSION_STATE::ST_INGAME) return;

		common::Vec3 targetPos = move_packet._position;
		common::Quat targetRotation = move_packet._rotation;
		common::packet::OBJECT_STATE targetState = move_packet._state;
		common::Vec3 player_extents = { 0.5f, 0.9f, 0.5f };

		// 2. 맵 범위 체크
		if (!MapDataManager::Instance()->IsInsideMap(targetPos.x, targetPos.z))
		{
			// 맵 밖으로 나가려는 시도 - 보정 패킷 전송
			packet::SC_PACKET_MOVE correction_packet;
			correction_packet._type = common::packet::PacketType::S2C_P_MOVE;
			correction_packet._size = sizeof(correction_packet);
			correction_packet._id = session->_id;
			correction_packet._position = session->_player._position; // 이전 안전한 위치
			correction_packet._rotation = session->_player._rotation;
			correction_packet._state = common::packet::OBJECT_STATE::IDLE;

			session->do_send(reinterpret_cast<char*>(&correction_packet), sizeof(correction_packet));
			return;
		}

		// 3. 지형 높이(Y) 보정 (클라이언트와 동일하게)
		float groundHeight = MapDataManager::Instance()->GetGroundHeight(targetPos.x, targetPos.z);
		targetPos.y = groundHeight;

		// 4. 충돌 체크 (MapObject AABB)
		if (MapDataManager::Instance()->CheckForCollision(targetPos, player_extents))
		{
			// [충돌!] 이전 위치로 롤백 패킷 전송
			packet::SC_PACKET_MOVE correction_packet;
			correction_packet._type = common::packet::PacketType::S2C_P_MOVE;
			correction_packet._size = sizeof(correction_packet);
			correction_packet._id = session->_id;
			correction_packet._position = session->_player._position; // 직전 위치
			correction_packet._rotation = session->_player._rotation;
			correction_packet._state = common::packet::OBJECT_STATE::IDLE;

			session->do_send(reinterpret_cast<char*>(&correction_packet), sizeof(correction_packet));
		}
		else
		{
			// [성공!] 서버 메모리 갱신 및 브로드캐스팅
			session->_player._position = targetPos;
			session->_player._rotation = targetRotation;
			session->_player._state = targetState;

			packet::SC_PACKET_MOVE sync_packet;
			sync_packet._type = common::packet::PacketType::S2C_P_MOVE;
			sync_packet._size = sizeof(sync_packet);
			sync_packet._id = session->_id;
			sync_packet._position = targetPos;
			sync_packet._rotation = targetRotation;
			sync_packet._state = targetState;

			// 나를 제외한 방 안의 모든 유저에게 전송
			Broadcast(reinterpret_cast<char*>(&sync_packet), sizeof(sync_packet), session->_id);
		}

		// 5. Jolt 물리 바디 동기화 (나중에 Jolt 바디 생성 후 활성화)
		/*
		if (!_physicsSystem) return;
		JPH::BodyInterface& bodyInterface = _physicsSystem->GetBodyInterface();
		if (!session->_player._physicsBodyID.IsInvalid()) {
			bodyInterface.SetPosition(session->_player._physicsBodyID,
				JPH::RVec3(targetPos.x, targetPos.y, targetPos.z), JPH::EActivation::Activate);
		}
		*/
	}

	void Room::Execute_C2S_ROOM_ENTER(std::shared_ptr<SESSION> session,
		const common::packet::CS_PACKET_ENTER_ROOM& enter_packet)
	{

		if (session->_room_id != -1)
		{
			server::Room* old_room = server::Server::Instance()->GetRoom(session->_room_id);
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
		session->_state = server::SESSION_STATE::ST_INGAME;
		session->_logic_thread_idx = GetLogicThreadIndex();
		common::Vec3 spawnPos{ 0, 10, 10 };
		session->_player._position = MapDataManager::Instance()->AdjustPositionToGround(spawnPos);
		session->_player._level = 1;
		session->_player._hp = 100;
		session->_player._exp = 0;


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

	void Room::CreatePhysicsTerrain()
	{
		// MapDataManager에서 로드된 지형 정보 가져오기
		const auto& terrainData = MapDataManager::Instance()->GetTerrainData();
		const auto& info = terrainData.GetInfo();
		const auto& heightMap = terrainData.GetHeightData();

		// Jolt HeightFieldShape 생성
		JPH::HeightFieldShapeSettings settings;
		settings.mOffset = JPH::Vec3(info.min_x, 0.0f, info.min_z);

		// 간격 계산 (Scale)
		float dx = (info.max_x - info.min_x) / (info.width - 1);
		float dz = (info.max_z - info.min_z) / (info.height - 1);
		settings.mScale = JPH::Vec3(dx, 1.0f, dz);
		settings.mSampleCount = info.width; // 정사각형 가정

		// 데이터 복사 및 변환 (float -> Jolt 포맷)
		settings.mHeightSamples.resize(heightMap.size());
		for (size_t i = 0; i < heightMap.size(); ++i) {
			settings.mHeightSamples[i] = heightMap[i];
		}

		auto result = settings.Create();
		if (result.HasError()) return;

		// Body 생성 (Static: 움직이지 않음)
		JPH::BodyCreationSettings bodySettings(result.Get(), JPH::RVec3(0, 0, 0), JPH::Quat::sIdentity(),
			JPH::EMotionType::Static, Layers::NON_MOVING);

		JPH::BodyInterface& bodyInterface = _physicsSystem->GetBodyInterface();
		JPH::Body* terrainBody = bodyInterface.CreateBody(bodySettings);

		_terrainBodyID = terrainBody->GetID();
		bodyInterface.AddBody(_terrainBodyID, JPH::EActivation::DontActivate);

		// 지형 생성 확인을 위한 간단한 레이캐스트 테스트
		JPH::RRayCast ray;
		ray.mOrigin = JPH::Vec3(0, 100, 0); // 맵 중앙 하늘 위
		ray.mDirection = JPH::Vec3(0, -200, 0); // 바닥 방향으로 쏘기

		JPH::RayCastResult ray_result;
		if (_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, ray_result)) {
			// 성공! 무언가에 부딪혔음.
			float hitY = ray.mOrigin.GetY() + ray.mDirection.GetY() * ray_result.mFraction;
			MYLOG("Physics Terrain Test Success! Height at (0,0): " << hitY);
		}
		else {
			MYERROR("Physics Terrain NOT FOUND! Ray missed.");
		}
	}

	void Room::PhysicsInitialize()
	{
		// 1. 물리 시스템 필수 객체 생성
		// TempAllocator: 물리 연산 중 임시 메모리 할당 (10MB 정도)
		_tempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

		// JobSystem: 아까 논의한 대로 '단일 스레드' 모드 사용
		_jobSystem = new JPH::JobSystemSingleThreaded(JPH::cMaxPhysicsJobs);

		// 2. PhysicsSystem 생성 및 초기화
		_physicsSystem = new JPH::PhysicsSystem();

		const JPH::uint cMaxBodies = 1024;
		const JPH::uint cNumBodyMutexes = 0;
		const JPH::uint cMaxBodyPairs = 1024;
		const JPH::uint cMaxContactConstraints = 1024;

		_physicsSystem->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
			_bpLayerInterface, _objVsBpLayerFilter, _objLayerPairFilter);

		// 3. 중력 설정 (Y축 아래 방향)
		_physicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

		// 4. 지형 생성 호출 (예시)
		CreatePhysicsTerrain();
	}

	//void Room::UpdateNPC(int npcId)
	//{
	//	NPC* npc = GetNPC(npcId);
	//	if (not npc)
	//	{
	//		MYERROR("npc not found!!");
	//		return;
	//	}
	//	// 랜덤이동
	//	/*common::Vec3 oldPos = npc->GetPosition();
	//	common::Vec3 newPos = oldPos;
	//	newPos.x += static_cast<float>(_npcURD(_gen)) * 10.0f;
	//	newPos.z += static_cast<float>(_npcURD(_gen)) * 10.0f;*/
	//
	//	common::Vec3 oldPos = npc->GetPosition();
	//	float deltaTime = 0.2f; // 200ms 마다 업데이트 되므로
	//	npc->UpdateAI(0.2f);
	//	common::Vec3 currPos = npc->GetPosition();

	//	common::Vec3 velocity;
	//	velocity.x = (currPos.x - oldPos.x) / deltaTime;
	//	velocity.y = (currPos.y - oldPos.y) / deltaTime;
	//	velocity.z = (currPos.z - oldPos.z) / deltaTime;
	//	npc->SetVelocity(velocity);

	//	common::Quat rotation = { 0,0,0,1 };
	//	if (velocity.x != 0 || velocity.z != 0) {
	//		// atan2 등을 이용해 Y축 회전각 계산 가능
	//		float angle_rad = std::atan2(velocity.x, velocity.z); // Z축이 앞쪽인 경우
	//		DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0.0f, angle_rad, 0.0f);
	//		XMStoreFloat4(&rotation, q);

	//	}
	//	npc->SetRotation(rotation);

	//	auto newPos = npc->GetPosition();
	//	// TODO: 맵 경계나 벽 충돌 체크 로직 추가 필요
	//	newPos = MapDataManager::Instance()->AdjustPositionToGround(newPos);
	//	npc->SetPosition(newPos);

	//	const std::string& npc_name = npc->GetName();

	//	packet::SC_PACKET_NPC_MOVE move_packet_data;
	//	move_packet_data._type = common::packet::PacketType::S2C_NPC_MOVE;
	//	move_packet_data._size = 0; // 임시
	//	move_packet_data._npc_id = npcId;
	//	move_packet_data._position = newPos;
	//	move_packet_data._velocity = npc->GetVelocity();
	//	move_packet_data._rotation = npc->GetRotation();
	//	move_packet_data._time_stamp = static_cast<uint32_t>(GetTickCount64());

	//	packet::PacketStream finalStream;
	//	finalStream << move_packet_data;
	//	finalStream << npc_name;

	//	// 최종 크기를 계산하여 패킷 헤더에 덮어쓰기 <중요>
	//	auto* final_header = reinterpret_cast<packet::PacketHeader*>(finalStream.mutable_data());
	//	final_header->_size = static_cast<uint16_t>(finalStream.Size());

	//	Broadcast(finalStream.constable_data(), finalStream.Size());


	//	//// 다음 업데이트 예약
	//	//Server::Instance()->AddTimerJob(_logic_thread_idx,std::chrono::milliseconds(200),[this, npcId]()
	//	//{
	//	//	this->UpdateNPC(npcId);
	//	//});
	//}
}

