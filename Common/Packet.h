#pragma once
// 테스트 주석: common::packet 네임스페이스가 포함된 파일입니다.
// [TEST] Gemini CLI를 통한 파일 수정 테스트 주석입니다.
#include "Vector3.h"


namespace common::packet
{
	constexpr short SERVER_PORT = 19001;

	enum class EntityState : uint16_t 
	{
		IDLE = 0,
		MOVE = 1,
		ACTION = 2,
		HITTED = 3,
		DEAD = 4,


		COUNT,
	};

	namespace ActionID
	{
		namespace Common
		{
			constexpr int32_t Attack = 1;
		}

		namespace Tainer
		{
			constexpr int32_t Roar		= 11;
			constexpr int32_t Charge	= 12;
			constexpr int32_t Slam		= 13;
			constexpr int32_t Claw		= 14;
			constexpr int32_t Grab		= 15;
		}
	}

	//enum class OBJECT_STATE : uint16_t { // 애니메이션용 상태값
	//	IDLE	,	// 대기
	//	WALK	,	// 걷기
	//	RUN		,	// 달리기 (필요 시 클라이언트에서 WALK와 RUN 애니메이션 구분하여 사용)
	//	JUMP	,	// 점프 시작 (점프 애니메이션이 시작되는 순간)
	//	LANDING ,	// 착지 (점프 후 땅에 닿는 순간)
	//	HOVER	,	// 공중에 떠있는 상태 (예: 점프 중, 낙하 중)
	//	T_POSE	,	// T-포즈 (디버깅용)
	//	ROAR	,	// 포효
	//	HITTED	,	// 피격
	//	CHARGE	,	// 돌진
	//	DEATH		,	// 사망

	//	ATTACK1 = 101,	// 공격 (추가적으로 여러개 필요할듯)
	//	ATTACK2 = 102,
	//	ATTACK3 = 103,

	//	SKILL1  = 201,	// 스킬 (추가적으로 여러개 필요할듯)
	//	SKILL2  = 202,
	//	SKILL3  = 203,
	//	// 필요 시 추가
	//};
	enum class PacketType : uint16_t {
		error = 0,

		//------------------------------------------ 로그인 관련 패킷 ------------------------------------------ //
		C2S_P_LOGIN = 11,
		S2C_P_LOGIN_ACK = 14,
		S2C_P_LEAVE = 12,
		S2C_P_SPAWN_PLAYER = 13,

		//------------------------------------------ 이동 관련 패킷 ------------------------------------------ //
		S2C_P_MOVE = 91,
		C2S_P_MOVE = 92,

		//------------------------------------------ Action 관련 패킷 ------------------------------------------ //
		//C2S_P_ATTACK = 101, // 범용 행동 패킷으로 대체되어 사용되지 않음
		C2S_P_ACTION = 105, // [신규] 범용 행동 패킷

		// S2C_P_ATTACK = 102, // 다중 공격을 위해 아래 패킷들로 대체되어 사용되지 않음
		S2C_P_PLAYER_ATTACK = 103, // 플레이어 다중 피격 정보
		S2C_P_NPC_ATTACK = 104,    // NPC 다중 피격 정보

		//------------------------------------------ 방 관련 패킷 ------------------------------------------ //
		C2S_P_ENTER_ROOM = 201,
		S2C_P_ENTER_ROOM_ACK = 202,
		C2S_P_ROOM_LIST = 203,
		S2C_P_ROOM_LIST_ACK = 204,
		C2S_P_PLAYER_READY = 205,		// [신규] 플레이어 준비 완료 패킷 (게임 시작 트리거용)
		S2C_P_ALL_PLAYERS_READY = 206,	// [신규] 모든 플레이어 준비 완료 패킷 (게임 시작 트리거용)
		S2C_P_CHANGE_SCENE = 207,		// [신규] 씬 변경 패킷 (어떤 씬 로딩 할지 보냄)
		S2C_P_NPC_COUNT = 208,

		//------------------------------------------ 채팅 관련 패킷 ------------------------------------------ //
		C2S_P_CHAT_IN_ROOM = 301, // 클라 -> 서버: 방 내부 채팅 메시지
		S2C_P_CHAT_IN_ROOM = 302, // 서버 -> 클라: 방 내부 채팅 메시지 전달

		//------------------------------------------ NPC 관련 패킷 ------------------------------------------ //
		S2C_P_NPC_SPAWN = 501,
		S2C_P_NPC_MOVE = 502,
		S2C_P_NPC_DESPAWN = 503,
		S2C_P_NPC_UPDATE_HP = 504,
		S2C_P_NPC_MOVE_BATCH = 505,

		//------------------------------------------- 디버깅용 패킷 --------------------------------------- //
		S2C_P_DEBUG_DRAW = 601,
		S2C_P_DEBUG_BT_INFO = 602,
	};


	enum class MOVE_TYPE : uint16_t
	{
		error = 0,
		MOVE_UP = 1,
		MOVE_DOWN = 2,
		MOVE_RIGHT = 3,
		MOVE_LEFT = 4,
	};

