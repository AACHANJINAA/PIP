#include "pch.h"
#include "Room.h"

#include "AIComponent.h"
#include "LuaManager.h"
#include "MapDataManager.h"
#include "Player.h"
#include "PacketHandlers.h"
#include "Tainer.h"
#include "Jolt/Physics/Collision/Shape/HeightFieldShape.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include "PlayerControllerComponent.h"
#include "CombatDef.h"
#include "StageManager.h"

namespace PIP::SERVER
{
	constexpr int MAX_ROOM_PLAYERS = 4;

	Room::Room(int room_id, int logic_thread_idx)
		: _room_id{ room_id }, _logic_thread_idx{ logic_thread_idx }, _max_players{ MAX_ROOM_PLAYERS }, _room_state{ RoomState::WAITING }
	{
		MYLOG("Room " << _room_id << " created. Assigned to Logic Thread " << _logic_thread_idx << " Max Players: " << static_cast<int>(_max_players));
	}

	void Room::Initialize()
	{
		PhysicsInitialize();
		_gridMap.Initialize(-1000, 1000, -1000, 1000, 40);


		// 1. 기본 스테이지(MainStage) 생성 및 물리 로드
		_currentStage = StageManager::Instance()->create_stage("MainStage");
		if (_currentStage) {
			_currentStage->on_initialize(this); // 지형 물리 바디 등록
		}
		/*SpawnInitialNPCs();
		SpawnBoss();*/
	}

	void Room::SpawnInitialNPCs()
	{
		for (int i = 0; i < 500; ++i)
		{
			int64_t npcId = _next_npc_id + (_room_id * 1000LL) + i;

			// 1. 무작위 XZ 위치 결정 (Y는 충분히 높은 곳에서 시작)
			common::Vec3 spawnPos = _currentStage->get_spawn_pos();
			std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
			common::Vec3 real_spawn_pos = {
				spawnPos.x + dist(gen),
				spawnPos.y + 50.0f, // 충분히 높은 곳에서 시작
				spawnPos.z + dist(gen)
			};

			// 2. [핵심] Jolt 물리 지형에 레이를 쏴서 실제 '정확한' 바닥 높이를 즉시 획득
			JPH::RRayCast ray{ Utils::ToJolt(real_spawn_pos), JPH::Vec3(0, -1000.0f, 0) };
			JPH::RayCastResult res;

			// NPC 레이어 자격으로 레이를 쏴서 지형을 찾습니다.
			if (_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, res,
				_physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::NPC),
				_physicsSystem->GetDefaultLayerFilter(Layers::NPC)))
			{
				// 찾은 바닥 높이로 즉시 견인
				real_spawn_pos.y = ray.mOrigin.GetY() + ray.mDirection.GetY() * res.mFraction;
			}
			else {
				// 바닥을 못 찾았다면 (지형 밖 등) 안전한 기본값 설정
				real_spawn_pos.y = MapDataManager::Instance()->AdjustPositionToGround(real_spawn_pos).y;
			}

			// 3. NPC 생성 및 컨트롤러 초기화 (이제 spawnPos는 바닥에 붙어있음)
			auto npc = std::make_unique<GAME::NPC>(npcId, GAME::NPCType::Basic, _room_id, real_spawn_pos, 100);
			auto controller = npc->GetComponent<GAME::CharacterControllerComponent>();
			controller->Initialize(_physicsSystem, 1.8f, 0.5f);

			// 4. [끼임 방지] 지형을 제외한 건물/나무에 박혔는지 최종 체크
			JPH::Shape* npcShape = (JPH::Shape*)controller->GetShape();
			bool isStuck = true;
			int attempts = 0;
			JPH::IgnoreMultipleBodiesFilter terrainFilter; // 지형은 무시

			// 2. 관리 중인 모든 지형 ID를 필터에 추가
			for (auto id : _terrainBodyIDs) {
				terrainFilter.IgnoreBody(id);
			}

			while (isStuck && attempts < 5) {
				JPH::CollideShapeSettings settings;
				JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;

				_physicsSystem->GetNarrowPhaseQuery().CollideShape(
					npcShape, JPH::Vec3::sReplicate(1.0f),
					JPH::RMat44::sTranslation(Utils::ToJolt(real_spawn_pos) + JPH::Vec3(0, 0.9f, 0)),
					settings, JPH::RVec3::sZero(), collector,
					_physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::NPC),
					_physicsSystem->GetDefaultLayerFilter(Layers::NPC),
					terrainFilter
				);

