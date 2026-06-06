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
#include "DBManager.h"
#include "InventoryComponent.h"
#include "StageManager.h"
#include "Elevator.h"
#include "MagicGuard.h"
#include "QuestNPC.h"

namespace PIP::SERVER
{
	constexpr int MAX_ROOM_PLAYERS = 4;

	Room::Room(int room_id, int logic_thread_idx)
		: _room_id{ room_id }, _logic_thread_idx{ logic_thread_idx }, _max_players{ MAX_ROOM_PLAYERS }, _room_state{ RoomState::WAITING }
	{
		MYLOG("Room " << _room_id << " created. Assigned to Logic Thread " << _logic_thread_idx << " Max Players: " << static_cast<int>(_max_players));
	}

	GAME::Elevator* Room::spawn_elevator(const common::Vec3& start, const common::Vec3& end, float speed, float waitTime, const std::string& name)
	{
		int64_t id = _next_npc_id++;
		auto elevator = std::make_unique<GAME::Elevator>(id, start, end, speed, waitTime);
		elevator->SetName(name);

		// [수정] glTF min/max 기준 정밀한 박스 규격 적용 (약 4.67m x 0.36m x 4.67m)
		// Half-extents: (2.336f, 0.179f, 2.338f)
		JPH::BoxShapeSettings boxSettings(JPH::Vec3(2.336f, 0.179f, 2.338f));
		auto result = boxSettings.Create();
		if (result.IsValid()) {
			elevator->InitializePhysics(_physicsSystem, result.Get());
		}

		GAME::Elevator* ptr = elevator.get();
		_elevators.push_back(std::move(elevator));
		_actors[id] = ptr;

		MYLOG("[Room] Elevator spawned: " << name << " ID: " << id << " Pos: (" << start.x << ", " << start.y << ", " << start.z << ")");
		return ptr;
	}

	void Room::Initialize()
	{
		PhysicsInitialize();
		_gridMap.Initialize(-1000, 1000, -1000, 1000, 40);


		// [수정] MainStage에서 시작하여 10초 후 BossScene으로 이동하도록 설정
		_currentStage = StageManager::Instance()->create_stage("CastleStage");
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
				cc->Initialize(_physicsSystem, 1.5f, 0.5f);
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
		this->Broadcast(reinterpret_cast<const char*>(&leave_packet), sizeof(leave_packet), player_id, true);
		

		auto it = _players.find(player_id);
		if (it != _players.end()) {
			auto session = it->second;
			if (session) {
				if (session->_player) _gridMap.Remove(session->_player.get());
				session->_viewedNpcs.clear(); // [추가] 다음 방 입장을 위해 시야 목록 초기화
			}
			_actors.erase(player_id); // [추가] 통합 맵에서 제거
			_readyPlayers.erase(player_id); // [추가] 준비 목록에서 제거
			_players.erase(it);
		}

		// [추가] 누군가 나가서 남은 인원만으로 시작 조건을 만족하는지 체크
		if (_room_state == RoomState::WAITING && !_players.empty()) {
			CheckAndStartGame();
		}

		if (_players.empty()) {
			MYLOG("Last player left. Resetting all NPC AI states...");
			for (auto& [id, npc] : _npcs) {
				if (auto ai = npc->GetComponent<GAME::AIComponent>()) {
					auto bb = ai->GetBlackboard();
					bb->set("target_enemy", std::any()); // 타겟 삭제
					bb->set("target_pos", std::any());   // 배회 목적지 삭제
					bb->set("stuck_timer", 0.0f);        // 끼임 타이머 리셋
					// 필요 시 BT 자체를 새로 Setup (NPC::SetupBT 호출)
					npc->SetupBT();
				}
				npc->SetState(common::packet::EntityState::IDLE);
			}
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
	GAME::NPC* Room::spawn_npc(GAME::NPCType type, const std::string& name)
	{
		// 1. NPC 고유 ID 생성 (방 번호 기반으로 충돌 방지)
		int64_t npc_id = _next_npc_id + (_room_id * 10000LL) + _npcs.size();

		// 2. 타입별 객체 생성 (확장 포인트)
		std::unique_ptr<GAME::NPC> new_npc;
		float radius = 0.5f;
		float height = 1.8f;

		const NPCSpawnData* data = LuaManager::Instance()->GetNPCSpawnData(type, 0);

		switch (type)
		{
		case GAME::NPCType::Tainer:
			{
				new_npc = std::make_unique<GAME::Tainer>(npc_id, _room_id, data->pos);
				radius = 1.0f; height = 2.5f; // 보스 규격
				break;
			}
		case GAME::NPCType::Basic:
			{
				new_npc = std::make_unique<GAME::NPC>(npc_id, type, _room_id, data->pos, data->max_hp);
				break;
			}
		case GAME::NPCType::MagicGuard:
			{
				new_npc = std::make_unique<GAME::MagicGuard>(npc_id, _room_id, data->pos, data->max_hp);
				break;
			}
		case GAME::NPCType::QuestNPC:
			{
				new_npc = std::make_unique<GAME::QuestNPC>(npc_id, type, _room_id, data->pos, data->max_hp);
				break;
			}
		default:
			MYERROR("[Room] Attempted to spawn unknown NPC type: " << static_cast<int>(type));
			break;
		}

		// [추가] LuaManager에서 해당 위치의 데이터를 찾아와서 적용
		if (data)
		{
			new_npc->ApplySpawnData(*data);
		}
		else
		{
			// Lua에 데이터가 없는 경우(예: 수동 스폰) 기본값 설정
			new_npc->SetHP(100);
		}

		if (!name.empty()) new_npc->SetName(name);

		// 3. 물리 컨트롤러 초기화
		auto cc = new_npc->GetComponent<GAME::CharacterControllerComponent>();
		cc->Initialize(_physicsSystem, height, radius);

		// 4. [핵심] 안전한 위치 찾기 (지형 레이캐스트 + 건물 끼임 체크)
		JPH::Shape* npc_shape = (JPH::Shape*)cc->GetShape();
		common::Vec3 safe_pos = find_safe_spawn_position(data->pos, npc_shape);

		// 5. 확정된 위치로 물리 및 트랜스폼 설정
		new_npc->SetPosition(safe_pos);
		new_npc->SetLastUpdateTime(std::chrono::steady_clock::now());

		GAME::NPC* ptr = new_npc.get();
		AddNPC(std::move(new_npc));

		return ptr;
	}
	common::Vec3 Room::find_safe_spawn_position(const common::Vec3& pos, JPH::Shape* npc_shape)
	{
		common::Vec3 current_pos = pos;
		bool is_stuck = true;
		int attempts = 0;

		// 지형은 무시하고 건물/오브젝트만 체크하기 위한 필터 설정
		JPH::IgnoreMultipleBodiesFilter terrain_filter;
		for (auto id : _terrainBodyIDs) {
			terrain_filter.IgnoreBody(id);
		}

		while (is_stuck && attempts < 5)
		{
			// 1. [Raycast] 하늘에서 레이를 쏴서 바닥(지형) 높이 찾기
			JPH::RRayCast ray{ Utils::ToJolt(current_pos + common::Vec3(0, 50, 0)), JPH::Vec3(0, -100, 0) };
			JPH::RayCastResult ray_res;

			if (_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, ray_res,
				_physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::NPC),
				_physicsSystem->GetDefaultLayerFilter(Layers::NPC)))
			{
				current_pos.y = ray.mOrigin.GetY() + ray.mDirection.GetY() * ray_res.mFraction;
			}

			// 2. [CollideShape] 찾은 바닥 위치에서 건물과 겹치는지 체크
			JPH::CollideShapeSettings settings;
			JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;

			// 약간 위(0.9m)에서 체크 (발밑이 지형에 살짝 걸리는 것 방지)
			_physicsSystem->GetNarrowPhaseQuery().CollideShape(
				npc_shape, JPH::Vec3::sReplicate(1.0f),
				JPH::RMat44::sTranslation(Utils::ToJolt(current_pos) + JPH::Vec3(0, 0.9f, 0)),
				settings, JPH::RVec3::sZero(), collector,
				_physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::NPC),
				_physicsSystem->GetDefaultLayerFilter(Layers::NPC),
				terrain_filter
			);

			if (!collector.HadHit()) {
				is_stuck = false; // 안 끼었음!
			}
			else {
				// 끼었다면 근처로 무작위 이동 후 재시도
				current_pos.x += (rand() % 2 == 0 ? 3.0f : -3.0f);
				current_pos.z += (rand() % 2 == 0 ? 3.0f : -3.0f);
				attempts++;
			}
		}

