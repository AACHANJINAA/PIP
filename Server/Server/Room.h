#pragma once
#include "JoltSetup.h"
#include "Server.h"
#include "NPC.h"

namespace PIP::server
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

		// 플레이어 추가/제거
		void EnterPlayer(std::shared_ptr<SESSION> new_player);
		void LeavePlayer(long long player_id);

		// NPC
		void AddNPC(std::unique_ptr<NPC> npc);
		NPC* GetNPC(int npc_id);

		// 메인 로직
		// 게임 시작
		void StartGame();
		void Update(float deltaTime);
		void UpdatePhysics(float deltaTime);
		void UpdateLogics(float deltaTime);



		void PushJob(std::function<void()> job);
		// 방에 있는 모든 플레이어에게 패킷을 전송 (브로드캐스팅)
		void Broadcast(const char* data, size_t size, long long except_id = -1);
		// 정보 전송
		void SendRoomInfoToNewPlayer(std::shared_ptr<SESSION> new_player);
		// 공격 처리
		void HandleAttack(std::shared_ptr<SESSION> attacker);

		// 플레이어 이동 처리 로직
		void Execute_C2S_MOVE(std::shared_ptr<SESSION> session, const common::packet::CS_PACKET_MOVE& move_packet);
		void Execute_C2S_ROOM_ENTER(std::shared_ptr<SESSION> session, const common::packet::CS_PACKET_ENTER_ROOM& enter_packet);


		// 게터
		size_t GetPlayerCount() const { return _players.size(); }
		int GetRoomId() const { return _room_id; }
		int GetLogicThreadIndex() const { return _logic_thread_idx; }
		RoomState GetRoomState() const { return _room_state; }
		bool IsFull() const { return static_cast<uint8_t>(_players.size()) >= _max_players; }


	private:
		void CreatePhysicsTerrain();
		void PhysicsInitialize();

		void ProcessJobs();

		void UpdateSingleNPC(int npcId);
		void UpdateAI(float deltaTime);
		void SendNpcMovePacket(NPC* npc);

		//void UpdateNPC(int npcId);

	private:
		int _room_id;
		int _logic_thread_idx; // 이 방을 담당하는 로직 스레드의 인덱스
		uint8_t _max_players;
		RoomState _room_state;

		// 이 방에 속한 플레이어들의 목록
		concurrency::concurrent_queue<std::function<void()>>	_jobQueue;
		std::unordered_map<long long, std::shared_ptr<SESSION>> _players;
		std::unordered_map<int, std::unique_ptr<NPC>> _npcs;
		int _next_npc_id = 20000; // NPC ID는 플레이어 ID와 겹치지 않도록 높은 수에서 시작

		// --- Jolt 물리 객체 ---
		JPH::PhysicsSystem*					_physicsSystem = nullptr;
		JPH::TempAllocator*					_tempAllocator = nullptr;
		JPH::JobSystem*						_jobSystem = nullptr;

		// 인터페이스 구현체 (JoltSetup.h에 정의한 것들)
		BPLayerInterfaceImpl				_bpLayerInterface;
		ObjectVsBroadPhaseLayerFilterImpl	_objVsBpLayerFilter;
		ObjectLayerPairFilterImpl			_objLayerPairFilter;

		// 지형 Body ID 저장 (나중에 삭제 등을 위해)
		JPH::BodyID _terrainBodyID;

	};
}