				if (!collector.HadHit()) {
					isStuck = false; // 안 겹침!
				}
				else {
					// 건물 등에 겹쳤다면 옆으로 3m 이동 후 바닥 높이 재조정
					real_spawn_pos.x += (rand() % 2 == 0 ? 3.0f : -3.0f);
					real_spawn_pos.z += (rand() % 2 == 0 ? 3.0f : -3.0f);

					// 다시 바닥 찾기
					JPH::RRayCast reRay{ Utils::ToJolt(real_spawn_pos) + JPH::Vec3(0, 100.0f, 0), JPH::Vec3(0, -200.0f, 0) };
					if (_physicsSystem->GetNarrowPhaseQuery().CastRay(reRay, res))
						real_spawn_pos.y = reRay.mOrigin.GetY() + reRay.mDirection.GetY() * res.mFraction;

					attempts++;
				}
			}

			// 5. 확정된 위치로 물리 좌표와 트랜스폼 동기화
			npc->SetPosition(real_spawn_pos);
			npc->SetLastUpdateTime(std::chrono::steady_clock::now());
			AddNPC(std::move(npc));
		}
	}

	void Room::SpawnBoss()
	{
		int64_t bossId = _next_npc_id + (_room_id * 1000) + 999;
		common::Vec3 bossSpawnPos = { 10.0f, 500.0f, 20.0f };

		JPH::RRayCast ray{ Utils::ToJolt(bossSpawnPos), JPH::Vec3(0, -1000.0f, 0) };
		JPH::RayCastResult res;
		if (_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, res,
			_physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::NPC),
			_physicsSystem->GetDefaultLayerFilter(Layers::NPC)))
		{
			bossSpawnPos.y = ray.mOrigin.GetY() + ray.mDirection.GetY() * res.mFraction;
		}
		else {
			bossSpawnPos.y = MapDataManager::Instance()->AdjustPositionToGround(bossSpawnPos).y;
		}

		auto boss = std::make_unique<GAME::Tainer>(bossId, _room_id, bossSpawnPos);
		auto controller = boss->GetComponent<GAME::CharacterControllerComponent>();
		controller->Initialize(_physicsSystem, 1.5f, 1.0f); // 보스는 더 크게 설정

		boss->SetPosition(bossSpawnPos);
		boss->SetLastUpdateTime(std::chrono::steady_clock::now());

		AddNPC(std::move(boss));
		MYLOG("[Room " << _room_id << "] Boss Tainer Spawned at: (" << bossSpawnPos.x << ", " << bossSpawnPos.y << ", " << bossSpawnPos.z << ")");
	}

	void Room::EnterPlayer(std::shared_ptr<SESSION> new_player)
	{
		int64_t id = new_player->_id;
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
		new_player->_logic_thread_idx = _logic_thread_idx;
		_actors[new_player->_id] = new_player->_player.get();
		if (wasEmpty) {
			MYLOG("First player entered Room " << _room_id << ". Waking up NPCs...");
			for (auto& [id, npc] : _npcs) {
				auto scatteredTime = std::chrono::steady_clock::now();
				npc->SetLastUpdateTime(scatteredTime);
			}
		}
		_players.emplace(id, std::move(new_player));
	}
	void Room::LeavePlayer(int64_t player_id)
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
			_actors.erase(player_id); // [추가] 통합 맵에서 제거
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

		_actors.erase(npcId); // [추가] 통합 맵에서 제거
		// 3. 실제 NPC 객체 삭제 및 맵에서 제거
		_npcs.erase(it);

		MYLOG("[Room] NPC " << npcId << " has been removed and cleaned up.");
	}

	void Room::AddNPC(std::unique_ptr<GAME::NPC> npc)
	{
		int64_t id = npc->GetNpcId();
		_actors[id] = npc.get(); // [추가] 통합 맵에 등록
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

	void Room::ChangeScene(const std::string& nextSceneName)
	{
		// 1. 기존 스테이지 정리 (NPC 및 물리 바디 제거)
		if (_currentStage) {
			_currentStage->on_exit(this);
		}

		_readyPlayers.clear();
		_requestedSceneName = nextSceneName;

		// 2. 새로운 스테이지 생성 및 물리 초기화 (지형 로드)
		_currentStage = StageManager::Instance()->create_stage(nextSceneName);
		if (_currentStage) {
			_currentStage->on_initialize(this);
		}

		// 3. 모든 클라이언트에게 씬 전환 명령 전송
		packet::PacketStream stream;
		packet::SC_PACKET_CHANGE_SCENE change_packet;
		change_packet._type = packet::PacketType::S2C_P_CHANGE_SCENE;
		change_packet._size = sizeof(change_packet) + static_cast<uint16_t>(nextSceneName.size());
		stream << change_packet;
		stream << nextSceneName; // 패킷 뒤에 씬 이름 문자열 추가
		auto* h_ptr = reinterpret_cast<packet::PacketHeader*>(stream.mutable_data());
		h_ptr->_size = (uint16_t)stream.Size(); // 전체 패킷 크기로 업데이트

		Broadcast(stream.constable_data(), stream.Size());
	}

	void Room::ClearAllNPCs()
	{
		// 맵을 순회하며 모든 NPC 제거 (Despawn 패킷 포함)
		std::vector<int64_t> npcIds;
		for (auto& [id, npc] : _npcs) npcIds.push_back(id);

		for (int64_t id : npcIds) {
			RemoveNPC(id); // 기존에 구현된 RemoveNPC 호출 (그리드 및 시야 정리)
		}
		_npcs.clear();
		_activeNpcList.clear();
		MYLOG("[Room] All NPCs cleared for scene transition.");
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

						player_hits.emplace_back(p->GetId(), (int32_t)config.damage, p->GetHP(), p->GetPosition(), knockForce);
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

	void Room::WaitGame()
	{
		_room_state = RoomState::WAITING;
		MYLOG("Room " << _room_id << " is now in WAITING state.");
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
		// [최적화] _npcs(5만 마리) 대신 _activeNpcList(시야 내 NPC)만 순회
		std::vector<GAME::NPC*> bosss;
		for (auto* npc : _activeNpcList) {
			if (npc->is_boss())
			{
				bosss.push_back(npc); // 보스는 따로 처리하기 위해 리스트에 저장
				continue;
			}// 보스는 정밀 물리(PhysicsUpdate) 대상

			auto nc = npc->GetNPCController(); // 캐싱된 포인터 사용
			if (!nc || !npc->IsActive()) continue;

			nc->LightPhysicsUpdate(deltaTime);
			_gridMap.UpdatePosition(npc, npc->GetPosition());
		}

		// [보스 예외 처리] 보스는 거리와 상관없이 항상 정밀 물리(Inner) 대상
		for (auto& boss : bosss) {
			if (boss->is_boss()) {
				boss->PhysicsUpdate(deltaTime, tempAllocator);
				_gridMap.UpdatePosition(boss, boss->GetPosition());
			}
		}
		// --- 3. 플레이어 물리 시뮬레이션 및 스마트 동기화 ---
		for (auto& [pid, session] : _players) {
			if (!session || !session->_player) continue;

			auto player = session->_player;
			auto cc = player->GetComponent<GAME::CharacterControllerComponent>();
			if (!cc) continue;

			// [핵심] 물리 엔진 업데이트 (조작 의도 + 넉백 + 중력)
			player->PhysicsUpdate(deltaTime, tempAllocator);

			// [추가] 플레이어의 그리드맵 위치 갱신
			_gridMap.UpdatePosition(player.get(), player->GetPosition());
		}

		// --- 4. Jolt 월드 시뮬레이션 (Static 지형 및 비-Actor 물리 객체용) ---
		// Actor들은 위에서 CharacterVirtual로 직접 제어했으므로,
		// 여기서는 정적인 장애물이나 동적 프롭들만 계산됩니다.
		_physicsSystem->Update(deltaTime, 1, tempAllocator, _jobSystem);


		// --- [추가] 1초 주기로 플레이어 위치 로깅 ---
		//static float debugTimer = 0.0f; // static으로 선언하여 값 유지
		//debugTimer += deltaTime;
		//if (debugTimer >= 1.0f) {
		//	debugTimer = 0.0f;
		//	for (auto& [pid, session] : _players) {
		//		if (session && session->_player) {
		//			common::Vec3 pos = session->_player->GetPosition();
		//			MYLOG("[DebugPos] Player " << session->_id << " | X: " << pos.x << " Y: " << pos.y << " Z: " << pos.z);
		//		}
		//	}
		//}

		//if (!_physicsSystem || _players.empty()) return;

		// 1. 활성 영역 NPC 찾기
		//std::unordered_set<GAME::NPC*> activeNpcs;
		//std::unordered_set<GAME::NPC*> innerNpcs;

		

		//for (auto& [pid, session] : _players) {
		//	if (!session || !session->_player) continue;
		//	common::Vec3 myPos = session->_player->GetPosition();

		//	std::vector<GAME::GameObject*> nearby;
		//	_gridMap.GetNearbyObjects(myPos, nearby); // 3x3 검색
		//	for (auto* obj : nearby) {
		//		if (auto npc = dynamic_cast<GAME::NPC*>(obj)) {
		//			// 이미 보스에서 처리했을 수 있으므로 체크
		//			if (npc->is_boss()) continue;

		//			activeNpcs.insert(npc);
		//			// 45m 이내면 정밀 물리
		//			float distSq = common::DistanceSq(myPos, npc->GetPosition());
		//			if (distSq <= 45.0f * 45.0f) innerNpcs.insert(npc);
		//		}
		//	}
		//}

		//// 2. NPC 물리 업데이트 (LOD 적용)
		//std::vector<GAME::NPC*> bosss;
		//for (auto& [id, npc] : _npcs) {
		//	auto nc = npc->GetComponent<GAME::NPCControllerComponent>();
		//	if (!nc) continue;
		//	if (!npc->IsActive()) continue;
		//	if (npc->is_boss())
		//	{
		//		bosss.push_back(npc.get());
		//		continue;
		//	}

		//	nc->LightPhysicsUpdate(deltaTime);
		//	_gridMap.UpdatePosition(npc.get(), npc->GetPosition());
		//	//if (innerNpcs.contains(npc.get())) {
		//	//	// [Tier 1] 정밀 물리 (벽 충돌 포함)
		//	//	npc->PhysicsUpdate(deltaTime, tempAllocator);
		//	//	_gridMap.UpdatePosition(npc.get(), npc->GetPosition());
		//	//}
		//	//else if (activeNpcs.contains(npc.get())) {
		//	//	// [Tier 2] 가벼운 물리 (중력 + 지형 보정) - AI 속도로 움직임!
		//	//	nc->LightPhysicsUpdate(deltaTime);
		//	//	_gridMap.UpdatePosition(npc.get(), npc->GetPosition());
		//	//}
		//	
		//}

		//// [보스 예외 처리] 보스는 거리와 상관없이 항상 정밀 물리(Inner) 대상
		//for (auto& boss : bosss) {
		//	if (boss->is_boss()) {
		//		boss->PhysicsUpdate(deltaTime, tempAllocator);
		//		_gridMap.UpdatePosition(boss, boss->GetPosition());
		//		//activeNpcs.insert(npc.get());
		//		//innerNpcs.insert(npc.get());
		//	}
		//}
		//// --- 3. 플레이어 물리 시뮬레이션 및 스마트 동기화 ---
		//for (auto& [pid, session] : _players) {
		//	if (!session || !session->_player) continue;

		//	auto player = session->_player;
		//	auto cc = player->GetComponent<GAME::CharacterControllerComponent>();
		//	if (!cc) continue;

		//	// [핵심] 물리 엔진 업데이트 (조작 의도 + 넉백 + 중력)
		//	player->PhysicsUpdate(deltaTime, tempAllocator);

		//	// [추가] 플레이어의 그리드맵 위치 갱신
		//	_gridMap.UpdatePosition(player.get(), player->GetPosition());
		//}

		//// --- 4. Jolt 월드 시뮬레이션 (Static 지형 및 비-Actor 물리 객체용) ---
		//// Actor들은 위에서 CharacterVirtual로 직접 제어했으므로,
		//// 여기서는 정적인 장애물이나 동적 프롭들만 계산됩니다.
		//_physicsSystem->Update(deltaTime, 1, tempAllocator, _jobSystem);
	
	}

	void Room::UpdateLogics(float deltaTime, JPH::TempAllocator* tempAllocator)
	{
		// 1. 방에 플레이어가 없으면 로직을 완전히 멈춤
		if (_players.empty()) return;

		/// 1. [중요] 지난 프레임의 리스트를 비우고 새로 수집 (메모리 재할당 방지 위해 clear만)
		_activeNpcList.clear();
		std::unordered_set<int64_t> processedIds; // 중복 방지 (여러 플레이어 시야에 걸칠 경우)

		// --- [Step 1] 활성 NPC 수집 (O(M)) ---
		for (auto& [pid, session] : _players) {
			if (!session || !session->_player) continue;

			// [추가] 아직 준비가 되지 않은(로딩 중인) 플레이어에게는 AOI 패킷을 보내지 않음
			if (!_readyPlayers.contains(pid)) continue;

			GAME::Player* player = session->_player.get();
			common::Vec3 myPos = player->GetPosition();

			// [플레이어 이동 동기화] (기존 코드 유지)
			if (player->IsDirty()) {
				packet::SC_PACKET_MOVE res;
				res._type = common::packet::PacketType::S2C_P_MOVE;
				res._size = sizeof(res);
				res._id = player->GetId();
				res._position = player->GetPosition();
				res._rotation = player->GetRotation();
				res._state = player->GetState();
				res._action_id = player->GetActionId();
				Broadcast(reinterpret_cast<const char*>(&res), sizeof(res));
				player->SyncSentData();
			}

			// [AOI 검색] 120m 내 NPC 찾기
			std::vector<GAME::GameObject*> nearby;
			_gridMap.GetNearbyObjects(myPos, nearby);

			// [추가] 현재 플레이어 주변에 있는 NPC ID들을 수집할 셋
			std::unordered_set<int64_t> currentNearbyIds;

			for (auto* obj : nearby) {
				if (auto npc = dynamic_cast<GAME::NPC*>(obj)) {
					// [검증] 살아있고, 아직 리스트에 없는 놈만 추가
					if (npc->IsActive()) {
						currentNearbyIds.insert(npc->GetId());
						if (!processedIds.contains(npc->GetId())) {
							_activeNpcList.push_back(npc);
							processedIds.insert(npc->GetId());
						}

						// 시야 진입 패킷 (모든 플레이어 개별 체크)
						if (!session->_viewedNpcs.contains(npc->GetId())) {
							session->_viewedNpcs.insert(npc->GetId());
							SendNpcSpawnToPlayer(session, npc);
						}
					}
				}
			}

			// [핵심 추가] AOI 이탈 체크
			// session->_viewedNpcs(이전 프레임까지 보던 목록)와 currentNearbyIds(현재 주변 목록) 비교
			for (auto it = session->_viewedNpcs.begin(); it != session->_viewedNpcs.end(); ) {
				int64_t npcId = *it;

				// 보스는 거리에 상관없이 항상 보여야 하므로 체크 (서버의 전체 NPC 맵에서 확인)
				auto npcIt = _npcs.find(npcId);
				bool isBoss = (npcIt != _npcs.end() && npcIt->second->is_boss());

				// 보스가 아니고, 현재 시야 목록(nearby)에 없다면 시야에서 나간 것임
				if (!isBoss && !currentNearbyIds.contains(npcId)) {
					SendNpcLeaveToPlayer(session, npcId); // S2C_NPC_DESPAWN 패킷 전송
					it = session->_viewedNpcs.erase(it);   // 서버측 관리 목록에서도 제거
				}
				else {
					++it;
				}
			}
		}
		

		// 보스는 거리 상관없이 항상 리스트에 추가하고 모든 플레이어에게 전송
		for (auto& [id, npc] : _npcs) {
			if (npc->is_boss() && npc->IsActive()) {
				if (!processedIds.contains(id)) {
					_activeNpcList.push_back(npc.get());
					processedIds.insert(id);
				}

				// 보스는 모든 플레이어에게 보여야 함
				for (auto& [pid, session] : _players) {
					// [추가] 로딩 중인 유저에게 미리 시야를 열어주지 마세요!
					if (!_readyPlayers.contains(pid)) continue;

					if (!session->_viewedNpcs.contains(id)) {
						session->_viewedNpcs.insert(id);
						SendNpcSpawnToPlayer(session, npc.get());
					}
				}
#ifdef _DEBUG
				//if (GetRoomId() == 0)
				//{
				//	auto ai = npc->GetComponent<GAME::AIComponent>();
				//	if (ai && ai->GetBlackboard()->has("debug_node_name")) {
				//		//auto bb = ai->GetBlackboard();
				//		//std::string nodeName = bb->get<std::string>("debug_node_name");
				//		//int status = bb->get<int>("debug_node_status");

				//		//// 상태를 문자열로 변환 (0:SUCCESS, 1:FAILURE, 2:RUNNING)
				//		//std::string statusStr = (status == 2) ? "[RUNNING]" : (status == 0 ? "[SUCCESS]" : "[FAILURE]");
				//		//std::string debugText = nodeName + " " + statusStr;

				//		//common::packet::PacketStream stream;
				//		//common::packet::SC_PACKET_DEBUG_BT_INFO pkt;
				//		//pkt._type = common::packet::PacketType::S2C_P_DEBUG_BT_INFO;
				//		//pkt._actor_id = npc->GetId();

				//		//stream << pkt;
				//		//stream << debugText; // 가변 문자열(노드 이름 + 상태) 추가

				//		//// 헤더 사이즈 갱신
				//		//auto* header = reinterpret_cast<common::packet::PacketHeader*>(stream.mutable_data());
				//		//header->_size = (uint16_t)stream.Size();

				//		//// [중요] stream.mutable_data()를 보내야 전체 내용이 전달됩니다!
				//		//Broadcast(stream.mutable_data(), stream.Size());

				//		auto bb = ai->GetBlackboard();
				//		std::string nodeName = bb->get<std::string>("debug_node_name");
				//		int status = bb->get<int>("debug_node_status");

				//		std::string statusStr = (status == 2) ? "[RUNNING]" : (status == 0 ? "[SUCCESS]" : "[FAILURE]");
				//		std::string debugText = nodeName + " " + statusStr;
				//		//MYLOG("[DebugBT] " << npc->GetId() << " Boss" << debugText);
				//	}
				//}
#endif
			}
		}


		// --- [Step 2] 로직 업데이트 루프 (O(M)) ---
		// [핵심] 이제 _activeNpcList에는 120m 내의 '살아있는' NPC만 들어있습니다.
		// 루프 내부에 if문이 거의 없어 분기 예측이 매우 안정적입니다.
		for (GAME::NPC* npc : _activeNpcList) {
			// AI/BT 업데이트
			npc->Update(deltaTime, tempAllocator);

			// 높이 보정 및 맵 이탈 방지
			common::Vec3 pos = npc->GetPosition();
			auto mapData = PIP::MapDataManager::Instance();

			// [여기에 추가!] 3. NaN(비정상 값) 체크 및 복구
			if (std::isnan(pos.x) || std::isnan(pos.y) || std::isnan(pos.z)) {
				MYERROR("NPC ID: " << npc->GetId() << " has NaN position! Resetting to safe spot.");
				pos = { 10.0f, 10.0f, 10.0f }; // 안전한 기본 위치 (마을 중앙 등)
				npc->SetPosition(pos); // 물리 바디 위치 강제 초기화
			}

			//// 1. 맵 경계 체크 (IsInsideMap이 false면 맵 밖임)
			//if (!mapData->IsInsideMap(pos.x, pos.z)) {
			//	// 맵 밖으로 나갔다면 안전한 위치(AdjustPositionToGround)로 강제 견인
			//	pos = mapData->AdjustPositionToGround(pos);
			//}
			//else {
			//	// 2. 맵 안쪽이라도 땅 밑으로 꺼지거나 공중에 뜨는 것을 방지 (높이 보정)
			//	pos = mapData->AdjustPositionToGround(pos);
			//}
			//npc->SetPosition(pos);
			// 부드러운 회전 처리
			common::Vec3 vel = npc->GetVelocity();
			if (vel.x * vel.x + vel.z * vel.z > 0.01f) {
				float angle = std::atan2(vel.x, vel.z);
				DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0, angle, 0);
				common::Quat rot;
				XMStoreFloat4((XMFLOAT4*)&rot, q);
				npc->SetRotation(rot);
			}

			// 리와인드용 스냅샷
			auto now = std::chrono::steady_clock::now();
			npc->SetLastUpdateTime(now);
			npc->RecordSnapshot(static_cast<uint32_t>(GetTickCount64()));
		}

		// --- [Step 3] 패킷 전송 및 클린업 (기존 로직) ---
		_npcSyncTimer += deltaTime;
		if (_npcSyncTimer >= 0.05f) {
			BroadcastNpcBatch(); // 여기서도 _activeNpcList를 활용하게 고치면 더 좋습니다!
			_npcSyncTimer = 0.0f;
		}

		// 플레이어 업데이트 및 스냅샷 기록
		uint32_t currentTick = static_cast<uint32_t>(GetTickCount64());
		for (auto& [pid, session] : _players) {
			session->_player->Update(deltaTime, tempAllocator);
			session->_player->RecordSnapshot(currentTick);
		}

		// 5. 스테이지 전용 업데이트 (보스 페이즈, 트리거 등)
		if (_currentStage) {
			_currentStage->update(this, deltaTime);
		}

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
		move_packet_data._type = common::packet::PacketType::S2C_P_NPC_MOVE;
		move_packet_data._npc_id = npc->GetNpcId();
		move_packet_data._position = npc->GetPosition();
		move_packet_data._velocity = npc->GetVelocity();
		move_packet_data._rotation = npc->GetRotation();
		move_packet_data._state = npc->GetState();
		move_packet_data._action_id = npc->GetActionId();
		move_packet_data._time_stamp = static_cast<uint32_t>(GetTickCount64());

		packet::PacketStream finalStream;
		finalStream << move_packet_data;
		finalStream << npc->GetName();

		auto* final_header = reinterpret_cast<packet::PacketHeader*>(finalStream.mutable_data());
		final_header->_size = static_cast<uint16_t>(finalStream.Size());

		Broadcast(finalStream.constable_data(), finalStream.Size());
	}
	void Room::Broadcast(const char* data, size_t size, int64_t except_id)
	{
		for (auto& pair : _players)
		{
			if (pair.first == except_id) continue;
			pair.second->do_send(data, size);
		}
	}

	void Room::BroadcastToNPCViewers(int64_t npc_id, const char* data, size_t size)
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

	void Room::BroadcastToPlayerViewers(int64_t player_id, const char* data, size_t size)
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
		for (auto& npc : _activeNpcList) {
			if (npc->IsDirty()) dirtyNPCs.push_back(npc);
		}
		if (dirtyNPCs.empty()) return;

		// 2. 플레이어별 전송
		for (auto& [pid, session] : _players)
		{
			if (!session || !session->_player) continue;

			packet::PacketStream stream;
			packet::SC_PACKET_NPC_MOVE_BATCH header;
			header._type = packet::PacketType::S2C_P_NPC_MOVE_BATCH;
			header._count = 0;
			stream << header;

			int count = 0;
			for (auto* npc : dirtyNPCs)
			{
				if (std::isnan(npc->GetPosition().x)|| std::isnan(npc->GetPosition().y) || std::isnan(npc->GetPosition().z)) {
					// 해당 NPC 위치가 NaN이면 전송 스킵하거나 로그 출력
					MYLOG("[Warning] NPC Id:" << npc->GetNpcId() << "-" <<static_cast<int32_t>(npc->GetNpcType()) << " has invalid position (NaN). Skipping move packet.");
					continue;
				}
				// [핵심] 시야 리스트(View List)에 있는 놈만 보낸다!
				if (!session->_viewedNpcs.contains(npc->GetNpcId()))
					continue;

				packet::NPCMoveData data;
				data._npc_id = npc->GetNpcId();
				data._position = npc->GetPosition();
				data._velocity = npc->GetVelocity();
				data._rotation = npc->GetRotation();
				data._time_stamp = static_cast<uint32_t>(GetTickCount64());
				data._state = npc->GetState();
				data._action_id = npc->GetActionId();

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
		spawn_packet_data._type = common::packet::PacketType::S2C_P_NPC_SPAWN;
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
	void Room::SendNpcLeaveToPlayer(const std::shared_ptr<SESSION>& session, int64_t npcId)
	{
		packet::SC_PACKET_NPC_DESPAWN despawn_packet;
		despawn_packet._type = common::packet::PacketType::S2C_P_NPC_DESPAWN;
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
					int32_t new_hp = player_session->_player->GetHP() - damage;
					if (new_hp < 0) new_hp = 0;
					player_session->_player->SetHP(new_hp);

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
	void Room::HandleAction(const std::shared_ptr<SESSION>& session,
	                        const common::packet::CS_PACKET_ACTION& action_packet)
	{
		if (!session || !session->_player) return;

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
					if (obj->GetId() == session->_id) continue;
					// [수정] Actor 인터페이스로 통합 판정 (NPC/Player 공통)
					if (auto targetActor = dynamic_cast<GAME::Actor*>(obj)) {
						if (session->_player->GetFaction() == targetActor->GetFaction()) continue;
						if (targetActor->ValidateHit(_physicsSystem, attackShape.GetPtr(), attackTransform,
						                             action_packet._client_time_stamp,
						                             session->_player.get(), session->_player->_damage))
						{
							// NPC 피격 기록
							if (auto npc = dynamic_cast<GAME::NPC*>(targetActor))
							{
								// ValidateHit 내부에서 HP가 깎였다면
								if (npc->GetHP() <= 0 && npc->IsActive()) {
									OnNPCDead(npc); // 사망 처리 호출
								}
								npc_hits.emplace_back(npc->GetNpcId(), session->_player->_damage, npc->GetHP());
							}
							// Player 피격 기록
							else if (auto player = dynamic_cast<GAME::Player*>(targetActor))
								player_hits.emplace_back(player->GetId(), session->_player->_damage, player->GetHP());
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
			hit_packet._attacker_id = session->_id;
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
			header._attacker_id = session->_id;
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
		if (!player) return;

		auto pcc = player->GetComponent<GAME::PlayerControllerComponent>();
		if (!pcc) return;

		// 1. 텔레포트 체크 (검증용)
		common::Vec3 currentServerPos = player->GetPosition();
		float moveDist = common::Distance(currentServerPos, move_packet._position);

		// 2. [의도 수용] 클라이언트가 보낸 방향 벡터를 그대로 사용
		common::Vec3 moveDir = move_packet._move_dir;

		if (moveDist < 10.0f) { // 정상 범위 이내일 때 (클라이언트와 서버의 순간적인 거리 차이가 10 이하여야 함)
			if (common::LengthSq(moveDir) > 0.0001f) {
				// 방향 벡터에 플레이어의 진짜 속도를 곱해 물리 엔진에 설정
				float currentTargetSpeed = 0.0f;

				switch (move_packet._state) {
				case common::packet::EntityState::RUN:
					currentTargetSpeed = common::move_speed::player_run_speed; // 달리기 속도 (기획에 맞게 수정)
					break;
				case common::packet::EntityState::MOVE: // 걷기 상태가 있다면
					currentTargetSpeed = common::move_speed::player_walk_speed;  // 걷기 속도
					break;
				default:
					currentTargetSpeed = 0.f; // 기본값 10.0f
					break;
				}

				player->SetSpeed(currentTargetSpeed);
				pcc->SetMoveVelocity(common::Normalize(moveDir) * currentTargetSpeed);
			}
			else {
				pcc->SetMoveVelocity({ 0, 0, 0 });
			}
		}
		else {
			// 너무 멀면 강제 스냅 (핵/렉 방지)
			pcc->SetMoveVelocity({ 0, 0, 0 });
			pcc->SetPosition(currentServerPos);
		}

		// 상태 동기화 (기존 로직 유지)
		player->SetRotation(move_packet._rotation);
		player->SetState(move_packet._state);
		player->SetActionId(move_packet._action_id);
		player->SetLastClientTargetPos(move_packet._position);

	}
	void Room::Execute_C2S_ROOM_ENTER(const std::shared_ptr<SESSION>& session, const common::packet::CS_PACKET_ENTER_ROOM& enter_packet) {

		// --- [Step 1] 세션 데이터 갱신 ---
		session->_room_id = _room_id; // 현재 방 ID (this->_room_id)
		session->_state = SERVER::SESSION_STATE::ST_INGAME;
		session->_logic_thread_idx = _logic_thread_idx;

		EnterPlayer(session);

		packet::SC_PACKET_ENTER_ROOM_ACK ack_packet;
		ack_packet._type = packet::PacketType::S2C_P_ENTER_ROOM_ACK;
		ack_packet._size = sizeof(ack_packet);
		ack_packet._room_id = _room_id;
		ack_packet._success = true;
		session->do_send(reinterpret_cast<char*>(&ack_packet), sizeof(ack_packet));

		// 3. [핵심] 클라이언트에게 현재 방의 씬으로 전환하라고 명령 (로딩 시작 유도)
		if (_currentStage) {
			std::string sceneName = _currentStage->get_stage_name();

			packet::PacketStream stream;
			packet::SC_PACKET_CHANGE_SCENE change_packet;
			change_packet._type = packet::PacketType::S2C_P_CHANGE_SCENE;
			stream << change_packet;
			stream << sceneName; // 가변 길이 씬 이름 추가

			auto* h_ptr = reinterpret_cast<packet::PacketHeader*>(stream.mutable_data());
			h_ptr->_size = (uint16_t)stream.Size();

			session->do_send(reinterpret_cast<const char*>(stream.constable_data()), stream.Size());
			MYLOG("[Room] Sent CHANGE_SCENE(" << sceneName << ") to Session " << session->_id);
		}

		//common::Vec3 spawn_pos = _currentStage->get_spawn_pos();
		//float tx = spawn_pos.x;
		//float tz = spawn_pos.z;

		//// 2. 충분히 높은 곳에서 아래로 레이 발사 준비
		//JPH::RRayCast ray;
		//ray.mOrigin = JPH::Vec3(tx, 500.0f, tz); // 하늘 높은 곳에서 발사
		//ray.mDirection = JPH::Vec3(0, -1000.0f, 0); // 땅바닥으로 길게 발사

		//// 3. 지형 레이캐스트 실행
		//JPH::RayCastResult ray_result;
		//float finalY = 0.0f;

		//// 지형 레이어(NON_MOVING)만 검사하도록 쿼리
		//if (_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, ray_result)) {
		//	float hitY = ray.mOrigin.GetY() + ray.mDirection.GetY() * ray_result.mFraction;
		//	finalY = hitY + 2.0f; // 지면 위 2m 안착
		//	MYLOG("[SPAWN] Ray Hit at Y: " << hitY << ", Spawn Y: " << finalY);
		//}
		//else {
		//	// 레이가 빗나갈 경우 MapDataManager 데이터 기반으로 강제 보정
		//	finalY = MapDataManager::Instance()->GetGroundHeight(tx, tz) + 2.0f;
		//	MYERROR("[SPAWN] Ray Missed! Using data height: " << finalY);
		//}

		//// --- [Step 4] 이제 _character가 생성되었으므로 안전하게 위치 설정 ---
		//common::Vec3 spawnPos{ tx, finalY, tz };
		//session->_player->SetPosition(spawnPos);
		//session->_player->SetHP(100);

		//// 1. 입장 성공 ACK 전송
		//packet::SC_PACKET_ENTER_ROOM_ACK ack_packet;
		//ack_packet._type = packet::PacketType::S2C_P_ENTER_ROOM_ACK;
		//ack_packet._size = sizeof(ack_packet);
		//ack_packet._room_id = _room_id;
		//ack_packet._success = true;
		//session->do_send(reinterpret_cast<char*>(&ack_packet), sizeof(ack_packet));

		//// 2. 다른 플레이어들의 정보를 나에게 전송
		//SendRoomInfoToNewPlayer(session);

		//// 3. 나의 스폰 패킷 생성 및 전송
		//packet::PacketStream self_spawn = packet::MakeSpawnPlayerPacket(session);
		//session->do_send(self_spawn.constable_data(), self_spawn.Size()); // 나에게 전송

		//// 4. 방에 있는 다른 사람들에게 나의 등장을 알림 (브로드캐스트)
		//// 주의: EnterPlayer() 호출 전이므로, Broadcast는 수동으로 session->_id를 제외하거나 포함하여 처리
		//Broadcast(self_spawn.constable_data(), self_spawn.Size(), session->_id);

		//SendMapDebugDraw(session);
		

		MYLOG("[Room] Session " << session->_id << " successfully entered Room " << _room_id);
	}

	void Room::Execute_C2S_PLAYER_READY(const std::shared_ptr<SESSION>& session,
		const common::packet::CS_PACKET_PLAYER_READY& ready_packet)
	{
		if (_readyPlayers.contains(session->_id)) {
			return;
		}
		_readyPlayers.insert(session->_id);
		// [Case 1] 이미 게임이 진행 중인 방에 들어온 경우 (Late Joiner)
		if (_room_state == RoomState::PLAYING) {
			MYLOG("[Room " << _room_id << "] Late Joiner Ready: Session " << session->_id);

			SetupPlayerSpawn(session);

			// *참고: NPC는 UpdateLogics의 AOI 로직에 의해 다음 루프에서 자동으로 스폰 패킷이 날아갑니다.
			return;
		}

		MYLOG("[Room " << _room_id << "] Session " << session->_id << " is READY for scene: " << _requestedSceneName);

		// 방에 있는 모든 플레이어가 로딩을 마쳤는가?
		if (_readyPlayers.size() == _players.size()) {
			MYLOG("[Room " << _room_id << "] All players READY! Starting Stage: " << _requestedSceneName);

			// 1. 스테이지 진입 (NPC 및 보스 스폰)
			if (_currentStage) {
				_currentStage->on_enter(this);
			}

			// 2. [핵심] 대기 중인 모든 플레이어를 루프 돌며 스폰 처리!!
			for (auto& [id, player_session] : _players) {
				SetupPlayerSpawn(player_session);
			}
			
			if (_players.size() >= 1) {
				MYLOG("First player or more player entered Room " << _room_id << ". Waking up NPCs...");
				for (auto& [id, npc] : _npcs) {
					auto scatteredTime = std::chrono::steady_clock::now();
					npc->SetLastUpdateTime(scatteredTime);
				}
			}
			//_readyPlayers.clear();
		}
	}

	void  Room::SetupPlayerSpawn(const std::shared_ptr<SESSION>& session) {
		common::Vec3 spawn_pos = _currentStage->get_spawn_pos();
		float tx = spawn_pos.x;
		float tz = spawn_pos.z;

		// 2. 충분히 높은 곳에서 아래로 레이 발사 준비
		JPH::RRayCast ray;
		ray.mOrigin = JPH::Vec3(tx, 500.0f, tz); // 하늘 높은 곳에서 발사
		ray.mDirection = JPH::Vec3(0, -1000.0f, 0); // 땅바닥으로 길게 발사

		// 3. 지형 레이캐스트 실행
		JPH::RayCastResult ray_result;
		float finalY = 0.0f;

		// 지형 레이어(NON_MOVING)만 검사하도록 쿼리
		if (_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, ray_result)) {
			float hitY = ray.mOrigin.GetY() + ray.mDirection.GetY() * ray_result.mFraction;
			finalY = hitY + 2.0f; // 지면 위 2m 안착
			MYLOG("[SPAWN] Ray Hit at Y: " << hitY << ", Spawn Y: " << finalY);
		}
		else {
			// 레이가 빗나갈 경우 MapDataManager 데이터 기반으로 강제 보정
			finalY = MapDataManager::Instance()->GetGroundHeight(tx, tz) + 2.0f;
			MYERROR("[SPAWN] Ray Missed! Using data height: " << finalY);
		}

		// --- [Step 4] 이제 _character가 생성되었으므로 안전하게 위치 설정 ---
		common::Vec3 spawnPos{ tx, finalY, tz };
		session->_player->SetPosition(spawnPos);
		session->_player->SetHP(100);

		SendRoomInfoToNewPlayer(session);

		// 3. 나의 스폰 패킷 생성 및 전송
		packet::PacketStream self_spawn = packet::MakeSpawnPlayerPacket(session);
		session->do_send(self_spawn.constable_data(), self_spawn.Size()); // 나에게 전송

		// DW추가 : npc 카운트 패킷 전송 (방 입장 시 NPC 수 알려주기)
		packet::SC_PACKET_SCENE_AWAKE npc_count_packet;
		npc_count_packet._type = packet::PacketType::S2C_P_NPC_COUNT;
		npc_count_packet._size = sizeof(npc_count_packet);
		npc_count_packet._boss_count = 1; // 보스 마리 수
		npc_count_packet._boss_start_id = _next_npc_id + (_room_id * 1000) + 999; // 보스 ID
		npc_count_packet._npc_count = static_cast<uint16_t>(_npcs.size()) - npc_count_packet._boss_count;
		npc_count_packet._npc_start_id = _next_npc_id + (_room_id * 1000); // 일반 NPC ID 시작 인덱스 번호
		session->do_send(reinterpret_cast<char*>(&npc_count_packet), sizeof(npc_count_packet));

		// 4. 방에 있는 다른 사람들에게 나의 등장을 알림 (브로드캐스트)
		// 주의: EnterPlayer() 호출 전이므로, Broadcast는 수동으로 session->_id를 제외하거나 포함하여 처리
		Broadcast(self_spawn.constable_data(), self_spawn.Size(), session->_id);
#ifdef _DEBUG
		// 5. 기타 환경 정보(디버그 드로 등) 전송
		SendMapDebugDraw(session);
#endif

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
		auto it = _actors.find(actor_id);
		if (it != _actors.end())
		{
			return it->second; // 플레이어든 NPC든 즉시 반환
		}
		return nullptr;
	}

	std::map<int64_t, common::Vec3> Room::GetPlayersPos() const
	{
		std::map<int64_t, common::Vec3> positions;
		for (const auto& [pid, session] : _players)
		{
			if (session && session->_player)
			{
				positions[pid] = session->_player->GetPosition();
			}
		}
		return positions;
	}

	void Room::CreatePhysicsTerrain() {

		const auto& terrainTiles = MapDataManager::Instance()->GetTerrainTiles();
		JPH::BodyInterface& bodyInterface = _physicsSystem->GetBodyInterface();

		for (const auto& tile : terrainTiles)
		{
			if (!tile.shape) continue;
			// 무거운 Create() 과정 없이, 미리 생성된 tile.shape을 그대로 전달
			JPH::BodyCreationSettings bodySettings(
				tile.shape,
				JPH::RVec3(0, 0, 0),
				JPH::Quat::sIdentity(),
				JPH::EMotionType::Static,
				Layers::NON_MOVING
			);

			JPH::Body* terrainBody = bodyInterface.CreateBody(bodySettings);
			_terrainBodyIDs.push_back(terrainBody->GetID());

			bodyInterface.AddBody(terrainBody->GetID(), JPH::EActivation::DontActivate);
		}
		MYLOG("Physics Landscapes created: " << _terrainBodyIDs.size());

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

	void Room::CreatePhysicsStaticMeshCollisions()
	{
		// 방마다 개별적으로 존재하는 물리 시스템의 BodyInterface
		auto& body_interface = _physicsSystem->GetBodyInterface();

		// 전역적으로 관리되는 Shape 리스트 가져오기
		const auto& shared_shapes = MapDataManager::Instance()->GetStaticMeshTiles();

		for (const auto& tile : shared_shapes)
		{
			// 동일한 Shape을 참조하여 각 방에 맞는 Body 생성
			JPH::BodyCreationSettings settings(
				tile.shape,
				JPH::RVec3::sZero(),
				JPH::Quat::sIdentity(),
				JPH::EMotionType::Static,
				Layers::NON_MOVING
			);

			// 실제 바디 생성 (메모리에는 Shape 데이터가 중복되지 않음!)
			body_interface.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
		}
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

	void Room::OnNPCDead(GAME::NPC* npc)
	{
		int64_t npcId = npc->GetId();
		
		npc->SetState(packet::EntityState::DEAD);
		npc->SetDeathAnimationTime(std::chrono::milliseconds(5000));
		// 1. 물리 및 그리드 제거
		if (auto cc = npc->GetComponent<GAME::CharacterControllerComponent>()) {
			cc->SetPhysicsActive(false);
		}

		// 3. 타이머 잡 등록 (Server::AddTimerJob 사용)
		int workerIdx = _logic_thread_idx;

		// 1초뒤 실제 죽음 발생
		Server::Instance()->AddTimerJob(workerIdx, npc->GetDeathAnimationTime(), [this, npcId]() {
			this->PushJob([this, npcId]() {
				if (auto* npc = this->GetNPC(npcId)) {
					npc->SetActive(false);

					for (auto& [pid, session] : _players)
					{
						if (session->_viewedNpcs.contains(npcId))
						{
							SendNpcLeaveToPlayer(session, npcId);
							session->_viewedNpcs.erase(npcId);
						}
					}
					_gridMap.Remove(npc);
				}
				});
			});
		// 10초(10000ms) 뒤 부활 예약
		Server::Instance()->AddTimerJob(workerIdx, npc->GetRespawnDelay() + npc->GetDeathAnimationTime(), [this, npcId]() {
			// 타이머 스레드에서 바로 부활시키면 레이스 컨디션 발생하므로 다시 PushJob으로 던짐
			this->PushJob([this, npcId]() {
				if (auto* npc = this->GetNPC(npcId)) {
					this->RespawnNPC(npc);
				}
				});
			});

	}

	void Room::RespawnNPC(GAME::NPC* npc)
	{
		// 1. 상태 및 위치 초기화
		npc->SetState(packet::EntityState::IDLE);
		npc->SetHP(npc->GetMaxHP());
		npc->SetPosition(npc->GetSpawnPosition());
		npc->SetActive(true);
		npc->SetupBT();

		// 2. 물리 및 그리드 복구
		if (auto cc = npc->GetComponent<GAME::CharacterControllerComponent>()) {
			cc->SetPhysicsActive(true);
			cc->SetPosition(npc->GetSpawnPosition());
		}
		_gridMap.Add(npc);

		for (auto& [pid, session] : _players)
		{
			common::Vec3 playerPos = session->_player->GetPosition();
			if (common::Distance(playerPos, npc->GetPosition()) <= 120.0f) {
				session->_viewedNpcs.insert(npc->GetNpcId());
				SendNpcSpawnToPlayer(session, npc);
			}
			else {
				session->_viewedNpcs.erase(npc->GetNpcId());
			}
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

		//CreatePhysicsTerrain();
		//CreatePhysicsMapObjects();
	}
}