		return current_pos;
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
		if (!StageManager::Instance()->is_existing_stage(nextSceneName)) {
			MYERROR("[Room] Attempted to change to non-existing scene: " << nextSceneName);
			return;
		}

		// 1. 기존 스테이지 정리 (NPC 및 물리 바디 제거)
		if (_currentStage) {
			_currentStage->on_exit(this);
		}

		WaitGame(); // [추가] 상태를 WAITING으로 변경하여 READY 체크가 정상 작동하게 함
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

		// [수정] _readyPlayers가 비워졌으므로 Broadcast 대신 전체 플레이어에게 직접 전송
		for (auto& [id, session] : _players) {
			session->do_send(stream.constable_data(), stream.Size());
		}
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

	void Room::ExecuteActorAction(GAME::Actor* attacker, const GAME::AttackConfig& config)
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
				// [추가] 죽은 대상은 공격하지 않음 (그랩 무한 루프 방지)
				if (target->GetHP() <= 0) continue;

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

						// [추가] 잡기 판정 처리 (타겟이 살아있고 아직 안 잡혔을 때만)
						if (config.isGrab && p->GetHP() > 0 && p->GetGrabbedById() != attacker->GetId()) {
							p->SetGrabbedById(attacker->GetId());
							p->SetState(common::packet::EntityState::GRABBED);
							MYLOG("[Grab] Player " << p->GetId() << " grabbed by " << attacker->GetId());
						}

						if (p->GetHP() <= 0) {
							auto it = _players.find(p->GetId());
							if (it != _players.end()) {
								OnPlayerDead(it->second); // 플레이어 사망 처리 호출
							}
						}
					}
					else if (auto n = dynamic_cast<GAME::NPC*>(target)) {
						npc_hits.emplace_back(n->GetNpcId(), (int32_t)config.damage, n->GetHP());

						// [수정] NPC 사망 처리 로직 추가
						if (n->GetHP() <= 0 && n->IsActive()) {
							OnNPCDead(n);
						}
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

		// 4. [패킷 전송] NPC가 맞았음을 알림
		for (const auto& hit : npc_hits) {
			packet::PacketStream stream;
			packet::SC_PACKET_NPC_ATTACK hit_packet;
			hit_packet._type = packet::PacketType::S2C_P_NPC_ATTACK;
			hit_packet._attacker_id = attacker->GetId();
			hit_packet._hit_count = 1; // 단일 전송 모드

			stream << hit_packet;
			stream << hit;

			auto* h = reinterpret_cast<packet::PacketHeader*>(stream.mutable_data());
			h->_size = static_cast<uint16_t>(stream.Size());

			// [핵심] 이 NPC를 시야에 둔 플레이어들에게만 브로드캐스트 (체력바 업데이트 보장)
			BroadcastToNPCViewers(hit._target_id, stream.constable_data(), stream.Size());
		}


	// 5. (디버깅) 서버 판정 범위를 시각화 패킷으로 전송
#ifdef _DEBUG
	// --- [디버그] 서버의 공격 판정 영역을 클라이언트에 가시화 ---
	{
		packet::SC_PACKET_DEBUG_DRAW debug;
		debug._type = packet::PacketType::S2C_P_DEBUG_DRAW;
		debug._size = sizeof(debug);
		debug._position = finalPos; // 계산된 공격 중심 월드 좌표
		debug._rotation = rot;      // 공격자의 월드 회전값
		debug._duration = 0.5f;     // 0.5초 동안만 표시

		// Jolt Shape의 실제 타입을 확인하여 디버그 데이터 추출
		const JPH::Shape* shape = config.shape.GetPtr();
		if (shape)
		{
			switch (shape->GetSubType())
			{
			case JPH::EShapeSubType::Sphere:
			{
				auto sphere = static_cast<const JPH::SphereShape*>(shape);
				debug._shape_type = packet::DebugShapeType::SPHERE;
				debug._extents = { sphere->GetRadius(), 0, 0 };
			}
			break;
			case JPH::EShapeSubType::Box:
			{
				auto box = static_cast<const JPH::BoxShape*>(shape);
				JPH::Vec3 halfExtents = box->GetHalfExtent();
				debug._shape_type = packet::DebugShapeType::BOX;
				debug._extents = { halfExtents.GetX(), halfExtents.GetY(), halfExtents.GetZ() };
			}
			break;
			case JPH::EShapeSubType::Capsule:
			{
				auto capsule = static_cast<const JPH::CapsuleShape*>(shape);
				debug._shape_type = packet::DebugShapeType::CAPSULE;
				debug._extents = { capsule->GetRadius(), capsule->GetHalfHeightOfCylinder(), 0 };
			}
			break;
			}
			// 방 안의 모든 플레이어에게 전송
			Broadcast(reinterpret_cast<const char*>(&debug), sizeof(debug));
		}
	}
#endif
	}

	void Room::StartGame()
	{
		if (_room_state == RoomState::PLAYING) return; // [추가] 이미 시작된 경우 중복 처리 방지

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
		// --- 4. Jolt 월드 시뮬레이션 (Static 지형 및 비-Actor 물리 객체용) ---
		// Actor들은 위에서 CharacterVirtual로 직접 제어했으므로,
		// 여기서는 정적인 장애물이나 동적 프롭들만 계산됩니다.
		_physicsSystem->Update(deltaTime, 1, tempAllocator, _jobSystem);

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
			
			// [추가] 잡힌 상태일 때는 물리 엔진 업데이트를 건너뜀 (보스 손 위치에 강제 고정 중이므로)
			if (player->GetState() == common::packet::EntityState::GRABBED) {
				_gridMap.UpdatePosition(player.get(), player->GetPosition());
				continue;
			}

			auto cc = player->GetComponent<GAME::CharacterControllerComponent>();
			if (!cc) continue;

			// [핵심] 물리 엔진 업데이트 (조작 의도 + 넉백 + 중력)
			player->PhysicsUpdate(deltaTime, tempAllocator);

			// [추가] 플레이어의 그리드맵 위치 갱신
			_gridMap.UpdatePosition(player.get(), player->GetPosition());
		}

		

		// [핵심 2] 매 프레임 그릴 때마다 임시로 레코더를 생성해서 넘깁니다.
#ifdef DEBUG_VIEWER
		if (_isSessionOpen && _captureNextFrame && _recorder)
		{

			JPH::BodyManager::DrawSettings drawSettings;
			drawSettings.mDrawBoundingBox = false;
			drawSettings.mDrawShape = true;
			drawSettings.mDrawShapeColor = JPH::BodyManager::EShapeColor::ShapeTypeColor;
			drawSettings.mDrawCenterOfMassTransform = true;
			drawSettings.mDrawVelocity = true;
			drawSettings.mDrawShapeWireframe = true;



			// [핵심 수정 1] 중괄호를 추가해서 recorder의 생명주기를 강제로 제한합니다.
			{
				JPH::DebugRendererRecorder recorder(*_streamOut);
				// 1. 기존의 월드 바디들(지형, 엘리베이터 등) 그리기
				_physicsSystem->DrawBodies(drawSettings, &recorder);

				// ----------------------------------------------------
				// 2. [추가] CharacterVirtual 수동 그리기
				// ----------------------------------------------------

				// [A] 플레이어 그리기 (초록색)
				for (auto& [pid, session] : _players) {
					if (!session || !session->_player) continue;

					auto pcc = session->_player->GetComponent<GAME::PlayerControllerComponent>();
					// 참고: _character 변수가 private라면 GetCharacter() 같은 Getter를 쓰셔야 합니다.
					if (pcc && pcc->GetCharacter()) {
						auto cv = pcc->GetCharacter();
						// 캐릭터의 현재 회전과 위치로 Jolt 트랜스폼 매트릭스 생성
						JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(cv->GetRotation(), cv->GetPosition());

						// Shape 객체의 Draw 함수에 recorder를 직접 넘겨서 그립니다.
						cv->GetShape()->Draw(&recorder, transform, JPH::Vec3::sReplicate(1.0f), JPH::Color::sGreen, false, true);
					}
				}

				// [B] NPC 그리기 (빨간색)
				for (auto* npc : _activeNpcList) {
					if (!npc || !npc->IsActive()) continue;

					auto nc = npc->GetComponent<GAME::CharacterControllerComponent>();
					if (nc && nc->GetCharacter()) {
						auto cv = nc->GetCharacter();
						JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(cv->GetRotation(), cv->GetPosition());

						cv->GetShape()->Draw(&recorder, transform, JPH::Vec3::sReplicate(1.0f), JPH::Color::sRed, false, true);
					}
				}
				recorder.EndFrame();
			} // <-- 이 괄호를 빠져나가면서 recorder가 정상적으로 소멸되고, 파일 쓰기가 깔끔하게 마무리됩니다.

			_captureNextFrame = false;
		}
