#pragma once
#include "Actor.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "CharacterControllerComponent.h"

namespace PIP::GAME
{
	class Player : public Actor
	{
	public:
		Player(int64_t owner_id);
		~Player() override = default;
		bool ValidateHit(JPH::PhysicsSystem* physics, const JPH::Shape* attackShape, const JPH::RMat44& attackTransform,
		                 uint32_t timestamp, GameObject* attacker, int32_t damage) override;
		void Update(float deltaTime, JPH::TempAllocator* allocator) override;
		void PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator) override;
		void init(int64_t id);
		void ResetState(); // [추가] 전투 및 상태 초기화

		int64_t GetId() const override { return _owner_id; } 

		void SetPosition(const common::Vec3& pos)
		{
			if (auto tc = GetComponent<GAME::TransformComponent>())
			{
				tc->SetPosition(pos);
			}
			// 물리 컴포넌트가 있다면 동기화도 반영
			if (auto cc = GetComponent<GAME::CharacterControllerComponent>())
			{
				cc->SetPosition(pos);
			}
		}

		void SetRotation(const common::Quat& rot)
		{
			auto tc = GetComponent<TransformComponent>();
			if (tc)
			{
				tc->SetRotation(rot);
			}
		}

		// [추가] 패킷 동기화 시 필요한 임시 정보 Getter/Setter
		void SetLastClientTargetPos(const common::Vec3& pos) { _lastClientTargetPos = pos; }
		common::Vec3 GetLastClientTargetPos() const { return _lastClientTargetPos; }

		void SetLastSentPos(const common::Vec3& pos) { _lastSentPos = pos; }
		common::Vec3 GetLastSentPos() const { return _lastSentPos; }

		void SetLastSentState(common::packet::EntityState state) { _lastSentState = state; }
		common::packet::EntityState GetLastSentState() const { return _lastSentState; }


		int32_t GetAttackDamage() const { return _damage; }

		void SetHP(int hp) override { _hp = hp; }
		int32_t GetHP() const override { return _hp; }
		int32_t GetMaxHP() const { return _max_hp; }

		void SetMP(int mp) { _mp = std::clamp(mp, 0, _max_mp); } // [추가]
		int32_t GetMP() const { return _mp; } // [추가]

		void SetState(const common::packet::EntityState& state) override { _state = state; }
		common::packet::EntityState GetState() const override { return _state; }

		void SetHitCooldown(float time) { _hitCooldown = time; }
		float GetHitCooldown() const { return _hitCooldown; }

		void SetDashCooldown(float time) { _dashCooldownTimer = time; }
		float GetDashCooldown() const { return _dashCooldownTimer; }

		void SetActionId(int32_t action_id) { _actionId = action_id; }
		int32_t GetActionId() const { return _actionId; }

		void SetSpeed(float speed) { _speed = speed; }
		float GetSpeed() const { return _speed; }

		void SetLastSentRot(common::Quat rot) { _lastSentRot = rot; }
		const common::Quat& GetLastSentRot() const { return _lastSentRot; }

		void SetLastClientTick(uint32_t tick) { _lastClientTick = tick; }
		uint32_t GetLastClientTick() const { return _lastClientTick; }

		// [추가] 공격 패킷의 클라이언트 타임스탬프로 지연 보상(리와인드) 판정용 시점을 계산.
		// 클라이언트/서버 시계는 동기화되어 있지 않으므로 절대 시각 비교 대신, 지금까지 관측된
		// 최솟값(clockOffset)을 기준선으로 삼아 "그 기준선보다 얼마나 더 늦게 왔는지"만 되감는다.
		uint32_t ComputeRewindTimestamp(uint32_t serverNow, uint32_t clientTimeStamp);

		bool IsDirty();
		void SyncSentData();

		void addMaterial(common::packet::ItemId item_id, uint32_t count);
		void removeMaterial(common::packet::ItemId item_id, uint32_t count);

		common::packet::SC_PACKET_MOVE CreateMovePacket() const;

		// [추가] 퀘스트 관련
		common::packet::QuestUpdateInfo AddQuest(int32_t quest_id);
		common::packet::QuestUpdateInfo CompleteQuest(int32_t quest_id);
		common::packet::QuestUpdateInfo UpdateQuestProgress(int32_t quest_id, int32_t current_count);
		common::packet::QuestUpdateInfo* GetQuest(int32_t quest_id);

		//common::Vec3				_position;
		//common::Quat				_rotation;
		
		int32_t						_max_hp;
		int32_t						_max_mp = 100; // [추가]
		int32_t						_mp = 100; // [추가]
		int32_t						_level;
		int32_t						_exp;
		int32_t						_damage;
		common::packet::EntityState _state = common::packet::EntityState::IDLE;
		int32_t						_actionId = 0; // 현재 액션 ID (애니메이션 트리거용)
		float						_speed = 10.0f; //방향에 곱해줄 속도값

		// [추가] 위치 보정 관련
		float						_mpRegenTimer = 0.0f; // [추가] 마나 회복 타이머
		float						_timeSinceLastHit = 0.0f; // [추가] 마지막 피격 이후 경과 시간
		float						_hpRegenTimer = 0.0f; // [추가] 체력 회복 타이머
		common::Vec3 _lastClientTargetPos = { 0.0f, 0.0f, 0.0f }; // 클라이언트가 마지막으로 보냈다고 우기는 좌표
		common::Vec3 _lastSentPos = { 0.0f, 0.0f, 0.0f };         // 서버에서 클라이언트에게 마지막으로 확정해서 보낸 좌표
		common::packet::EntityState _lastSentState = common::packet::EntityState::IDLE; // 서버에서 클라이언트에게 마지막으로 보낸 상태
		int32_t _lastSentActionId = 0; // 서버에서 클라이언트에게 마지막으로 보낸 액션 ID
		common::Quat _lastSentRot = { 0,0,0,1 }; // 서버에서 클라이언트에게 마지막으로 보낸 회전
		int64_t _lastSentGrabbedById = -1; // [추가]
		int8_t _lastSentGrabSlot = -1; // [추가]
		int32_t _lastSentHp = 0; // [추가]
		int32_t _lastSentMp = 0; // [추가]

		// [추가] 퀘스트 데이터 (진행중, 완료된 퀘스트 모두 포함)
		std::unordered_map<int32_t, common::packet::QuestUpdateInfo> _quests;

	private:
		float _hitCooldown = 0.0f;
		float _dashCooldownTimer = 0.0f;
		int64_t _owner_id; // 이 플레이어를 소유한 세션 ID	
		int32_t	_hp;
		uint32_t _lastClientTick = 0;

		uint32_t _actionClockOffsetMs = 0;
		bool _actionClockOffsetInit = false;
	};
}
