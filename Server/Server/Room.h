#pragma once
#include "GridMap.h"
#include "JoltSetup.h"
#include "Server.h"
#include "NPC.h"

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
		static std::random_device _rd;
		static std::mt19937 _gen;
		static std::uniform_real_distribution<> _npcURD;
	public:
		Room(int room_id, int logic_thread_idx);
		void Initialize();

		void EnterPlayer(std::shared_ptr<SESSION> new_player);
		void LeavePlayer(int64_t player_id);

		// [추가] ID를 이용한 안전한 NPC 삭제 (그리드 및 시야 정리 포함)
		void RemoveNPC(int64_t npcId);
		void AddNPC(std::unique_ptr<GAME::NPC> npc);
		GAME::NPC* GetNPC(int64_t npc_id);
		// NPC의 공격 및 행동 판정
		void ExecuteActorAction(GAME::Actor* attacker, const GAME::NPCAttackConfig& config);

		void StartGame();

		bool IsPlayerNearby(const common::Vec3& get_position, float size);
		// 물리 업데이트 (할당자 필수)
		void UpdatePhysics(float deltaTime, JPH::TempAllocator* allocator);
		
		// 로직 업데이트 (할당자 선택적 허용 - AI 때문)
		void UpdateLogics(float deltaTime, JPH::TempAllocator* tempAllocator = nullptr);

		void PushJob(std::function<void()> job);

		void Broadcast(const char* data, size_t size, long long except_id = -1);

		// 특정 NPC를 보고 있는 플레이어들에게 데이터 전송
		void BroadcastToNPCViewers(int npc_id, const char* data, size_t size);
		// 특정 플레이어를 보고 있는 플레이어들에게 데이터 전송
		void BroadcastToPlayerViewers(long long player_id, const char* data, size_t size);

		void BroadcastNpcBatch();
		void SendRoomInfoToNewPlayer(std::shared_ptr<SESSION> new_player);
		void SendNpcSpawnToPlayer(const std::shared_ptr<SESSION>& session, const GAME::NPC* npc);
		void SendNpcLeaveToPlayer(const std::shared_ptr<SESSION>& session, int npcId);

		void HandleAttack(const std::shared_ptr<SESSION>& attacker);
		void HandleAction(const std::shared_ptr<PIP::SERVER::SESSION>& actor, const common::packet::CS_PACKET_ACTION& action_packet);
		void Execute_C2S_MOVE(std::shared_ptr<SESSION> session, const common::packet::CS_PACKET_MOVE& move_packet);
		void Execute_C2S_ROOM_ENTER(std::shared_ptr<SESSION> session, const common::packet::CS_PACKET_ENTER_ROOM& enter_packet);

		size_t GetPlayerCount() const { return _players.size(); }
		int GetRoomId() const { return _room_id; }
		int GetLogicThreadIndex() const { return _logic_thread_idx; }
		RoomState GetRoomState() const { return _room_state; }
		bool IsFull() const { return static_cast<uint8_t>(_players.size()) >= _max_players; }

		GAME::Player* GetPlayer(int64_t player_id);
		GAME::Actor* GetActor(int64_t actor_id);

	private:
		void PhysicsInitialize();
		void CreatePhysicsTerrain();
		void CreatePhysicsMapObjects();

		void ProcessJobs();

		// [삭제] UpdateSingleNPC는 더 이상 사용하지 않음
		// void UpdateSingleNPC(int npcId);
		
		void SendNpcMovePacket(GAME::NPC* npc);

	private:
		int								_room_id;
		int								_logic_thread_idx;
		uint8_t							_max_players;
		RoomState						_room_state;
		float							_npcSyncTimer = 0.0f;

		GAME::GridMap 					_gridMap;

		concurrency::concurrent_queue<std::function<void()>>	_jobQueue;
		std::unordered_map<long long, std::shared_ptr<SESSION>> _players;
		std::unordered_map<int64_t, std::unique_ptr<GAME::NPC>> _npcs;
		int64_t _next_npc_id = 20000;

		JPH::PhysicsSystem*					_physicsSystem = nullptr;
		JPH::JobSystem*						_jobSystem = nullptr;

		BPLayerInterfaceImpl				_bpLayerInterface;
		ObjectVsBroadPhaseLayerFilterImpl	_objVsBpLayerFilter;
		ObjectLayerPairFilterImpl			_objLayerPairFilter;

		JPH::BodyID _terrainBodyID;
	};
}