#endif
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
		if (_players.empty()) return;

		// --- [Step 1] 활성 셀 및 NPC 수집 (최적화: dynamic_cast 제거) ---
		_activeNpcList.clear();
		_activeCellIndices.clear();

		// 1-1. 모든 플레이어 주변의 활성 셀 인덱스 수집
		for (auto& [pid, session] : _players) {
			if (!session || !session->_player || !_readyPlayers.contains(pid)) continue;

			common::Vec3 myPos = session->_player->GetPosition();
			int centerCell = _gridMap.GetCellIndex(myPos);
			_gridMap.GetNearbyCellIndices(centerCell, _activeCellIndices);

			// 플레이어 이동 동기화 (기존 로직)
			auto player = session->_player.get();
			if (player->IsDirty()) {
				packet::SC_PACKET_MOVE res;
				res = player->CreateMovePacket();
				Broadcast(reinterpret_cast<const char*>(&res), sizeof(res));
				player->SyncSentData();
			}
		}

		// 1-2. 활성 셀 인덱스 중복 제거
		std::sort(_activeCellIndices.begin(), _activeCellIndices.end());
		_activeCellIndices.erase(std::unique(_activeCellIndices.begin(), _activeCellIndices.end()), _activeCellIndices.end());

		// 1-3. 활성 셀의 NPC들만 수집
		for (int cellIdx : _activeCellIndices) {
			const auto& cellNpcs = _gridMap.GetNpcsInCell(cellIdx);
			for (auto* npc : cellNpcs) {
				if (npc->IsActive()) {
					_activeNpcList.push_back(npc);
				}
			}
		}
		// activeNpcList 중복 제거
		std::sort(_activeNpcList.begin(), _activeNpcList.end());
		_activeNpcList.erase(std::unique(_activeNpcList.begin(), _activeNpcList.end()), _activeNpcList.end());

		// 1-4. 보스 추가
		for (auto& [id, npc] : _npcs) {
			if (npc->is_boss() && npc->IsActive()) {
				_activeNpcList.push_back(npc.get());
			}
		}
		std::sort(_activeNpcList.begin(), _activeNpcList.end());
		_activeNpcList.erase(std::unique(_activeNpcList.begin(), _activeNpcList.end()), _activeNpcList.end());

		// 1-5. 플레이어별 AOI 진입/이탈 체크
		for (auto& [pid, session] : _players) {
			if (!session || !session->_player || !_readyPlayers.contains(pid)) continue;

			int centerCell = _gridMap.GetCellIndex(session->_player->GetPosition());
			_playerNearbyCells.clear();
			_gridMap.GetNearbyCellIndices(centerCell, _playerNearbyCells);

			_currentViewedIds.clear();

			for (int cellIdx : _playerNearbyCells) {
				for (auto* npc : _gridMap.GetNpcsInCell(cellIdx)) {
					if (!npc->IsActive()) continue;
					
					int64_t nid = npc->GetId();
					_currentViewedIds.push_back(nid);

					if (!session->_viewedNpcs.contains(nid)) {
						session->_viewedNpcs.insert(nid);
						SendNpcSpawnToPlayer(session, npc);
					}
				}
			}

			// 이탈 체크 (보스 제외)
			std::sort(_currentViewedIds.begin(), _currentViewedIds.end());
			for (auto it = session->_viewedNpcs.begin(); it != session->_viewedNpcs.end(); ) {
				int64_t nid = *it;
				auto npcIt = _npcs.find(nid);
				if (npcIt == _npcs.end()) {
					it = session->_viewedNpcs.erase(it);
					continue;
				}

				if (!npcIt->second->is_boss() && !std::binary_search(_currentViewedIds.begin(), _currentViewedIds.end(), nid)) {
					SendNpcLeaveToPlayer(session, nid);
					it = session->_viewedNpcs.erase(it);
				}
				else {
					++it;
				}
			}
		}

		// --- [Step 2] 로직 업데이트 루프 (O(Active NPCs)) ---
		uint32_t currentTick = static_cast<uint32_t>(GetTickCount64());

		// [추가] 엘리베이터 업데이트 및 동기화
		for (auto& elevator : _elevators) {
			elevator->Update(deltaTime, tempAllocator);

			// 엘리베이터 이동 패킷 전송 (NPC 패킷 재사용)
			packet::SC_PACKET_NPC_MOVE move_pkt;
			move_pkt._type = packet::PacketType::S2C_P_NPC_MOVE;
			move_pkt._npc_id = elevator->GetId();
			move_pkt._position = elevator->GetPosition();
			move_pkt._velocity = elevator->GetVelocity();
			move_pkt._rotation = elevator->GetRotation();
			move_pkt._state = elevator->GetState();
			move_pkt._action_id = 0;
			move_pkt._time_stamp = currentTick;

			packet::PacketStream stream;
			stream << move_pkt;
			stream << elevator->GetName();

			auto* h = reinterpret_cast<packet::PacketHeader*>(stream.mutable_data());
			h->_size = (uint16_t)stream.Size();

			Broadcast(stream.constable_data(), stream.Size());
		}

		for (GAME::NPC* npc : _activeNpcList) {
			// [최적화] 거리 기반 AI 스태거링
			bool skipAI = false;
			if (!npc->is_boss() && npc->GetState() != common::packet::EntityState::ACTION && npc->GetState() != common::packet::EntityState::HITTED) {
				float minDistSq = 10000.0f; 
				for (auto& [pid, session] : _players) {
					if (!session || !session->_player) continue;
					float d2 = common::DistanceSq(npc->GetPosition(), session->_player->GetPosition());
					if (d2 < minDistSq) minDistSq = d2;
				}
				if (minDistSq > 1600.0f && (currentTick % 3 != 0)) {
					skipAI = true;
				}
			}

			if (!skipAI) {
				npc->Update(deltaTime, tempAllocator);
			}

			common::Vec3 pos = npc->GetPosition();
			if (std::isnan(pos.x)) { // 약식 NaN 체크
				pos = { 10.0f, 10.0f, 10.0f };
				npc->SetPosition(pos);
			}

			common::Vec3 vel = npc->GetVelocity();
			if (vel.x * vel.x + vel.z * vel.z > 0.01f) {
				float angle = std::atan2(vel.x, vel.z);
				DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(0, angle, 0);
				common::Quat rot;
				XMStoreFloat4((XMFLOAT4*)&rot, q);
				npc->SetRotation(rot);
			}

			npc->SetLastUpdateTime(std::chrono::steady_clock::now());
			npc->RecordSnapshot(currentTick);
		}

		// --- [Step 3] 패킷 전송 및 클린업 (기존 로직) ---
		_npcSyncTimer += deltaTime;
		if (_npcSyncTimer >= 0.05f) {
			BroadcastNpcBatch(); // 여기서도 _activeNpcList를 활용하게 고치면 더 좋습니다!
			_npcSyncTimer = 0.0f;
		}

		// 플레이어 업데이트 및 스냅샷 기록
		currentTick = static_cast<uint32_t>(GetTickCount64());
		for (auto& [pid, session] : _players) {
			session->_player->Update(deltaTime, tempAllocator);
			session->_player->RecordSnapshot(currentTick);
		}

		// 5. 스테이지 전용 업데이트 (보스 페이즈, 트리거 등)
		if (_currentStage) {
			_currentStage->update(this, deltaTime);
		}

		// --- [추가] DB 자동 저장 타이머 ---
		static float saveTimer = 0.0f;
		saveTimer += deltaTime;

		if (saveTimer >= 10.0f) { // 10초마다 자동 저장 체크
			for (auto& [id, session] : _players) {
				if (!session || !session->_player) continue;

				auto player = session->_player;
				auto inven = player->GetComponent<GAME::InventoryComponent>();

				// 수정된 사항이 있다면 DB로 작업을 던짐
				if (inven && inven->is_dirty()) {

					DBTask task;
					task.type = DBTaskType::SAVE_INVENTORY_ALL;
					task.session_id = player->GetId();
					task.logic_thread_idx = _logic_thread_idx; // 내 로직 스레드 인덱스 기록

					// 1. 인벤토리 스냅샷 생성 (값 복사 발생)
					InventorySnapshot snapshot;
					snapshot.materials = inven->get_materials_snapshot();
					snapshot.equipments = inven->get_equipments_snapshot();

					// 2. std::any에 구조체 할당 (이동 시맨틱 적용)
					task.data = std::make_any<InventorySnapshot>(std::move(snapshot));

					// 3. DB 작업 완료 후 로직 스레드에서 실행될 콜백 지정
					task.callback = [player]() {
						// 주의: 비동기 콜백이므로 player 포인터 유효성 검사가 필요할 수 있습니다.
						// 여기서는 컴포넌트를 가져오면서 유효성을 한 번 체크합니다.
						if (!player)
						{
							return;
						}
						if (auto inv = player->GetComponent<GAME::InventoryComponent>()) {
							inv->mark_saved(); // 저장이 성공했으므로 dirty 플래그 해제
							// MYLOG("[Logic] Player " << player->GetId() << " Inventory Dirty flag cleared.");
						}
					};

					// 4. DB 스레드로 Task 푸시
					DBManager::Instance()->push_task(std::move(task));
				}
			}
			saveTimer = 0.0f;
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
		// [추가] 잡기 정보 동기화
		// NPC가 잡혔을 가능성은 낮지만 일관성을 위해 추가
		// (혹은 NPC가 플레이어를 잡고 있는 상태에서 플레이어의 시점에서는 NPC의 위치가 중요하므로)

		packet::PacketStream finalStream;
		finalStream << move_packet_data;
		finalStream << npc->GetName();

		auto* final_header = reinterpret_cast<packet::PacketHeader*>(finalStream.mutable_data());
		final_header->_size = static_cast<uint16_t>(finalStream.Size());

		Broadcast(finalStream.constable_data(), finalStream.Size());
	}

	

	void Room::Broadcast(const char* data, size_t size, int64_t except_id, bool force)
	{
		for (auto& pair : _players)
		{
			if (pair.first == except_id) continue;

			// [수정] force가 true면 준비 여부와 상관없이 전송 (시스템 패킷 등)
			if (!force && !_readyPlayers.contains(pair.first)) continue;

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

		// 1. 셀 단위 미리 직렬화 (O(N))
		_cellMoveBuffers.clear();
		_dirtyNPCs.clear();

		// activeNpcList는 이미 UpdateLogics에서 dynamic_cast 없이 수집됨
		for (auto* npc : _activeNpcList) {
			if (!npc->IsDirty()) continue;
			_dirtyNPCs.push_back(npc);

			common::Vec3 pos = npc->GetPosition();
			if (std::isnan(pos.x)) continue;

			int cellIdx = _gridMap.GetCellIndex(pos);
			auto& buffer = _cellMoveBuffers[cellIdx];

			packet::NPCMoveData data;
			data._npc_id = npc->GetNpcId();
			data._position = pos;
			data._velocity = npc->GetVelocity();
			data._rotation = npc->GetRotation();
			data._time_stamp = static_cast<uint32_t>(GetTickCount64());
			data._state = npc->GetState();
			data._action_id = npc->GetActionId();
			data._grabbed_by_id = npc->GetGrabbedById(); // [추가]
			data._grab_slot = npc->GetGrabSlot();         // [추가]
			data._hp = npc->GetHP();                     // [추가]

			const char* pData = reinterpret_cast<const char*>(&data);
			buffer.insert(buffer.end(), pData, pData + sizeof(packet::NPCMoveData));
		}

		if (_dirtyNPCs.empty()) return;

		// 2. 플레이어별 전송 (O(P * 9))
		for (auto& [pid, session] : _players)
		{
			if (!session || !session->_player || !_readyPlayers.contains(pid)) continue;

			int centerCell = _gridMap.GetCellIndex(session->_player->GetPosition());
			_playerNearbyCells.clear();
			_gridMap.GetNearbyCellIndices(centerCell, _playerNearbyCells);

			packet::PacketStream stream;
			packet::SC_PACKET_NPC_MOVE_BATCH header;
			header._type = packet::PacketType::S2C_P_NPC_MOVE_BATCH;
			header._count = 0;
			stream << header;

			int totalNpcCount = 0;
			for (int cellIdx : _playerNearbyCells) {
				auto it = _cellMoveBuffers.find(cellIdx);
				if (it == _cellMoveBuffers.end()) continue;

				const auto& buffer = it->second;
				int npcCountInCell = static_cast<int>(buffer.size() / sizeof(packet::NPCMoveData));
				
				if (stream.Size() + buffer.size() > 4000) {
					if (totalNpcCount > 0) {
						auto* h = reinterpret_cast<packet::SC_PACKET_NPC_MOVE_BATCH*>(stream.mutable_data());
						h->_count = totalNpcCount;
						h->_size = (uint16_t)stream.Size();
						session->do_send(stream.constable_data(), stream.Size());
						
						stream.Clear();
						stream << header;
						totalNpcCount = 0;
					}
				}

				stream.Write(buffer.data(), buffer.size());
				totalNpcCount += npcCountInCell;
			}

			if (totalNpcCount > 0) {
				auto* h = reinterpret_cast<packet::SC_PACKET_NPC_MOVE_BATCH*>(stream.mutable_data());
				h->_count = totalNpcCount;
				h->_size = (uint16_t)stream.Size();
				session->do_send(stream.constable_data(), stream.Size());
			}
		}

		// 3. 클린업
		for (auto* npc : _dirtyNPCs) npc->SyncSentData();
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

	/*void Room::HandleAttack(const std::shared_ptr<SESSION>& attacker) {
		//레거시 삭제
	}*/
	
	void Room::Execute_C2S_NPC_INTERACT(const std::shared_ptr<SESSION>& session, const common::packet::CS_PACKET_NPC_INTERACT& interact_packet)
	{
		if (!session || !session->_player) return;
		int64_t npc_id = interact_packet._npc_id;
		int32_t quest_id = interact_packet._quest_id;

		GAME::NPC* npc = GetNPC(npc_id);
		if (!npc) return; // NPC가 방에 없음

		// 거리 검사 (예: 반경 5m 이내)
		if (common::DistanceSq(session->_player->GetPosition(), npc->GetPosition()) > 25.0f) {
			MYLOG("[Quest] Interaction failed. NPC too far.");
			return;
		}
		common::packet::QuestUpdateInfo info;
		if (quest_id != 0) {
			auto quest = session->_player->GetQuest(quest_id);
			if (!quest) {
				// 퀘스트 수락
				info = session->_player->AddQuest(quest_id);
				if (info._state == packet::QuestState::NONE)
				{
					MYERROR("[Quest] Failed to add quest " << quest_id << " for session " << session->_id);
					return;
				}
				SendQuestUpdate(session, info);
				MYLOG("[Quest] Session " << session->_id << " accepted quest " << quest_id);
			} else {
				// 퀘스트 완료 처리 (조건 만족 시)
				if (quest->_state == common::packet::QuestState::COMPLETED) {
					info = session->_player->CompleteQuest(quest_id);
					if (info._state == packet::QuestState::NONE)
					{
						MYERROR("[Quest] Failed to complete quest " << quest_id << " for session " << session->_id);
						return;
					}
					SendQuestUpdate(session, info);
					MYLOG("[Quest] Session " << session->_id << " completed quest " << quest_id);
				}
			}
		} else if (npc->GetNpcType() == common::packet::NPCType::Lever) {
			// [신규] 레버 상호작용
			if (!_activatedLevers.contains(npc_id)) {
				_activatedLevers.insert(npc_id);
				MYLOG("[Room " << _room_id << "] Lever activated! Total: " << _activatedLevers.size());
				
				if (_activatedLevers.size() == 2) {
					// 레버 2개 작동 완료 -> 컷씬 재생 패킷 브로드캐스트
					MYLOG("[Room " << _room_id << "] Both levers activated. Broadcasting PLAY_CUTSCENE.");
					packet::PacketStream stream;
					packet::SC_PACKET_PLAY_CUTSCENE pkt;
					pkt._type = packet::PacketType::S2C_P_PLAY_CUTSCENE;
					pkt._size = sizeof(pkt);
					pkt._cutscene_id = 1; // 예: 1번 컷씬 (보스 진입 전)
					stream << pkt;
					Broadcast(stream.constable_data(), stream.Size());
					
					// 컷씬 상태로 전환
					_room_state = RoomState::WAITING;
					_cutsceneFinishedPlayers.clear();
				}
			}
		} else {
			// quest_id가 0인 경우 해당 NPC가 주는 기본 퀘스트를 처리 (임시 하드코딩 - 1번 퀘스트)
			int32_t default_quest_id = 1; 
			auto quest = session->_player->GetQuest(default_quest_id);
			if (!quest) {
				info = session->_player->AddQuest(default_quest_id);
				if (info._state == packet::QuestState::NONE)
				{
					MYERROR("[Quest] Failed to add quest " << quest_id << " for session " << session->_id);
					return;
				}
				SendQuestUpdate(session, info);
				MYLOG("[Quest] Session " << session->_id << " accepted default quest " << default_quest_id);
			} else if (quest->_state == common::packet::QuestState::COMPLETED) {
				info = session->_player->CompleteQuest(default_quest_id);
				if (info._state == packet::QuestState::NONE)
				{
					MYERROR("[Quest] Failed to complete quest " << quest_id << " for session " << session->_id);
					return;
				}
				SendQuestUpdate(session, info);
				MYLOG("[Quest] Session " << session->_id << " completed default quest " << default_quest_id);
			}
		}
	}

	void Room::Execute_C2S_CUTSCENE_DONE(const std::shared_ptr<SESSION>& session)
	{
		// [디버깅용 임시 주석처리] 원래는 레버를 통해 WAITING 상태가 되어야만 처리되지만, 
		// 당장 레버 없이 F9만 눌러서 씬 전환을 테스트해볼 수 있도록 주석 처리해둡니다.
		// if (_room_state != RoomState::WAITING) return; 

		_cutsceneFinishedPlayers.insert(session->_id);
		MYLOG("[Room " << _room_id << "] Session " << session->_id << " finished cutscene. " 
			<< _cutsceneFinishedPlayers.size() << "/" << _players.size());

		// 모든 플레이어가 컷씬 시청 완료 시 보스 씬으로 전환
		if (_cutsceneFinishedPlayers.size() == _players.size() && !_players.empty()) {
			MYLOG("[Room " << _room_id << "] All players finished cutscene! Transitioning to BossStage.");
			_activatedLevers.clear(); // 상태 초기화
			ChangeScene("BossStage");
		}
		else if (_players.empty())
		{
			MYLOG("[Room " << _room_id << "] No players in room after cutscene. Transitioning to BossStage.");
			_activatedLevers.clear(); // 상태 초기화
			ChangeScene("BossStage");
		}
	}

	void Room::Execute_C2S_ACTION(const std::shared_ptr<SESSION>& session,
	                              const common::packet::CS_PACKET_ACTION& action_packet)
	{
		if (!session || !session->_player) return;

		std::vector<packet::NPCHitInfo> npc_hits;
		std::vector<packet::PlayerHitInfo> player_hits;

		// TODO: [Action] 공격 모션 알림 (공격자 중심 AOI)
		// SC_PACKET_ACTION_NOTIFY(actor_id, action_type, direction) 패킷을 정의하고 
		// 공격자(actor_id)를 시야에 둔 유저들에게 전송하여 애니메이션을 동기화해야 함.

		switch (action_packet._action_id)
		{
		case packet::ActionID::Common::Attack:
			{
				// 기본 공격 통합 구현
				GAME::AttackConfig config;
				config.damage = (float)session->_player->_damage;
				config.posOffset = { 0.0f, 0.0f, 1.0f }; // 플레이어 약간 앞(1m) 중심
				config.knockbackValue = 5.0f;           // 기본 공격의 가벼운 넉백

				// 3m 반경 구체 히트박스 생성
				JPH::SphereShapeSettings shapeSettings(2.0f);
				JPH::Shape::ShapeResult result = shapeSettings.Create();
				if (result.IsValid()) {
					config.shape = result.Get();
				}

				// 공용 로직으로 판정 및 브로드캐스트 위임
				ExecuteActorAction(session->_player.get(), config);
			}
			break;
		case packet::ActionID::Common::SKILL1:
			{
				// 대검 찍기 스킬 (SKILL1) 구현
				// 요구사항: 플레이어 앞 30cm, 3m 크기의 박스, 높은 데미지 판정

				GAME::AttackConfig config;
				config.damage = session->_player->_damage * 3.0f; // 기본 데미지의 3배 (강력한 일격)
				config.posOffset = { 0.0f, 1.0f, 3.8f };          // 플레이어 앞 0.3m + 박스 반경 1.5m
				config.knockbackValue = 30.0f;                    // 대검의 중량감을 살린 넉백

				// Jolt Box Shape 생성 (3m x 3m x 3m)
				// Jolt의 BoxShape는 half-extents를 사용하므로 1.5f를 입력합니다.
				JPH::BoxShapeSettings shapeSettings(JPH::Vec3(1.5f, 1.5f, 5.5f));
				JPH::Shape::ShapeResult result = shapeSettings.Create();
				if (result.IsValid()) {
					config.shape = result.Get();
				}

				// 서버 권위 판정: ExecuteActorAction을 통해 통합 처리 (팀킬 방지, 넉백, 패킷 브로드캐스트 포함)
				ExecuteActorAction(session->_player.get(), config);

				MYLOG("Executed SKILL1 (Greatsword Slam) for player session: " << session->_id);
			}
			break;		
		default:
			MYERROR("Unknown action type received: " << static_cast<int>(action_packet._action_id));
			return;
			break;
		}

		//// --- 3. [AOI 브로드캐스트] 피격 결과 전송 ---
		//// 모든 유저에게 쏘는 것이 아니라, "맞은 놈을 보고 있는 유저"에게만 전송합니다.

		//// NPC 피격 알림
		//for (const auto& hit : npc_hits) {
		//	packet::PacketStream stream;
		//	packet::SC_PACKET_NPC_ATTACK hit_packet;
		//	hit_packet._type = packet::PacketType::S2C_P_NPC_ATTACK;
		//	hit_packet._attacker_id = session->_id;
		//	hit_packet._hit_count = 1; // 단일 전송 모드

		//	stream << hit_packet;
		//	stream << hit;

		//	auto* h = reinterpret_cast<packet::PacketHeader*>(stream.mutable_data());
		//	h->_size = static_cast<uint16_t>(stream.Size());

		//	// [핵심] 이 NPC를 시야에 둔 플레이어들에게만 브로드캐스트
		//	BroadcastToNPCViewers(hit._target_id, stream.constable_data(), stream.Size());
		//}


		//// 2. 하단에 플레이어 피격 알림 브로드캐스트 추가
		//if (!player_hits.empty()) {
		//	packet::PacketStream stream;
		//	packet::SC_PACKET_PLAYER_ATTACK header;
		//	header._type = packet::PacketType::S2C_P_PLAYER_ATTACK;
		//	header._attacker_id = session->_id;
		//	header._hit_count = (uint8_t)player_hits.size();
		//	stream << header;
		//	for (auto& h : player_hits) stream << h;

		//	auto* h_ptr = reinterpret_cast<packet::PacketHeader*>(stream.mutable_data());
		//	h_ptr->_size = (uint16_t)stream.Size();
		//	Broadcast(stream.constable_data(), stream.Size());
		//}

		

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

		if (moveDist < common::move_speed::one_frame_max_speed) { // 정상 범위 이내일 때 (클라이언트와 서버의 순간적인 거리 차이가 one_frame_max_speed 이하여야 함)
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

		// 위치는 데드 레코닝 보정을 위해 저장해두고,
		// 패킷 에코 시 RTT 계산을 위해 client_tick 저장
		player->SetLastClientTick(move_packet._client_tick);
		player->SetRotation(move_packet._rotation);

		// [핵심 보정] 서버 권위 상태 보호 (네트워크 지연으로 인한 덮어쓰기 방지)
		auto currentState = player->GetState();
		if (currentState != common::packet::EntityState::GRABBED &&
			currentState != common::packet::EntityState::DEAD &&
			currentState != common::packet::EntityState::HITTED)
		{
			// 클라이언트가 임의로 특수 상태를 보내는 것도 방지
			if (move_packet._state != common::packet::EntityState::GRABBED &&
				move_packet._state != common::packet::EntityState::DEAD &&
				move_packet._state != common::packet::EntityState::HITTED)
			{
				player->SetState(move_packet._state);
				player->SetActionId(move_packet._action_id);
			}
		}

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

		// 방에 있는 모든 플레이어가 로딩을 마쳤는지 확인하고 시작 로직 실행
		CheckAndStartGame();
	}

	void Room::SetupPlayerSpawn(const std::shared_ptr<SESSION>& session) {
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

		// [추가] 현재 방에 있는 엘리베이터 정보 전송
		for (auto& elevator : _elevators) {
			packet::SC_PACKET_NPC_SPAWN elevator_spawn;
			elevator_spawn._type = common::packet::PacketType::S2C_P_NPC_SPAWN;
			elevator_spawn._npc_id = elevator->GetId();
			elevator_spawn._npc_type = common::packet::NPCType::Elevator;
			elevator_spawn._position = elevator->GetPosition();
			elevator_spawn._hp = elevator->GetHP();
			elevator_spawn._state = elevator->GetState();
			elevator_spawn._action_id = 0;

			packet::PacketStream elevatorStream;
			elevatorStream << elevator_spawn;
			elevatorStream << elevator->GetName();

			auto* h = reinterpret_cast<packet::PacketHeader*>(elevatorStream.mutable_data());
			h->_size = (uint16_t)elevatorStream.Size();

			session->do_send(elevatorStream.constable_data(), elevatorStream.Size());
		}

		int64_t bossCount = 0;
		for (auto& [npc_id, npc] : _npcs) {
			if (npc->is_boss())
			{
				bossCount++;
			}
		}

		// DW추가 : npc 카운트 패킷 전송 (방 입장 시 NPC 수 알려주기)
		packet::SC_PACKET_SCENE_AWAKE npc_count_packet;
		npc_count_packet._type = packet::PacketType::S2C_P_NPC_COUNT;
		npc_count_packet._size = sizeof(npc_count_packet);
		npc_count_packet._boss_count = bossCount; // 보스 마리 수
		npc_count_packet._boss_start_id = _next_npc_id + (_room_id * 1000) + 999; // 보스 ID
		npc_count_packet._npc_count = static_cast<uint16_t>(_npcs.size()) - npc_count_packet._boss_count;
		npc_count_packet._npc_start_id = _next_npc_id + (_room_id * 1000); // 일반 NPC ID 시작 인덱스 번호
		session->do_send(reinterpret_cast<char*>(&npc_count_packet), sizeof(npc_count_packet));

		// 4. 방에 있는 다른 사람들에게 나의 등장을 알림 (브로드캐스트)
		// 주의: EnterPlayer() 호출 전이므로, Broadcast는 수동으로 session->_id를 제외하거나 포함하여 처리
		Broadcast(self_spawn.constable_data(), self_spawn.Size(), session->_id);
#ifdef _DEBUG
		// 5. 기타 환경 정보(디버그 드로 등) 전송
		//SendMapDebugDraw(session);
		/*auto finded_convexs = MapDataManager::Instance()->get_find_mesh();
		for (const auto& mesh : finded_convexs)
		{
			SendDebugShape(session, mesh);
		}*/
#endif

	}

	void Room::SendFullInventory(const std::shared_ptr<SESSION>& session)
	{
		if (!session || !session->_player) return;

		auto inven = session->_player->GetComponent<GAME::InventoryComponent>();
		if (!inven) return;

		packet::PacketStream stream;
		packet::SC_PACKET_INVENTORY_INFO pkt;
		pkt._type = packet::PacketType::S2C_P_INVENTORY_ALL_INFO;

		auto mats = inven->get_materials_snapshot();
		auto equips = inven->get_equipments_snapshot();

		pkt._material_count = static_cast<uint16_t>(mats.size());
		pkt._equip_count = static_cast<uint16_t>(equips.size());
		stream << pkt;

		// 재료 직렬화
		for (const auto& [id, count] : mats) {
			stream << static_cast<uint32_t>(id) << count;
		}
		// 장비 직렬화
		for (const auto& [uid, equip] : equips) {
			stream << equip;
		}

		auto* h = reinterpret_cast<packet::PacketHeader*>(stream.mutable_data());
		h->_size = static_cast<uint16_t>(stream.Size());

		session->do_send(stream.constable_data(), stream.Size());
	}

	void Room::SendItemUpdate(const std::shared_ptr<SESSION>& session, packet::ItemId id, uint32_t amount,
		common::packet::InventoryUpdateType type)
	{
		if (!session) return;

		packet::SC_PACKET_ITEM_UPDATE pkt;
		pkt._type = packet::PacketType::S2C_P_ITEM_UPDATE;
		pkt._size = sizeof(pkt);
		pkt._update_type = type;
		pkt._item_id = static_cast<uint32_t>(id);
		pkt._amount = amount;

		session->do_send(reinterpret_cast<const char*>(&pkt), sizeof(pkt));
	}

	void Room::SendEquipUpdateBroadcast(int64_t player_id, const packet::EquipItem& equip)
	{
		// 1. 패킷 생성 및 데이터 채우기
		packet::SC_PACKET_EQUIP_UPDATE pkt;
		pkt._type = packet::PacketType::S2C_P_EQUIP_ITEM_UPDATE;
		pkt._size = sizeof(pkt);
		pkt._player_id = player_id;
		pkt._equip_data = equip;

		// 2. 브로드캐스트 실행
		// 기본적으로 방 안의 모든 사람에게 알리거나, 
		// 나중에 성능 최적화가 필요하면 시야 범위(GridMap) 내 유저들에게만 보낼 수 있습니다.
		Broadcast(reinterpret_cast<const char*>(&pkt), sizeof(pkt), -1, true);

		MYLOG("[Room] Broadcast EquipUpdate: Player " << player_id << " equipped ItemID " << static_cast<uint32_t>(equip.item_id));
	}

	void Room::SendQuestUpdate(const std::shared_ptr<SESSION>& session, const common::packet::QuestUpdateInfo& info)
	{
		if (!session) return;
		packet::SC_PACKET_QUEST_UPDATE quest_update_pkt;
		quest_update_pkt._type = packet::PacketType::S2C_P_QUEST_UPDATE;
		quest_update_pkt._size = sizeof(quest_update_pkt);
		quest_update_pkt._quest_info = info;
		session->do_send(reinterpret_cast<const char*>(&quest_update_pkt), sizeof(quest_update_pkt));
	}

	GAME::Player* Room::GetPlayer(int64_t player_id)
	{
		if (_players.contains(player_id))
		{
			return _players[player_id]->_player.get();
		}
		return nullptr;
	}

	void Room::GetNPCTypeName(common::packet::NPCType type, std::string& npcTypeName)
	{
		switch (type)
		{
		case common::packet::NPCType::Basic: npcTypeName = "Basic"; break;
		case common::packet::NPCType::Tainer: npcTypeName = "Tainer"; break;
		case common::packet::NPCType::MagicGuard: npcTypeName = "MagicGuard"; break;
		case common::packet::NPCType::Elevator: npcTypeName = "Elevator"; break;
		case common::packet::NPCType::QuestNPC: npcTypeName = "QuestNPC"; break;
		default: npcTypeName = "Basic"; break;
		}
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

	void Room::GetShapeTriangles(const JPH::Shape* inShape, std::vector<common::Vec3>& outTriangles)
	{
		if (!inShape) return;

		// 1. 모든 리프(Leaf) 쉐이프를 수집하기 위한 컬렉터
		JPH::AllHitCollisionCollector<JPH::TransformedShapeCollector> collector;
		JPH::SubShapeIDCreator id_creator;

		// 2. 계층 구조 순회 (ScaledShape, CompoundShape 등을 모두 분해하여 리프 노드 추출)
		// 인자: Box, Position, Rotation, Scale, IDCreator, Collector, Filter
		inShape->CollectTransformedShapes(
			JPH::AABox::sBiggest(),
			JPH::Vec3::sZero(),
			JPH::Quat::sIdentity(),
			JPH::Vec3::sReplicate(1.0f),
			id_creator,
			collector,
			JPH::ShapeFilter()
		);

		// 3. 수집된 각 리프(TransformedShape)에서 삼각형 추출
		for (const JPH::TransformedShape& ts : collector.mHits)
		{
			// TransformedShape의 mShape는 반드시 리프 노드임이 보장됩니다.
			JPH::Shape::GetTrianglesContext ctx;

			// ts 내부의 트랜스폼 정보를 사용하여 삼각형 추출 시작
			JPH::Vec3Arg scale = {ts.mShapeScale.x, ts.mShapeScale.y, ts.mShapeScale.z};
			ts.mShape->GetTrianglesStart(ctx, JPH::AABox::sBiggest(), ts.mShapePositionCOM, ts.mShapeRotation, scale);

			while (true) {
				const int max_tris = 64;
				JPH::Float3 vertices[max_tris * 3];
				int count = ts.mShape->GetTrianglesNext(ctx, max_tris, vertices);
				if (count == 0) break;

				for (int i = 0; i < count; ++i) {
					outTriangles.push_back({ vertices[i * 3 + 0].x, vertices[i * 3 + 0].y, vertices[i * 3 + 0].z });
					outTriangles.push_back({ vertices[i * 3 + 1].x, vertices[i * 3 + 1].y, vertices[i * 3 + 1].z });
					outTriangles.push_back({ vertices[i * 3 + 2].x, vertices[i * 3 + 2].y, vertices[i * 3 + 2].z });
				}
			}
		}
	}


	//void Room::CreatePhysicsStaticMeshCollisions()
	//{
	//	// 방마다 개별적으로 존재하는 물리 시스템의 BodyInterface
	//	auto& body_interface = _physicsSystem->GetBodyInterface();

	//	// 전역적으로 관리되는 Shape 리스트 가져오기
	//	const auto& shared_shapes = MapDataManager::Instance()->GetStaticMeshTiles();

	//	for (const auto& tile : shared_shapes)
	//	{
	//		// 동일한 Shape을 참조하여 각 방에 맞는 Body 생성
	//		JPH::BodyCreationSettings settings(
	//			tile.shape,
	//			JPH::RVec3::sZero(),
	//			JPH::Quat::sIdentity(),
	//			JPH::EMotionType::Static,
	//			Layers::NON_MOVING
	//		);

	//		// 실제 바디 생성 (메모리에는 Shape 데이터가 중복되지 않음!)
	//		body_interface.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
	//	}
	//}

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

	void Room::SendDebugShape(const std::shared_ptr<SESSION>& session, const StaticMeshTile& tile)
	{
		if (!tile.shape) return;

		std::vector<common::Vec3> allVertices;
		GetShapeTriangles(tile.shape, allVertices);

		if (allVertices.empty()) return;

		// 정점 개수가 3의 배수가 아닐 경우를 대비한 안전 장치
		size_t totalVertices = (allVertices.size() / 3) * 3;

		// 한 패킷당 최대 300개 정점 (100개 삼각형)씩 전송
		const size_t verticesPerPacket = 300;

		common::Vec3 worldPos = PIP::Utils::FromJolt(tile.position + (tile.rotation * tile.shape->GetCenterOfMass()));
		common::Quat worldRot = PIP::Utils::FromJolt(tile.rotation);

		for (size_t i = 0; i < totalVertices; i += verticesPerPacket)
		{
			// 남은 정점 개수 계산
			size_t currentBatchVertices = (std::min)(verticesPerPacket, totalVertices - i);
			uint32_t currentTriangleCount = static_cast<uint32_t>(currentBatchVertices / 3);

			uint16_t packetSize = static_cast<uint16_t>(sizeof(packet::SC_PACKET_DEBUG_SHAPE) + (currentBatchVertices *
				sizeof(common::Vec3)));
			std::vector<char> buffer(packetSize);

			auto* header = reinterpret_cast<packet::SC_PACKET_DEBUG_SHAPE*>(buffer.data());
			header->_size = packetSize;
			header->_type = packet::PacketType::S2C_P_DEBUG_SHAPE;
			header->_triangle_count = currentTriangleCount;
			header->_position = worldPos;
			header->_rotation = worldRot;

			// [수정] 벡터 범위를 초과하지 않도록 안전하게 복사
			memcpy(buffer.data() + sizeof(packet::SC_PACKET_DEBUG_SHAPE), &allVertices[i], currentBatchVertices *
				sizeof(common::Vec3));

			session->do_send(buffer.data(), packetSize);
		}

		MYLOG("[Debug] Sent " << totalVertices << " triangles in chunks for mesh: " << tile.meshName);
	}

	void Room::OnNPCDead(GAME::NPC* npc)
	{
		int64_t npcId = npc->GetId();
		
		// [추가] 퀘스트 킬 카운트 업데이트
		std::string npcTypeName = "";
		switch (npc->GetNpcType()) {
		case common::packet::NPCType::Basic: npcTypeName = "Basic"; break;
		case common::packet::NPCType::Tainer: npcTypeName = "Tainer"; break;
		case common::packet::NPCType::MagicGuard: npcTypeName = "MagicGuard"; break;
		case common::packet::NPCType::Elevator: npcTypeName = "Elevator"; break;
		case common::packet::NPCType::QuestNPC: npcTypeName = "QuestNPC"; break;
		default: 
			npcTypeName = "Basic";
			MYLOG("[Room] Unknown NPC Type: " << static_cast<int>(npc->GetNpcType()));
			break;
		}

		if (!npcTypeName.empty()) {
			for (auto& [pid, session] : _players) {
				if (session && session->_player) {
					for (auto& [quest_id, quest_info] : session->_player->_quests) {
						if (quest_info._state == common::packet::QuestState::IN_PROGRESS) {
							const QuestData* qData = LuaManager::Instance()->GetQuestData(quest_id);
							if (qData && qData->type == common::packet::QuestType::KILL_MONSTER && qData->target_name == npcTypeName) {
								session->_player->UpdateQuestProgress(quest_id, quest_info._current_count + 1);
							}
						}
					}
				}
			}
		}

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
		// [순서 중요]
		// SetActive(true) 이전에 BT와 상태를 완전히 초기화해야
		// UpdateLogics/UpdatePhysics 스레드가 초기화 안 된 상태로 틱하는 것을 방지합니다.

		// 1. BT 재구성 (Blackboard·노드 타이머 모두 새로 생성)
		npc->SetupBT();

		// 2. 전투 상태·dirty 필드 전체 초기화
		npc->SetHP(npc->GetMaxHP());
		npc->ResetForRespawn();             // _hitCooldown, _actionId, dirty 필드 리셋

		// 3. 위치 초기화 (Transform 기준)
		npc->SetPosition(npc->GetSpawnPosition());

		// 4. 물리 컨트롤러 복구 (위치 동기화 포함)
		if (auto cc = npc->GetComponent<GAME::CharacterControllerComponent>()) {
			cc->SetPhysicsActive(true);
			cc->SetPosition(npc->GetSpawnPosition());
		}

		// 5. 마지막에 Active 활성화 (이 시점부터 UpdateLogics 루프에 진입)
		npc->SetActive(true);
		_gridMap.Add(npc);

		// 6. 주변 플레이어에게 Spawn 패킷 전송
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


	void Room::OnPlayerDead(const std::shared_ptr<SESSION>& session)
	{
		if (!session || !session->_player) return;

		session->_player->SetState(common::packet::EntityState::DEAD);
		MYLOG("[Room] Player " << session->_id << " is DEAD. Respawning in 5s...");

		int64_t dead_player_id = session->_id;
		// 모든 NPC를 순회하며 타겟팅 초기화
		for (auto& [npc_id, npc] : _npcs) {
			if (auto ai = npc->GetComponent<GAME::AIComponent>()) {
				auto bb = ai->GetBlackboard();
				if (bb->has("target_enemy") && bb->get<int64_t>("target_enemy") == dead_player_id) {
					bb->set("target_enemy", std::any()); // 타겟 상실
				}
			}
		}

		int64_t playerId = session->_id;
		Server::Instance()->AddTimerJob(_logic_thread_idx, std::chrono::milliseconds(10000), [this, playerId]() {
			this->PushJob([this, playerId]() {
				auto it = _players.find(playerId);
				if (it != _players.end()) this->RespawnPlayer(it->second);
				});
			});
	}
	void Room::RespawnPlayer(const std::shared_ptr<SESSION>& session)
	{
		auto player = session->_player;
		common::Vec3 spawnPos = _currentStage->get_spawn_pos();

		player->SetHP(player->_max_hp);
		player->SetPosition(spawnPos);
		player->ResetState(); // [핵심 수정] 전투 상태 및 잡기 정보 강제 초기화

		if (auto cc = player->GetComponent<GAME::CharacterControllerComponent>()) {
			cc->SetPosition(spawnPos);
		}

		// 부활 패킷 브로드캐스트
		packet::SC_PACKET_PLAYER_RESURRECT res_pkt;
		res_pkt._type = packet::PacketType::S2C_P_PLAYER_RESURRECT;
		res_pkt._size = sizeof(res_pkt);
		res_pkt._id = session->_id;
		res_pkt._position = spawnPos;
		res_pkt._hp = player->GetHP();

		Broadcast(reinterpret_cast<char*>(&res_pkt), sizeof(res_pkt), -1, true);
	}

	void Room::PhysicsInitialize() {
		_jobSystem = new JPH::JobSystemSingleThreaded(JPH::cMaxPhysicsJobs);

		_physicsSystem = new JPH::PhysicsSystem();

		const JPH::uint cMaxBodies = 20480;
		const JPH::uint cNumBodyMutexes = 0;
		const JPH::uint cMaxBodyPairs = 20480;
		const JPH::uint cMaxContactConstraints = 20480;

		_physicsSystem->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
			_bpLayerInterface, _objVsBpLayerFilter, _objLayerPairFilter);

		_physicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));


		//CreatePhysicsTerrain();
		//CreatePhysicsMapObjects();
	}
	void Room::StartPhysicsRecording()
	{
#ifdef DEBUG_VIEWER
		// 1. 세션이 안 열려있으면 (최초 1회) 파일을 생성하고 레코더를 초기화
		if (!_isSessionOpen) {
			_dumpFile.open("physics_dump.bin", std::ios::binary);
			if (_dumpFile.is_open()) {
				_streamOut = std::make_unique<JPH::StreamOutWrapper>(_dumpFile);
				_recorder = std::make_unique<JPH::DebugRendererRecorder>(*_streamOut);
				_isSessionOpen = true;
				MYLOG("[Physics] Recording Session Opened.");
			}
		}

		// 2. 다음 물리 업데이트 때 딱 1프레임만 기록하라고 플래그를 켬
		_captureNextFrame = true;
		MYLOG("[Physics] Snapshot queued for next frame.");
#endif
	}

	void Room::StopPhysicsRecording()
	{
#ifdef DEBUG_VIEWER
		if (_isSessionOpen) {
			_recorder.reset();
			_streamOut.reset();
			if (_dumpFile.is_open()) _dumpFile.close();
			_isSessionOpen = false;
			MYLOG("[Physics] Recording Session Closed.");
		}
#endif
	}

	void Room::CheckAndStartGame()
	{
		// 모든 플레이어가 로딩을 마쳤는가? (혹은 로딩 중 누군가 나가서 남은 인원이 모두 준비되었는가?)
		if (_readyPlayers.size() == _players.size() && !_players.empty()) {
			MYLOG("[Room " << _room_id << "] All players READY! Starting Stage: " << _requestedSceneName);

			// 1. 스테이지 진입 (NPC 및 보스 스폰)
			if (_currentStage) {
				_currentStage->on_enter(this);
			}

			// 2. 모든 플레이어 위치 초기화
			for (auto& [id, player_session] : _players) {
				common::Vec3 spawn_pos = _currentStage->get_spawn_pos();
				float tx = spawn_pos.x;
				float tz = spawn_pos.z;

				JPH::RRayCast ray;
				ray.mOrigin = JPH::Vec3(tx, 500.0f, tz);
				ray.mDirection = JPH::Vec3(0, -1000.0f, 0);

				JPH::RayCastResult ray_result;
				float finalY = 0.0f;

				if (_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, ray_result)) {
					float hitY = ray.mOrigin.GetY() + ray.mDirection.GetY() * ray_result.mFraction;
					finalY = hitY + 2.0f;
				}
				else {
					finalY = MapDataManager::Instance()->GetGroundHeight(tx, tz) + 2.0f;
				}

				common::Vec3 spawnPos{ tx, finalY, tz };
				player_session->_player->SetPosition(spawnPos);
				player_session->_player->SetHP(100);
			}

			// 3. 현재 소환된 NPC 정보 집계 (동적 계산)
			uint16_t boss_count = 0;
			uint16_t npc_count = 0;
			int64_t min_boss_id = -1;
			int64_t min_npc_id = -1;

			for (auto& [id, npc] : _npcs) {
				if (npc->is_boss()) {
					if (min_boss_id == -1 || id < min_boss_id) min_boss_id = id;
					boss_count++;
				}
				else {
					if (min_npc_id == -1 || id < min_npc_id) min_npc_id = id;
					npc_count++;
				}
			}

			// [추가] 엘리베이터도 NPC 카운트에 포함하여 클라이언트가 풀을 생성하게 함
			if (!_elevators.empty()) {
				for (auto& elevator : _elevators) {
					int64_t eid = elevator->GetId();
					if (min_npc_id == -1 || eid < min_npc_id) min_npc_id = eid;
					npc_count++;
				}
			}

			// 4. 스폰 패킷 전송
			for (auto& [target_id, target_session] : _players) {
				// 플레이어 스폰 정보 전송
				for (auto& [source_id, source_session] : _players) {
					packet::PacketStream spawn_packet = packet::MakeSpawnPlayerPacket(source_session);
					target_session->do_send(spawn_packet.constable_data(), spawn_packet.Size());
				}

				// [추가] 현재 방의 모든 엘리베이터 스폰 정보를 준비된 플레이어에게 전송
				for (auto& elevator : _elevators) {
					packet::SC_PACKET_NPC_SPAWN elevator_spawn;
					elevator_spawn._type = common::packet::PacketType::S2C_P_NPC_SPAWN;
					elevator_spawn._npc_id = elevator->GetId();
					elevator_spawn._npc_type = common::packet::NPCType::Elevator;
					elevator_spawn._position = elevator->GetPosition();
					elevator_spawn._hp = elevator->GetHP();
					elevator_spawn._state = elevator->GetState();
					elevator_spawn._action_id = 0;

					packet::PacketStream elevatorStream;
					elevatorStream << elevator_spawn;
					elevatorStream << elevator->GetName();

					auto* h = reinterpret_cast<packet::PacketHeader*>(elevatorStream.mutable_data());
					h->_size = (uint16_t)elevatorStream.Size();

					target_session->do_send(elevatorStream.constable_data(), elevatorStream.Size());
				}

				// 동적으로 계산된 NPC/보스 카운트 정보 전송
				packet::SC_PACKET_SCENE_AWAKE npc_count_packet;
				npc_count_packet._type = packet::PacketType::S2C_P_NPC_COUNT;
				npc_count_packet._size = sizeof(npc_count_packet);
				npc_count_packet._boss_count = boss_count;
				npc_count_packet._boss_start_id = min_boss_id;
				npc_count_packet._npc_count = npc_count;
				npc_count_packet._npc_start_id = min_npc_id;
				target_session->do_send(reinterpret_cast<char*>(&npc_count_packet), sizeof(npc_count_packet));
			}

			// 5. NPC AI 활성화 및 방 상태 전환
			for (auto& [id, npc] : _npcs) {
				npc->SetLastUpdateTime(std::chrono::steady_clock::now());
			}

			StartGame(); // 이제 방 상태를 PLAYING으로 변경하여 게임 루프가 본격적으로 돌아가게 함
		}
	}
}
