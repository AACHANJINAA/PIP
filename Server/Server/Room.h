#pragma once
#include "CombatDef.h"
#include "GridMap.h"
#include "JoltSetup.h"
#include "Server.h"
#include "NPC.h"
#include "Stage.h"

namespace PIP::GAME
{
	class GameObject;
	class Player;
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


		void ChangeScene(const std::string& nextSceneName);
		void ClearAllNPCs();


		bool IsPlayerNearby(const common::Vec3& get_position, float size);


		void ProcessJobs();
		// 물리 업데이트 (할당자 필수)
		void UpdatePhysics(float deltaTime, JPH::TempAllocator* allocator);
		// 로직 업데이트 (할당자 선택적 허용 - AI 때문)
		void UpdateLogics(float deltaTime, JPH::TempAllocator* tempAllocator = nullptr);




		void Broadcast(const char* data, size_t size, int64_t except_id = -1);
		// 특정 NPC를 보고 있는 플레이어들에게 데이터 전송
		void BroadcastToNPCViewers(int64_t npc_id, const char* data, size_t size);
		// 특정 플레이어를 보고 있는 플레이어들에게 데이터 전송
		void BroadcastToPlayerViewers(int64_t player_id, const char* data, size_t size);
		void BroadcastNpcBatch();

		void SendRoomInfoToNewPlayer(std::shared_ptr<SESSION> new_player);
		void SendNpcSpawnToPlayer(const std::shared_ptr<SESSION>& session, const GAME::NPC* npc);
		void SendNpcLeaveToPlayer(const std::shared_ptr<SESSION>& session, int64_t npcId);

		//void HandleAttack(const std::shared_ptr<SESSION>& attacker);
		void HandleAction(const std::shared_ptr<SESSION>& session, const common::packet::CS_PACKET_ACTION& action_packet);
		void ExecuteActorAction(GAME::Actor* attacker, const GAME::NPCAttackConfig& config);
		void Execute_C2S_MOVE(std::shared_ptr<SESSION> session, const common::packet::CS_PACKET_MOVE& move_packet);
		void Execute_C2S_ROOM_ENTER(const std::shared_ptr<SESSION>& session, const common::packet::CS_PACKET_ENTER_ROOM& enter_packet);
		void Execute_C2S_PLAYER_READY(const std::shared_ptr<SESSION>& session, const common::packet::CS_PACKET_PLAYER_READY& ready_packet);
		void SetupPlayerSpawn(const std::shared_ptr<SESSION>& session);

		size_t GetPlayerCount() const { return _players.size(); }
		int GetRoomId() const { return _room_id; }
		int GetLogicThreadIndex() const { return _logic_thread_idx; }
		RoomState GetRoomState() const { return _room_state; }
		bool IsFull() const { return static_cast<uint8_t>(_players.size()) >= _max_players; }

		GAME::Player* GetPlayer(int64_t player_id);
		GAME::Actor* GetActor(int64_t actor_id);

		std::map<int64_t, common::Vec3> GetPlayersPos() const;

		GAME::NPC* spawn_npc(GAME::NPCType type, const common::Vec3& pos, const std::string& name = "");
		JPH::PhysicsSystem* GetPhysicsSystem() const { return _physicsSystem; }
	private:
		void SpawnBoss();
		void SpawnInitialNPCs();
		void PhysicsInitialize();
		void CreatePhysicsTerrain();
		void CreatePhysicsMapObjects();
		/*void CreatePhysicsStaticMeshCollisions();*/
		

		void SendMapDebugDraw(const std::shared_ptr<SESSION>& session);
		void OnNPCDead(GAME::NPC* npc);
		void RespawnNPC(GAME::NPC* npc);

		void OnPlayerDead(const std::shared_ptr<SESSION>& session);
		void RespawnPlayer(const std::shared_ptr<SESSION>& session);
		
		void SendNpcMovePacket(GAME::NPC* npc);
		common::Vec3 find_safe_spawn_position(const common::Vec3& pos, JPH::Shape* npc_shape);

	private:
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


		std::unordered_map<int64_t, GAME::Actor*>				_actors;
		std::unordered_map<int64_t, std::shared_ptr<SESSION>>	_players;
		std::unordered_map<int64_t, std::unique_ptr<GAME::NPC>> _npcs;
		std::vector<GAME::NPC*>									_activeNpcList;
		int64_t _next_npc_id = 1000000;

		JPH::PhysicsSystem*					_physicsSystem = nullptr;
		JPH::JobSystem*						_jobSystem = nullptr;

		BPLayerInterfaceImpl				_bpLayerInterface;
		ObjectVsBroadPhaseLayerFilterImpl	_objVsBpLayerFilter;
		ObjectLayerPairFilterImpl			_objLayerPairFilter;

		std::vector<JPH::BodyID>			_terrainBodyIDs;
	};
}