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
			int64_t npcId = _next_npc_id++;
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
			auto scatteredTime = std::chrono::steady_clock::now(); //- std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(randomOffset));
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
		if (new_player->_player) {
			if (auto cc = new_player->_player->GetComponent<GAME::CharacterControllerComponent>()) {
				// NPC와 동일하게 1.8m 높이, 0.5m 반지름으로 초기화
				cc->Initialize(_physicsSystem, 1.8f, 0.5f);
			}
		}
		_players.emplace(new_player->_id, new_player);
		new_player->_logic_thread_idx = _logic_thread_idx;
		if (wasEmpty) {
			MYLOG("First player entered Room " << _room_id << ". Waking up NPCs...");
			for (auto& [id, npc] : _npcs) {
				float randomOffset = (rand() % 200) / 1000.0f; 
				auto scatteredTime = std::chrono::steady_clock::now(); //- std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(randomOffset));
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
		

		auto it = _players.find(player_id);
		if (it != _players.end()) {
			auto session = it->second;
			if (session) {
				if (session->_player) _gridMap.Remove(session->_player.get());
				session->_viewedNpcs.clear(); // [추가] 다음 방 입장을 위해 시야 목록 초기화
			}
			_players.erase(it);
		}
	}

	void Room::RemoveNPC(int64_t npcId)
	{
		auto it = _npcs.find(npcId);
		if (it == _npcs.end()) return;

		// 1. 그리드 맵에서 제거 (가장 중요: 다른 유저의 시야 검색 시 유령 포인터 방지)
		_gridMap.Remove(it->second.get());

		// 2. 이 NPC를 보고 있던 모든 플레이어의 시야 목록에서 제거 및 패킷 전송
		for (auto& [pid, session] : _players)
		{
			if (!session) continue;

			// viewedNpcs에서 ID를 지우는 데 성공했다면 (즉, 보고 있었다면)
			if (session->_viewedNpcs.erase(npcId))
			{
				// 클라이언트에게 삭제(Despawn) 패킷 전송
				SendNpcLeaveToPlayer(session, npcId);
			}
		}

		// 3. 실제 NPC 객체 삭제 및 맵에서 제거
		_npcs.erase(it);

		MYLOG("[Room] NPC " << npcId << " has been removed and cleaned up.");
	}

	void Room::AddNPC(std::unique_ptr<GAME::NPC> npc)
	{
		_gridMap.Add(npc.get());
		_npcs.emplace(npc->GetNpcId(), std::move(npc));
	}

	GAME::NPC* Room::GetNPC(int64_t npc_id)
	{
		auto it = _npcs.find(npc_id);
		if (it == _npcs.end())
		{
			return nullptr;
		}
		return it->second.get();
	}

	void Room::ExecuteActorAction(GAME::Actor* attacker, const GAME::NPCAttackConfig& config)
	{
		if (!attacker) return;

		// 1. 공격 위치 계산 (공격자의 방향/회전 고려)
		common::Quat rot = attacker->GetRotation();
		XMVECTOR rotVec = XMLoadFloat4((XMFLOAT4*)&rot);
		XMVECTOR offsetVec = XMLoadFloat3((XMFLOAT3*)&config.posOffset);

		// 오프셋을 캐릭터가 바라보는 방향으로 회전시킴
		XMVECTOR rotatedOffset = XMVector3Rotate(offsetVec, rotVec);
		common::Vec3 finalPos = attacker->GetPosition();
		XMStoreFloat3((XMFLOAT3*)&finalPos, XMLoadFloat3((XMFLOAT3*)&finalPos) + rotatedOffset);

		// Jolt 트랜스폼 생성
		JPH::RMat44 attackTransform = JPH::RMat44::sRotationTranslation(
			Utils::ToJolt(rot), Utils::ToJolt(finalPos));

		// 2. GridMap을 통해 범위 내 잠재적 타겟 선별
		std::vector<GAME::GameObject*> nearby;
		_gridMap.GetNearbyObjects(finalPos, nearby);

		uint32_t now = static_cast<uint32_t>(GetTickCount64());

		// 피격된 대상들을 모으기 위한 리스트
		std::vector<packet::PlayerHitInfo> player_hits;
		std::vector<packet::NPCHitInfo> npc_hits;

		for (auto* obj : nearby) {
			if (obj->GetId() == attacker->GetId()) continue; // 자가 공격 방지

			if (auto target = dynamic_cast<GAME::Actor*>(obj)) {
				// [추가] 같은 진영끼리는 공격 스킵 (팀킬 방지) 
				if (attacker->GetFaction() == target->GetFaction()) continue;

				// 3. 타겟의 ValidateHit 호출 (공격자 포인터 전달)
				if (target->ValidateHit(_physicsSystem, config.shape, attackTransform, now, attacker, (int32_t)config.damage))
				{
					// [추가] 타겟 타입에 따라 피격 정보 기록
					if (auto p = dynamic_cast<GAME::Player*>(target)) {
						common::Vec3 currentPos = p->GetPosition();
						common::Vec3 attackerPos = attacker->GetPosition();
						common::Vec3 knockDir = common::Normalize(currentPos - attackerPos);
						knockDir.y = 0;
						common::Vec3 knockForce = knockDir * 20.0f; // 넉백 세기 설정

						player_hits.emplace_back(p->GetId(), (int32_t)config.damage, p->_hp, p->GetPosition(), knockForce);
					}
					else if (auto n = dynamic_cast<GAME::NPC*>(target)) {
						npc_hits.emplace_back(n->GetNpcId(), (int32_t)config.damage, n->GetHP());
					}
				}
			}
		}

		// 3. [패킷 전송] 플레이어가 맞았음을 알림
		if (!player_hits.empty()) {
			packet::PacketStream stream;
			packet::SC_PACKET_PLAYER_ATTACK header;
			header._type = packet::PacketType::S2C_P_PLAYER_ATTACK;
			header._attacker_id = attacker->GetId();
			header._hit_count = (uint8_t)player_hits.size();
			stream << header;
			for (auto& h : player_hits) stream << h;

			auto* h_ptr = reinterpret_cast<packet::PacketHeader*>(stream.mutable_data());
			h_ptr->_size = (uint16_t)stream.Size();
			Broadcast(stream.constable_data(), stream.Size());
		}

		// 4. [패킷 전송] NPC가 맞았음을 알림 (NPC끼리 맞았을 때)
		if (!npc_hits.empty()) {
			packet::PacketStream stream;
			packet::SC_PACKET_NPC_ATTACK header;
			header._type = packet::PacketType::S2C_P_NPC_ATTACK;
			header._attacker_id = attacker->GetId();
			header._hit_count = (uint8_t)npc_hits.size();
			stream << header;
			for (auto& h : npc_hits) stream << h;

			auto* h_ptr = reinterpret_cast<packet::PacketHeader*>(stream.mutable_data());
			h_ptr->_size = (uint16_t)stream.Size();
			Broadcast(stream.constable_data(), stream.Size());
		}


	// 4. (디버깅) 서버 판정 범위를 시각화 패킷으로 전송
#ifdef _DEBUG
	// --- [디버그] 서버의 공격 판정 영역을 클라이언트에 가시화 ---
		packet::SC_PACKET_DEBUG_DRAW debug;
		debug._type = packet::PacketType::S2C_P_DEBUG_DRAW;
		debug._size = sizeof(debug);
		debug._position = finalPos; // 계산된 공격 중심 월드 좌표
		debug._rotation = rot;      // 공격자의 월드 회전값
		debug._duration = 0.5f;     // 0.5초 동안만 표시

		// Jolt Shape의 실제 타입을 확인하여 디버그 데이터 추출
		const JPH::Shape* shape = config.shape.GetPtr();
		switch (shape->GetSubType())
		{
		case JPH::EShapeSubType::Sphere:
		{
			auto sphere = static_cast<const JPH::SphereShape*>(shape);
			debug._shape_type = packet::DebugShapeType::SPHERE;
			// x에 반지름 저장
			debug._extents = { sphere->GetRadius(), 0, 0 };
		}
		break;
		case JPH::EShapeSubType::Box:
		{
			auto box = static_cast<const JPH::BoxShape*>(shape);
			JPH::Vec3 halfExtents = box->GetHalfExtent();
			debug._shape_type = packet::DebugShapeType::BOX;
			// x, y, z에 반폭값 저장
			debug._extents = { halfExtents.GetX(), halfExtents.GetY(), halfExtents.GetZ() };
		}
		break;
		case JPH::EShapeSubType::Capsule:
		{
			auto capsule = static_cast<const JPH::CapsuleShape*>(shape);
			debug._shape_type = packet::DebugShapeType::CAPSULE;
			// x에 반지름, y에 실린더 절반 높이 저장
			debug._extents = { capsule->GetRadius(), capsule->GetHalfHeightOfCylinder(), 0 };
		}
		break;
		default:
			// 지원하지 않는 도형은 로그만 남기고 전송 안 함
			// MYLOG("DebugDraw: Unsupported shape type.");
			return;
		}

		// 방 안의 모든 플레이어에게 전송 (서버 판정이 맞는지 개발 중에 확인용)
		Broadcast(reinterpret_cast<const char*>(&debug), sizeof(debug));
#endif
	}

	void Room::StartGame()
	{
		_room_state = RoomState::PLAYING;
		MYLOG("Room " << _room_id << " is now in PLAYING state with " << GetPlayerCount() << " players.");
	}

	bool Room::IsPlayerNearby(const common::Vec3& get_position, float size)
	{
		bool isAnyPlayerNear = false;
		for (auto& [pid, player] : _players) {
			float distSq = common::DistanceSq(get_position, player->_player->GetPosition());
			if (distSq < size * size) { // 50m 이내에 플레이어가 한명이라도 있으면
				isAnyPlayerNear = true;
				break;
			}
		}
		return isAnyPlayerNear;
	}


	void Room::UpdatePhysics(float deltaTime, JPH::TempAllocator* tempAllocator)
	{
		if (!_physicsSystem || _players.empty()) return;

		// --- [추가] 1초 주기로 플레이어 위치 로깅 ---
		static float debugTimer = 0.0f; // static으로 선언하여 값 유지
		debugTimer += deltaTime;
		if (debugTimer >= 1.0f) {
			debugTimer = 0.0f;
			for (auto& [pid, session] : _players) {
				if (session && session->_player) {
					common::Vec3 pos = session->_player->GetPosition();
					MYLOG("[DebugPos] Player " << session->_id << " | X: " << pos.x << " Y: " << pos.y << " Z: " <<
						pos.z);
				}
			}
		}

		uint32_t currentTick = static_cast<uint32_t>(GetTickCount64());

		// --- 1. 시야 내에 있는(활성화된) NPC들 찾기 ---
		std::unordered_set<GAME::NPC*> activeNpcs;
		for (auto& [pid, session] : _players) {
			if (!session || !session->_player) continue;

			std::vector<GAME::GameObject*> nearby;
			// 그리드 맵에서 내 주변(3x3)에 있는 모든 객체를 가져옴
			_gridMap.GetNearbyObjects(session->_player->GetPosition(), nearby);

			for (auto* obj : nearby) {
				if (auto npc = dynamic_cast<GAME::NPC*>(obj)) {
					activeNpcs.insert(npc); // 중복 방지를 위해 set에 저장
				}
			}
		}

		for (auto& [id, npc] : _npcs) {
			// [수정] 그리드 맵 시야 안에 있는 NPC만 물리 시뮬레이션(Kinematic)을 돌림
			if (activeNpcs.contains(npc.get())) {
				if (auto cc = npc->GetComponent<GAME::CharacterControllerComponent>()) {
					// [최적화] 플레이어 근처일 때만 CharacterVirtual::Update 수행
					npc->PhysicsUpdate(deltaTime, tempAllocator);
					
				}
				else if (auto pc = npc->GetComponent<GAME::PhysicsComponent>()) {
					// 일반 리지드 바디일 경우 Jolt 엔진 수준에서 활성화/비활성화
					auto& bi = _physicsSystem->GetBodyInterface();
					bi.ActivateBody(pc->GetBodyID());
				}
			}else
			{
				if (auto pc = npc->GetComponent<GAME::PhysicsComponent>()) {
					// 일반 리지드 바디일 경우 Jolt 엔진 수준에서 활성화/비활성화
					auto& bi = _physicsSystem->GetBodyInterface();
					bi.DeactivateBody(pc->GetBodyID());
				}
			}

			// 기록은 리와인드를 위해 무조건 수행
			npc->RecordSnapshot(currentTick);
		}

		// --- 2. 플레이어 물리 시뮬레이션 및 스마트 동기화 ---
		for (auto& [pid, session] : _players) {
			if (!session || !session->_player) continue;

			auto player = session->_player;
			auto cc = player->GetComponent<GAME::CharacterControllerComponent>();
			if (!cc) continue;

			// [핵심] 물리 엔진 업데이트 (조작 의도 + 넉백 + 중력)
			player->PhysicsUpdate(deltaTime, tempAllocator);

			common::Vec3 serverPos = player->GetPosition();
			common::Vec3 clientTargetPos = player->GetLastClientTargetPos();
			common::Vec3 lastSentPos = player->GetLastSentPos();

			// --- 패킷 전송 조건 체크 (최적화) ---
			// 1. 넉백 중인가? (Impact 속도가 남아있음)
			bool isKnockback = common::Length(cc->GetImpactVelocity()) > 0.1f;

			// 2. 서버-클라이언트 오차가 0.5m 이상이면 (벽에 막힘 등 Desync 발생)
			float desyncDistSq = common::DistanceSq(serverPos, clientTargetPos);

			if (isKnockback || desyncDistSq > (0.5f * 0.5f))
			{
				packet::SC_PACKET_MOVE sync_packet;
				sync_packet._type = common::packet::PacketType::S2C_P_MOVE;
				sync_packet._size = sizeof(sync_packet);
				sync_packet._id = session->_id;
				sync_packet._position = serverPos;
				sync_packet._rotation = player->GetRotation();
				sync_packet._state = player->_state;

				
				// 본인 포함 브로드캐스트 (강제 위치 견인)
				Broadcast(reinterpret_cast<char*>(&sync_packet), sizeof(sync_packet));

				// 전송 기록 갱신
				player->SetLastSentPos(serverPos);
				player->SetLastClientTargetPos(serverPos); // 의도 동기화
			}
			else if (common::DistanceSq(serverPos, lastSentPos) > (0.05f * 0.05f))
			{
				// 3. 오차는 적지만 이동량이 유의미할 때 (5cm 이상)
				// -> 타인에게만 전송 (대역폭 절약)

				packet::SC_PACKET_MOVE sync_packet;
				sync_packet._type = common::packet::PacketType::S2C_P_MOVE;
				sync_packet._size = sizeof(sync_packet);
				sync_packet._id = session->_id;
				sync_packet._position = serverPos;
				sync_packet._rotation = player->GetRotation();
				sync_packet._state = player->_state;

				Broadcast(reinterpret_cast<char*>(&sync_packet), sizeof(sync_packet), session->_id);
				player->SetLastSentPos(serverPos);
			}

			// 히스토리 기록 (리와인드용)
			player->RecordSnapshot(currentTick);
			
		}

		// --- 3. Jolt 월드 시뮬레이션 (Static 지형 및 비-Actor 물리 객체용) ---
		// Actor들은 위에서 CharacterVirtual로 직접 제어했으므로,
		// 여기서는 정적인 장애물이나 동적 프롭들만 계산됩니다.
		_physicsSystem->Update(deltaTime, 1, tempAllocator, _jobSystem);
	}

	void Room::UpdateLogics(float deltaTime, JPH::TempAllocator* tempAllocator)
	{
		ProcessJobs();
		
		if (_players.empty()) return;
		// --- [추가] 2. 플레이어 로직 업데이트 ---
		for (auto& [pid, session] : _players)
		{
			if (session && session->_player)
			{
				// 여기서 Player::Update(deltaTime, allocator)가 호출됩니다.
				session->_player->Update(deltaTime, tempAllocator);
			}
		}
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
		move_packet_data._state = npc->GetState();
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

	void Room::BroadcastToNPCViewers(int npc_id, const char* data, size_t size)
	{
		for (auto& [pid, session] : _players)
		{
			if (!session) continue;

			// 이 세션(유저)이 해당 NPC를 보고 있는가?
			if (session->_viewedNpcs.contains(npc_id))
			{
				session->do_send(data, size);
			}
		}
	}

	void Room::BroadcastToPlayerViewers(long long player_id, const char* data, size_t size)
	{
		// 플레이어 AOI는 아직 gridMap 기반으로 완벽하지 않을 수 있으나,
		// 기본적으로 '같은 방' 혹은 '근처' 개념을 사용해야 함.
		// 여기서는 간단하게 방 전체 전송 (추후 GridMap 기반으로 고도화 가능)
		// 혹은 GridMap을 통해 주변 플레이어를 찾아서 전송
		Broadcast(data, size);
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
				data._state = npc->GetState();
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
		spawn_packet_data._state = npc->GetState();
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
	void Room::HandleAction(const std::shared_ptr<PIP::SERVER::SESSION>& actor,
	                        const common::packet::CS_PACKET_ACTION& action_packet)
	{
		if (!actor || !actor->_player) return;

		std::vector<packet::NPCHitInfo> npc_hits;
		std::vector<packet::PlayerHitInfo> player_hits;

		// TODO: [Action] 공격 모션 알림 (공격자 중심 AOI)
		// SC_PACKET_ACTION_NOTIFY(actor_id, action_type, direction) 패킷을 정의하고 
		// 공격자(actor_id)를 시야에 둔 유저들에게 전송하여 애니메이션을 동기화해야 함.

		switch (action_packet._action_type)
		{
		case packet::ActionType::NORMAL_ATTACK:
			{
				//TODO: [공격 정의] 나중에 무기/스킬 테이블에서 가져오는 구조로 확장 가능
				JPH::Ref<JPH::Shape> attackShape = new JPH::SphereShape(3.0f);// 3m 반경 공격
				JPH::RMat44 attackTransform = JPH::RMat44::sRotationTranslation(
					Utils::ToJolt(action_packet._direction), Utils::ToJolt(action_packet._position));

				// TODO: [GridMap 최적화] 공격 지점 주변 5m 이내 대상들만 1차 선별
				std::vector<GAME::GameObject*> nearbyObjects;
				_gridMap.GetNearbyObjects(action_packet._position, nearbyObjects);


				for (auto* obj : nearbyObjects) {
					// 자기 자신은 제외
					if (obj->GetId() == actor->_id) continue;
					// [수정] Actor 인터페이스로 통합 판정 (NPC/Player 공통)
					if (auto targetActor = dynamic_cast<GAME::Actor*>(obj)) {
						if (actor->_player->GetFaction() == targetActor->GetFaction()) continue;
						if (targetActor->ValidateHit(_physicsSystem, attackShape.GetPtr(), attackTransform,
						                             action_packet._client_time_stamp,
						                             actor->_player.get(), actor->_player->_damage))
						{
							// NPC 피격 기록
							if (auto npc = dynamic_cast<GAME::NPC*>(targetActor))
								npc_hits.emplace_back(npc->GetNpcId(), actor->_player->_damage, npc->GetHP());
							// Player 피격 기록
							else if (auto player = dynamic_cast<GAME::Player*>(targetActor))
								player_hits.emplace_back(player->GetId(), actor->_player->_damage, player->_hp);
						}
					}
					// (확장) Player vs Player 판정도 동일한 로직으로 여기에 추가 가능
				}

#ifdef _DEBUG_PHYSICS_VISUALIZATION
				// --- 4. [디버그] 서버 판정 가시화 패킷 ---
				packet::SC_PACKET_DEBUG_DRAW debug;
				debug._type = packet::PacketType::S2C_P_DEBUG_DRAW;
				debug._size = sizeof(debug);
				debug._position = action_packet._position;
				debug._rotation = action_packet._direction;
				debug._duration = 0.5f;
				// 공격 타입에 맞춘 디버그 도형 설정 (여기서는 구체)
				debug._shape_type = packet::DebugShapeType::SPHERE;
				debug._extents = { 3.0f, 0, 0 };

				Broadcast(reinterpret_cast<const char*>(&debug), sizeof(debug));
#endif
			}
			break;
		case packet::ActionType::SKILL:
			// 스킬 ID(action_packet._action_id)에 따른 다양한 박스/캡슐 판정 로직 추가 지점
			break;
		case packet::ActionType::INTERACT:
			// 상호작용 로직 (아이템 줍기 등)
			break;
		case packet::ActionType::NONE:
			MYERROR("에러!!");
			return;
			break;
		default:
			MYERROR("Unknown action type received: " << static_cast<int>(action_packet._action_type));
			return;
			break;
		}

		// --- 3. [AOI 브로드캐스트] 피격 결과 전송 ---
		// 모든 유저에게 쏘는 것이 아니라, "맞은 놈을 보고 있는 유저"에게만 전송합니다.

		// NPC 피격 알림
		for (const auto& hit : npc_hits) {
			packet::PacketStream stream;
			packet::SC_PACKET_NPC_ATTACK hit_packet;
			hit_packet._type = packet::PacketType::S2C_P_NPC_ATTACK;
			hit_packet._attacker_id = actor->_id;
			hit_packet._hit_count = 1; // 단일 전송 모드

			stream << hit_packet;
			stream << hit;

			auto* h = reinterpret_cast<packet::PacketHeader*>(stream.mutable_data());
			h->_size = static_cast<uint16_t>(stream.Size());

			// [핵심] 이 NPC를 시야에 둔 플레이어들에게만 브로드캐스트
			BroadcastToNPCViewers(hit._target_id, stream.constable_data(), stream.Size());
		}


		// 2. 하단에 플레이어 피격 알림 브로드캐스트 추가
		if (!player_hits.empty()) {
			packet::PacketStream stream;
			packet::SC_PACKET_PLAYER_ATTACK header;
			header._type = packet::PacketType::S2C_P_PLAYER_ATTACK;
			header._attacker_id = actor->_id;
			header._hit_count = (uint8_t)player_hits.size();
			stream << header;
			for (auto& h : player_hits) stream << h;

			auto* h_ptr = reinterpret_cast<packet::PacketHeader*>(stream.mutable_data());
			h_ptr->_size = (uint16_t)stream.Size();
			Broadcast(stream.constable_data(), stream.Size());
		}

		

	}
	void Room::Execute_C2S_MOVE(std::shared_ptr<SESSION> session, const common::packet::CS_PACKET_MOVE& move_packet) {
		if (!session || session->_state != SERVER::SESSION_STATE::ST_INGAME) return;
		auto player = session->_player;
		auto cc = player->GetComponent<GAME::CharacterControllerComponent>();
		// [1] 넉백 힘이 강력하게 작용 중인지 체크 (임계값 2.0f 이상)
		bool isHeavyKnockback = common::Length(cc->GetImpactVelocity()) > 2.0f;
		if (isHeavyKnockback) {
			// [넉백 중 로직]
			// 클라이언트 조작 속도를 0으로 만들어 물리적 밀려남만 허용함
			cc->SetVelocity({ 0, 0, 0 });

			// 이때는 클라이언트의 위치를 억지로 승인하기보다, 서버 물리 엔진이 미는 대로 둡니다.
			// 클라이언트는 서버에서 오는 보정 패킷을 비주얼 오프셋으로 부드럽게 받아냅니다.
			player->SetLastClientTargetPos(move_packet._position);
		}
		else
		{
			auto snapshot = player->GetSnapshotAt(move_packet._client_tick);
			// 2. 과거 위치와 클라이언트가 보낸 위치 사이의 거리 계산
			float moveDist = common::Distance(snapshot._position, move_packet._position);

			// 3. [승인 로직] 초당 5m 속도 플레이어가 0.2초(RTT) 지연 시 약 1m 오차는 정상 범위
			// 해킹이 아니라고 판단되면 클라이언트의 예측 위치를 서버 물리 바디에 즉시 수용
			constexpr float MAX_RECONCILE_DIST = 5.0f;
			if (moveDist < MAX_RECONCILE_DIST) {
				// [A] 이동 승인: 서버 물리 바디를 클라이언트 위치로 즉시 옮겨 동기화함
				player->SetPosition(move_packet._position);
				player->SetLastClientTargetPos(move_packet._position);

				// 순간 이동했으므로 속도는 0으로 초기화 (관성 꼬임 방지)
				cc->SetVelocity({ 0, 0, 0 });
			}
			else 
			{
				// [거절] 너무 멀면(렉/핵) 기존 추적 로직 작동 -> 보정 패킷 발송됨
				common::Vec3 currentPos = player->GetPosition();
				common::Vec3 moveDir = move_packet._position - currentPos;
				// Y축 거리 체크를 위해 moveDir.y = 0; 제거

				float dist = common::Length(moveDir);

				// 텔레포트(5m) 혹은 속도 이동
				if (dist > 5.0f) {
					player->SetPosition(move_packet._position);
					cc->SetVelocity({ 0, 0, 0 });
				}
				else if (dist > 0.01f) {
					// 속도 제한을 50.0f로 넉넉하게 주어 억울한 보정 방지
					common::Vec3 vel = common::Normalize(moveDir) * (dist / 0.02f);
					if (common::Length(vel) > 50.0f) vel = common::Normalize(vel) * 50.0f;
					cc->SetVelocity(vel);
				}
				else {
					cc->SetVelocity({ 0, 0, 0 });
				}
				player->SetLastClientTargetPos(move_packet._position);
			}
		}
		session->_player->SetRotation(move_packet._rotation);
		session->_player->_state = move_packet._state;
	}
	void Room::Execute_C2S_ROOM_ENTER(std::shared_ptr<SESSION> session, const common::packet::CS_PACKET_ENTER_ROOM& enter_packet) {

		// --- [Step 1] 세션 데이터 갱신 ---
		session->_room_id = _room_id; // 현재 방 ID (this->_room_id)
		session->_state = SERVER::SESSION_STATE::ST_INGAME;
		session->_logic_thread_idx = _logic_thread_idx;

		common::Vec3 spawnPos{ 10, 10, 10 };
		session->_player->SetPosition(MapDataManager::Instance()->AdjustPositionToGround(spawnPos));
		session->_player->_hp = 100;

		// --- [Step 2] 패킷 전송 (여기서 다 보냅니다) ---

		// 1. 입장 성공 ACK 전송
		packet::SC_PACKET_ENTER_ROOM_ACK ack_packet;
		ack_packet._type = packet::PacketType::S2C_P_ENTER_ROOM_ACK;
		ack_packet._size = sizeof(ack_packet);
		ack_packet._room_id = _room_id;
		ack_packet._success = true;
		session->do_send(reinterpret_cast<char*>(&ack_packet), sizeof(ack_packet));

		// 2. 다른 플레이어들의 정보를 나에게 전송
		SendRoomInfoToNewPlayer(session);

		// 3. 나의 스폰 패킷 생성 및 전송
		packet::PacketStream self_spawn = packet::MakeSpawnPlayerPacket(session);
		session->do_send(self_spawn.constable_data(), self_spawn.Size()); // 나에게 전송

		// 4. 방에 있는 다른 사람들에게 나의 등장을 알림 (브로드캐스트)
		// 주의: EnterPlayer() 호출 전이므로, Broadcast는 수동으로 session->_id를 제외하거나 포함하여 처리
		Broadcast(self_spawn.constable_data(), self_spawn.Size(), session->_id);

		SendMapDebugDraw(session);
		// --- [Step 3] 최종 입장 완료 (리스트 및 그리드맵 추가) ---
		EnterPlayer(session);

		MYLOG("[Room] Session " << session->_id << " successfully entered Room " << _room_id);
	}


	GAME::Player* Room::GetPlayer(int64_t player_id)
	{
		if (_players.contains(player_id))
		{
			return _players[player_id]->_player.get();
		}
		return nullptr;
	}

	GAME::Actor* Room::GetActor(int64_t actor_id)
	{
		if (auto npc = GetNPC(actor_id))
		{
			return npc;
		}
		if (auto player = GetPlayer(actor_id))
		{
			return player;
		}
		return nullptr;
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

	void Room::CreatePhysicsMapObjects()
	{
		const auto& mapObjects = MapDataManager::Instance()->GetMapObjects();
		JPH::BodyInterface& bodyInterface = _physicsSystem->GetBodyInterface();

		for (const auto& obj : mapObjects)
		{
			// 1. BoxShape 생성 (Half-extents 전달)
			JPH::BoxShapeSettings shapeSettings(Utils::ToJolt(obj._extent));
			JPH::Shape::ShapeResult shapeResult = shapeSettings.Create();

			if (shapeResult.HasError()) continue;

			// 2. Static Body 생성 (Center 위치와 Rotation 회전값 적용)
			JPH::BodyCreationSettings bodySettings(
				shapeResult.Get(),
				Utils::ToJolt(obj._center),
				Utils::ToJolt(obj._rotation),
				JPH::EMotionType::Static,
				Layers::NON_MOVING
			);

			JPH::Body* body = bodyInterface.CreateBody(bodySettings);
			if (body)
			{
				// 물리 세계에 추가 (Static이므로 비활성 상태로 추가)
				bodyInterface.AddBody(body->GetID(), JPH::EActivation::DontActivate);
			}
		}
		MYLOG("[Room] Physics OBB Map Objects created: " << mapObjects.size());
	}

	void Room::SendMapDebugDraw(const std::shared_ptr<SESSION>& session)
	{
		const auto& mapObjects = MapDataManager::Instance()->GetMapObjects();

		for (const auto& obj : mapObjects)
		{
			packet::SC_PACKET_DEBUG_DRAW debug;
			debug._type = packet::PacketType::S2C_P_DEBUG_DRAW;
			debug._size = sizeof(debug);

			debug._position = obj._center;
			debug._rotation = obj._rotation; // Quaternion 회전 포함
			debug._extents = obj._extent;

			debug._shape_type = packet::DebugShapeType::BOX;
			debug._duration = 600.0f; // 10분 유지

			session->do_send(reinterpret_cast<const char*>(&debug), sizeof(debug));
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
		CreatePhysicsMapObjects();
	}
}