	enum class AttackDirection : uint8_t
	{
		UP,
		DOWN,
		LEFT,
		RIGHT
	};

	// [신규] 행동 종류 열거형
	enum class ActionType : uint8_t
	{
		NONE = 0,
		NORMAL_ATTACK = 1, // 일반 공격
		SKILL = 2,         // 스킬 사용
		INTERACT = 3       // 상호작용 (예: 아이템 줍기)
	};

	enum class DebugShapeType : uint8_t {
		SPHERE = 0,
		BOX = 1,
		CAPSULE = 2
	};

	enum class NPCType : int32_t
	{
		error = 0,
		Basic = 1,
		Tainer = 2,
		// 향후 추가될 NPC 유형들...
	};

#pragma pack (push, 1)
	struct PacketHeader
	{
		uint16_t _size;
		PacketType _type;
	};
	struct RoomInfo
	{
		int _room_id;
		uint8_t _player_count; // 방의 현재 인원 수
		// 필요하다면 방 제목, 게임 상태 등 추가 정보 포함 가능
	};


	// ------------------------------------------ client to server ------------------------------------------ //
	// 클라 -> 서버
	// 로그인 요청 패킷
	struct CS_PACKET_LOGIN : PacketHeader
	{
		// 로그인 한다~
	};
	// 방 목록 요청 패킷
	struct CS_PACKET_ROOM_LIST : PacketHeader
	{
		// 실제로는 아무것도 필요하지않는 클라에서 리스트 보여줘 요청
	};
	// 방 입장 요청 패킷
	struct CS_PACKET_ENTER_ROOM : PacketHeader
	{
		int _room_id;
	};
	// enum class MOVE_TYPE : uint16_t
	struct CS_PACKET_MOVE : PacketHeader
	{
		Vec3			_position;
		Vec3            _move_dir; // [추가] 클라이언트의 이동 입력 단위 벡터 (XZ 평면)
		common::Quat	_rotation;
		EntityState		_state;
		int32_t			_action_id;
		uint32_t		_client_tick; // [추가] 클라이언트의 타임스탬프
	};
	// [신규] 클라 -> 서버: 범용 행동 패킷
	struct CS_PACKET_ACTION : PacketHeader
	{
		ActionType   _action_type;   // 1: 평타, 2: 스킬...
		int32_t      _action_id;     // 스킬 인덱스 or 아이템 ID
		int64_t      _target_id;     // 타겟팅 스킬일 경우 대상 ID (없으면 -1)
		common::Quat _direction;     // 바라보는 방향
		Vec3         _position;      // 시전 위치 (클라이언트 기준 발사 위치)
		uint32_t     _client_time_stamp; // 클라이언트 타임스탬프 (밀리초)
	};

	//struct CS_PACKET_MOVE : PacketHeader
	struct CS_PACKET_ATTACK : PacketHeader
	{
		//int64_t _id; 
		//AttackDirection _direction;
	};
	// 클라 -> 서버: 방 내부 채팅 메시지
	struct CS_PACKET_CHAT_IN_ROOM : PacketHeader
	{
		uint16_t _message_length;
	};
	
	struct CS_PACKET_PLAYER_READY : PacketHeader
	{
		// 플레이어가 준비 완료 상태임을 알리는 패킷 (추가 데이터 필요 없음)
		// 확인용으로 뒤에 바뀐 씬 이름 붙을수 있음
	};

	// ------------------------------------------ server to client ------------------------------------------ // 
	// 서버 -> 클라
	// 로그인 결과 패킷
	struct SC_PACKET_LOGIN_ACK : PacketHeader
	{
		int64_t _my_session_id; // 클라이언트 자신의 세션 ID
		bool     _success;       // 로그인 성공 여부
	};
	
	/// 메모리 구조 [ SC_PACKET_ROOM_LIST_ACK 룸 갯수 ] [ RoomInfo 룸 정보 구조체 ]
	/// 방 목록 패킷
	struct SC_PACKET_ROOM_LIST_ACK : PacketHeader
	{
		uint16_t _room_count; // 방의 갯수 ( 이 뒤에 RoomInfo 구조체가 _room_count 만큼 반복됨 )
	};
	// 방 입장 결과 패킷
	struct SC_PACKET_ENTER_ROOM_ACK : PacketHeader
	{
		bool _success; // 방 들어갈수 있는 지 없는지 (true: 가능, false: 불가능)
		int _room_id;  // 들어갈 방 아이디 ( false면 의미 없음)
	};

	struct SC_PACKET_SCENE_AWAKE : PacketHeader
	{
		int64_t _npc_count; // npc개수
		int64_t _npc_start_id; // npc 아이디
		int64_t _boss_count; // 보스개수
		int64_t _boss_start_id; // 보스 아이디
	};

	// 플레이어 스폰 패킷
	struct SC_PACKET_SPAWN_PLAYER : PacketHeader
	{
		int64_t			_id; // long long
		Vec3			_position; // 플레이어의 위치
		Quat			_rotation;
		EntityState		_state;
		int32_t			_action_id;
		int32_t			_hp;
		int32_t			_level;
		int32_t			_exp;
		//뒤에 가변크기 name
	};

