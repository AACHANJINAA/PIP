#pragma once
#include "CombatDef.h"
#include "GridMap.h"
#include "JoltSetup.h"
#include "MapDataManager.h"
#include "Server.h"
#include "NPC.h"
#include "Stage.h"
#include "Elevator.h"

namespace PIP::GAME
{
	class GameObject;
	class Player;
	class Elevator;
}
namespace PIP::SERVER
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
		void StartGame();
		void WaitGame();
		void Initialize();
		void PushJob(std::function<void()> job);

		void EnterPlayer(std::shared_ptr<SESSION> new_player);
		void LeavePlayer(int64_t player_id);

		// [추가] ID를 이용한 안전한 NPC 삭제 (그리드 및 시야 정리 포함)
		void RemoveNPC(int64_t npcId);
		void AddNPC(std::unique_ptr<GAME::NPC> npc);
		GAME::NPC* GetNPC(int64_t npc_id);
		std::vector<int64_t> spawn_npc(GAME::NPCType type, const std::string& name = "");
		
		// [추가] 엘리베이터 생성
		GAME::Elevator* spawn_elevator(const common::Vec3& start, const common::Vec3& end, float speed, float waitTime, const std::string& name = "Elevator");


		void ChangeScene(const std::string& nextSceneName);
		void ClearAllNPCs();


		bool IsPlayerNearby(const common::Vec3& get_position, float size);


		void ProcessJobs();
		// 물리 업데이트 (할당자 필수)
		void UpdatePhysics(float deltaTime, JPH::TempAllocator* allocator);
		// 로직 업데이트 (할당자 선택적 허용 - AI 때문)
		void UpdateLogics(float deltaTime, JPH::TempAllocator* tempAllocator = nullptr);




		void Broadcast(const char* data, size_t size, int64_t except_id = -1, bool force = false);
		// 특정 NPC를 보고 있는 플레이어들에게 데이터 전송
		void BroadcastToNPCViewers(int64_t npc_id, const char* data, size_t size);
		// 특정 플레이어를 보고 있는 플레이어들에게 데이터 전송
		void BroadcastToPlayerViewers(int64_t player_id, const char* data, size_t size);
		void BroadcastNpcBatch();

		void SendRoomInfoToNewPlayer(std::shared_ptr<SESSION> new_player);
		void SendNpcSpawnToPlayer(const std::shared_ptr<SESSION>& session, const GAME::NPC* npc);
		void SendNpcLeaveToPlayer(const std::shared_ptr<SESSION>& session, int64_t npcId);

		//void HandleAttack(const std::shared_ptr<SESSION>& attacker);
		void Execute_C2S_ACTION(const std::shared_ptr<SESSION>& session, const common::packet::CS_PACKET_ACTION& action_packet);
		void Execute_C2S_NPC_INTERACT(const std::shared_ptr<SESSION>& session, const common::packet::CS_PACKET_NPC_INTERACT& interact_packet);
		void Execute_C2S_CUTSCENE_DONE(const std::shared_ptr<SESSION>& session); // [추가] 퀘스트 등 상호작용
		void ExecuteActorAction(GAME::Actor* attacker, const GAME::AttackConfig& config);
		void Execute_C2S_MOVE(std::shared_ptr<SESSION> session, const common::packet::CS_PACKET_MOVE& move_packet);
		void Execute_C2S_ROOM_ENTER(const std::shared_ptr<SESSION>& session, const common::packet::CS_PACKET_ENTER_ROOM& enter_packet);
		void Execute_C2S_PLAYER_READY(const std::shared_ptr<SESSION>& session, const common::packet::CS_PACKET_PLAYER_READY& ready_packet);
		void SetupPlayerSpawn(const std::shared_ptr<SESSION>& session);
		void CheckAndStartGame();

		void OnNPCDead(GAME::NPC* npc);
		void OnPlayerDead(const std::shared_ptr<SESSION>& session);

		//---------- 아이템 관련 ---------------
		void SendFullInventory(const std::shared_ptr<SESSION>& session);
		void SendItemUpdate(const std::shared_ptr<SESSION>& session, common::packet::ItemId id, uint32_t amount, common::packet::InventoryUpdateType type);
		void SendEquipUpdateBroadcast(int64_t player_id, const common::packet::EquipItem& equip); // 장착 시 주변에 알림

		//---------- 퀘스트 관련 ---------------
		void SendQuestUpdate(const std::shared_ptr<SESSION>& session, const common::packet::QuestUpdateInfo& info);

		//---------- getter ----------------
		size_t GetPlayerCount() const { return _players.size(); }
		int GetRoomId() const { return _room_id; }
		int GetLogicThreadIndex() const { return _logic_thread_idx; }
		RoomState GetRoomState() const { return _room_state; }
		bool IsFull() const { return static_cast<uint8_t>(_players.size()) >= _max_players; }
		int64_t GetNextNpcId() { return _next_npc_id++; }

		GAME::Player* GetPlayer(int64_t player_id);
		void GetNPCTypeName(common::packet::NPCType type, std::string& npcTypeName);
		GAME::Actor* GetActor(int64_t actor_id);
		std::shared_ptr<SESSION> GetSession(int64_t player_id) {
			auto it = _players.find(player_id);
			if (it != _players.end()) return it->second;
			return nullptr;
		}

		std::map<int64_t, common::Vec3> GetPlayersPos() const;

		// BossStage 등에서 플레이어 맵 전체 접근용
		const std::unordered_map<int64_t, std::shared_ptr<SESSION>>& GetPlayers() const { return _players; }

		// 카운트다운 시작 (보스전 진입 시 호출)
		void StartCountdown(float seconds) {
			_countdownTimer     = seconds;
			_lastBroadcastCount = -1; // 즉시 첫 브로드캐스트 보내도록 초기화
			_isCountdownActive  = true;
		}

		JPH::PhysicsSystem* GetPhysicsSystem() const { return _physicsSystem; }

		void StartPhysicsRecording();
		void StopPhysicsRecording();
		void KillMonstersNearby(int64_t player_id, float range = 300.0f);

	private:
		void SpawnBoss();
		void SpawnInitialNPCs();
		void PhysicsInitialize();
		void CreatePhysicsTerrain();
		void CreatePhysicsMapObjects();
		/*void CreatePhysicsStaticMeshCollisions();*/
		void GetShapeTriangles(const JPH::Shape* inShape, std::vector<common::Vec3>& outTriangles);
		

		void SendMapDebugDraw(const std::shared_ptr<SESSION>& session);
		void SendDebugShape(const std::shared_ptr<SESSION>& session, const StaticMeshTile& tile);
		void RespawnNPC(GAME::NPC* npc);

		void RespawnPlayer(const std::shared_ptr<SESSION>& session);
		
		void SendNpcMovePacket(GAME::NPC* npc);
		common::Vec3 find_safe_spawn_position(const common::Vec3& pos, GAME::CharacterControllerComponent* cc);


	private:
		// [최적화] 셀 단위로 미리 직렬화된 이동 패킷 데이터 (BroadcastNpcBatch에서 사용)
		std::unordered_map<int, std::vector<char>> _cellMoveBuffers;

		// [최적화] 루프 내 재할당 방지를 위한 인스턴스별 버퍼 (스레드 안전)
		std::vector<int> _activeCellIndices;
		std::vector<int64_t> _processedNpcIds;
		std::vector<int> _playerNearbyCells;
		std::vector<int64_t> _currentViewedIds;
		std::vector<GAME::NPC*> _dirtyNPCs;

		int								_room_id;
		int								_logic_thread_idx;
		uint8_t							_max_players;
		RoomState						_room_state;
		float							_npcSyncTimer = 0.0f;

		GAME::GridMap 					_gridMap;

		concurrency::concurrent_queue<std::function<void()>>	_jobQueue;

		std::unique_ptr<Stage> _currentStage;		// 현재 맵 정보 (맵 오브젝트, NPC 스폰 지점 등)
		std::string            _requestedSceneName; // 전환 대기 중인 씬 이름
		std::set<int64_t>      _readyPlayers;       // 로딩 완료 보고를 한 플레이어 목록
		std::set<int64_t>      _cutsceneFinishedPlayers; // 컷씬 종료를 보고한 플레이어 목록
		std::set<int64_t>      _activatedLevers;         // 작동된 레버들의 ID 집합
		bool                   _isSkillUnlocked = false; // [추가] 방 기준 스킬 잠금 해제 상태
		int32_t                _bossKillCount = 0;       // [추가] 보스 킬 카운트 (스케일링 용도)

		// [카운트다운] 보스전 진입 카운트다운 제어
		float   _countdownTimer    = 0.0f; // 남은 카운트다운 시간
		int8_t  _lastBroadcastCount = -1;  // 마지막으로 브로드캐스트한 카운트 값
		bool    _isCountdownActive = false; // 카운트다운 진행 중 여부 (이동 잠금 플래그)
		void BroadcastCountdown(int8_t count); // 카운트다운 패킷 브로드캐스트


		std::unordered_map<int64_t, GAME::Actor*>				_actors;
		std::unordered_map<int64_t, std::shared_ptr<SESSION>>	_players;
		std::unordered_map<int64_t, std::unique_ptr<GAME::NPC>> _npcs;
		std::vector<std::unique_ptr<GAME::Elevator>>			_elevators; // [추가] 엘리베이터 관리
		std::vector<GAME::NPC*>									_activeNpcList;
		int64_t _next_npc_id = 1000000;

		JPH::PhysicsSystem*					_physicsSystem = nullptr;
		JPH::JobSystem*						_jobSystem = nullptr;

		BPLayerInterfaceImpl				_bpLayerInterface;
		ObjectVsBroadPhaseLayerFilterImpl	_objVsBpLayerFilter;
		ObjectLayerPairFilterImpl			_objLayerPairFilter;

		std::vector<JPH::BodyID>			_terrainBodyIDs;

		std::ofstream _dumpFile;
		std::unique_ptr<JPH::StreamOutWrapper> _streamOut;
#ifdef DEBUG_VIEWER
		std::unique_ptr<JPH::DebugRendererRecorder> _recorder;
#endif
		bool _isSessionOpen = false;     // 파일이 열려있는지 여부
		bool _captureNextFrame = false;  // 이번 프레임을 기록할지 여부 (F8 트리거)
	};
}