	// 플레이어 이동 패킷
	struct SC_PACKET_MOVE : PacketHeader
	{
		int64_t			_id; // long long
		Vec3			_position;
		common::Quat	_rotation;
		EntityState		_state;
		int32_t			_action_id; // [추가]
	};

	// 공격 결과 패킷 (사용되지 않음)
    /*struct SC_PACKET_ATTACK : PacketHeader
	{
        int64_t     _attacker_id;
        int64_t     _target_id;
        int32_t     _damage;
        int32_t     _target_current_hp;
    };*/

	// 플레이어 단일 피격 정보를 담는 구조체
	struct PlayerHitInfo {
		int64_t _target_id;
		int32_t _damage;
		int32_t _target_current_hp;
		Vec3	_target_position;
		Vec3	_knockback_vector;
	};

	// 플레이어 다중 공격 결과를 담는 새로운 패킷 구조체
	struct SC_PACKET_PLAYER_ATTACK : PacketHeader {
		int64_t _attacker_id;
		uint8_t _hit_count;
		// 이 헤더 뒤에 _hit_count 만큼의 PlayerHitInfo 구조체가 이어집니다.
	};
	// 접속 종료 패킷
	struct SC_PACKET_LEAVE : PacketHeader
	{
		int64_t _id; // long long
	};
	struct SC_PACKET_CHANGE_SCENE : PacketHeader
	{
		// 씬 변경 패킷 뒤에 이름이 들어옴
	};
	struct SC_PACKET_ALL_PLAYERS_READY : PacketHeader
	{
		// 모든 플레이어가 준비 완료 상태임을 알리는 패킷 (추가 데이터 필요 없음)
	};

	// ------------------------------------------- NPC 관련 패킷 ------------------------------------------ //
	struct NPCMoveData {
		int64_t			_npc_id;
		Quat			_rotation;
		Vec3			_position;
		Vec3			_velocity;
		uint32_t		_time_stamp;
		EntityState		_state;      // 논리 상태
		int32_t			_action_id;  // [추가] 0이면 없음, 보스 스킬 번호 등
	};

	struct SC_PACKET_NPC_MOVE_BATCH : PacketHeader {
		uint16_t _count; // 몬스터 수
		// 뒤에 NPCMoveData 배열이 옴
	};

	struct SC_PACKET_NPC_SPAWN : PacketHeader
	{
		int64_t _npc_id; // NPC의 고유 ID
		NPCType _npc_type; // NPC의 타입 (예: 몬스터 종류)
		Vec3    _position;  // NPC의 초기 위치
		int32_t _hp;        // NPC의 초기 HP
		EntityState _state; // NPC의 초기 상태
		int32_t _action_id;
		// 뒤에 가변크기 name
	};

	struct SC_PACKET_NPC_MOVE : PacketHeader
	{
		int64_t     _npc_id;
		Vec3        _position;
		Vec3		_velocity;
		Quat		_rotation;
		uint32_t	_time_stamp;
		EntityState	_state;
		int32_t		_action_id; // [추가] 0이면 없음, 보스 스킬 번호 등
		// 뒤에 가변 크기 name
	};
	// NPC 단일 피격 정보를 담는 구조체
	struct NPCHitInfo {
		int64_t _target_id;
		int32_t _damage;
		int32_t _target_current_hp;
	};

	// NPC 다중 공격 결과를 담는 새로운 패킷 구조체
	struct SC_PACKET_NPC_ATTACK : PacketHeader {
		int64_t _attacker_id;
		uint8_t _hit_count;
		// 이 헤더 뒤에 _hit_count 만큼의 NPCHitInfo 구조체가 이어집니다.
	};
	struct SC_PACKET_NPC_DESPAWN : PacketHeader
	{
		int64_t _npc_id; // NPC의 고유 ID
	};
	struct SC_PACKET_NPC_UPDATE_HP : PacketHeader
	{
		int64_t _npc_id; // NPC의 고유 ID
		int32_t _current_hp; // NPC의 현재 HP
	};

	// ------------------------------------------- 채팅 관련 패킷 ------------------------------------------ //
	struct SC_PACKET_CHAT_IN_ROOM : PacketHeader
	{
		int64_t _sender_id;
		uint16_t _message_length;
	};
	// ------------------------------------------- 디버깅용 패킷 ------------------------------------------ //
	struct SC_PACKET_DEBUG_DRAW : PacketHeader {
		DebugShapeType _shape_type;
		Vec3           _position;
		Quat           _rotation;
		Vec3           _extents;  // Sphere: x=반경 / Box: x,y,z=반폭 / Capsule: x=반경, y=절반높이
		float          _duration; // 지속 시간 (초)
	};
	struct SC_PACKET_DEBUG_BT_INFO : PacketHeader {
		int64_t  _actor_id;
		// 현재 실행 중인 노드 이름 -> 가변으로 들어감
	};
#pragma pack (pop)
}

// TEST COMMENT: This is for testing purposes.
