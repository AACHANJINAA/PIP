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
		MOVE,
		RUN,
		JUMP,		// 점프 시작 요청
		HOVER,		// 공중 체류 (낙하 중)
		LANDING,	// 착지
		ACTION,
		SKILL_ONE,
		HITTED,
		DEAD,
		GRABBED,    // [추가] 잡힌 상태

		COUNT,
	};

	namespace ActionID
	{
		namespace Common
		{
			constexpr int32_t Attack = 1;
			constexpr int32_t SKILL1 = 2;
			constexpr int32_t JUMP   = 3; // [추가] 점프 액션 ID
			constexpr int32_t DASH   = 4; // [추가] 대쉬 액션 ID
			constexpr int32_t INTERACT = 5; // [추가] 레버 등 환경 상호작용
		}

		namespace Tainer
		{
			constexpr int32_t Roar		= 11;
			constexpr int32_t Charge	= 12;
			constexpr int32_t Slam		= 13;
			constexpr int32_t Claw		= 14;
			constexpr int32_t Grab		= 15;
			constexpr int32_t GrabCharge = 16; // [추가] 잡기 돌진
			constexpr int32_t GrabCarry  = 17; // [추가] 잡고 이동
			constexpr int32_t GrabSlam   = 18; // [추가] 잡고 메치기
		}
	}

	enum class QuestState : uint8_t {
		NONE = 0,
		IN_PROGRESS,
		COMPLETED,
		REWARDED
	};

	enum class QuestType : uint8_t {
		KILL_MONSTER = 1,
		GATHER_ITEM = 2,
		TALK_TO_NPC = 3,
		INTERACT_LEVER = 4
	};

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
		C2S_P_ACTION = 105, // [신규] 범용 행동 패킷
		S2C_P_PLAYER_ATTACK = 103, // 플레이어 다중 피격 정보
		S2C_P_NPC_ATTACK = 104,    // NPC 다중 피격 정보
		S2C_P_PLAYER_RESURRECT = 106, // [신규] 플레이어 부활 패킷 (죽은 플레이어가 부활할 때 브로드캐스트)
		S2C_P_SKILL_UNLOCKED = 107, // [신규] 스킬 잠금 해제 알림

		//------------------------------------------ 방 관련 패킷 ------------------------------------------ //
		C2S_P_ENTER_ROOM = 201,
		S2C_P_ENTER_ROOM_ACK = 202,
		C2S_P_ROOM_LIST = 203,
		S2C_P_ROOM_LIST_ACK = 204,
		C2S_P_PLAYER_READY = 205,		// [신규] 플레이어 준비 완료 패킷 (게임 시작 트리거용)
		S2C_P_ALL_PLAYERS_READY = 206,	// [신규] 모든 플레이어 준비 완료 패킷 (게임 시작 트리거용)
		S2C_P_CHANGE_SCENE = 207,		// [신규] 씬 변경 패킷 (어떤 씬 로딩 할지 보냄)
		S2C_P_PLAY_CUTSCENE = 208,		// [신규] 컷씬 재생 패킷
		C2S_P_CUTSCENE_DONE = 209,		// [신규] 컷씬 완료 패킷

		//------------------------------------------ 채팅 관련 패킷 ------------------------------------------ //
		C2S_P_CHAT_IN_ROOM = 301, // 클라 -> 서버: 방 내부 채팅 메시지
		S2C_P_CHAT_IN_ROOM = 302, // 서버 -> 클라: 방 내부 채팅 메시지 전달

		//------------------------------------------ NPC 관련 패킷 ------------------------------------------ //
		S2C_P_NPC_SPAWN = 501,
		S2C_P_NPC_MOVE = 502,
		S2C_P_NPC_DESPAWN = 503,
		S2C_P_NPC_UPDATE_HP = 504,
		S2C_P_NPC_MOVE_BATCH = 505,
		S2C_P_NPC_COUNT = 506,

		//------------------------------------------- 디버깅용 패킷 --------------------------------------- //
		C2S_P_DEBUG_COMMAND = 600,
		S2C_P_DEBUG_DRAW = 601,
		S2C_P_DEBUG_BT_INFO = 602,
		S2C_P_DEBUG_SHAPE = 603,

		//------------------------------------------- 인벤토리 관련 패킷 --------------------------------------- //
		S2C_P_INVENTORY_ALL_INFO = 701, // 인벤토리 전체 정보 패킷 (방 입장 시 또는 인벤토리 변경 시 전체 정보 전송)
		S2C_P_ITEM_UPDATE = 702, // 아이템 업데이트 패킷 (아이템 추가/제거/수량 변경 등)
		S2C_P_EQUIP_ITEM_UPDATE = 703, // 장착 아이템 업데이트 패킷 (장착/해제/강화 등)

		//------------------------------------------- 퀘스트 관련 패킷 --------------------------------------- //
		C2S_P_NPC_INTERACT = 801, // 퀘스트 수락/완료용 (클라 -> 서버)
		S2C_P_QUEST_UPDATE = 802, // 퀘스트 상태 변경 (서버 -> 클라)
		S2C_P_QUEST_INFO = 803,   // 현재 퀘스트 목록 동기화 (서버 -> 클라)
		S2C_P_INTERACT_ACK = 804, // [신규] 환경 사물(레버 등) 상호작용 성공 패킷 (서버 -> 클라)

		//------------------------------------------- 플레이어 스탯 동기화 관련 패킷 --------------------------------------- //
		S2C_P_PLAYER_STAT_SYNC = 901, // 최대 체력, 현재 체력, 데미지 동기화

		//------------------------------------------- 게임 흐름 제어 패킷 --------------------------------------- //
		S2C_P_COUNTDOWN = 1001, // 보스전 카운트다운 패킷 (서버 -> 클라)
	};

	enum class DebugShapeType : uint8_t {
		SPHERE = 0,
		BOX = 1,
		CAPSULE = 2,
		MESH = 3,
	};

	enum class NPCType : int32_t
	{
		error = 0,
		Basic = 1,
		Tainer = 2,
		Elevator = 3, // [추가] 엘리베이터 객체
		MagicGuard = 4, // [신규] 길찾기 경비병
		QuestNPC = 5,   // [추가] 퀘스트 제공 NPC
		Lever = 6,      // [추가] 보스방 진입 레버
		DynamicBox = 7, // [추가] 물리 테스트용 다이나믹 박스
		// 향후 추가될 NPC 유형들...
	};
	enum class InventoryUpdateType : uint8_t {
		Add = 1,    // 아이템 획득/증가
		Remove = 2, // 아이템 감소
		Delete = 3  // 아이템 완전 삭제 (수량 0)
	};

	enum class ItemId : uint32_t {
		ITEM_WOOD1 = 1,
		ITEM_ORE1 = 2,
		ITEM_STICK = 3,
		// ... 추가 아이템 ID
	};
	struct EquipItem {
		int64_t item_uid;     // DB에서 발급된 고유 ID (Primary Key)
		ItemId  item_id;      // 원본 아이템 ID (예: 롱소드)
		int     enhance_level;// 강화 수치
		bool    is_equipped;  // 장착 여부
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
	// 컷씬 완료 패킷
	struct CS_PACKET_CUTSCENE_DONE : PacketHeader
	{
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
		int32_t      _action_id;     // 스킬 인덱스 or 아이템 ID
		int64_t      _target_id;     // 타겟팅 스킬일 경우 대상 ID (없으면 -1)
		common::Quat _direction;     // 바라보는 방향
		Vec3         _position;      // 시전 위치 (클라이언트 기준 발사 위치)
		uint32_t     _client_time_stamp; // 클라이언트 타임스탬프 (밀리초)
	};

	// 클라 -> 서버: NPC 상호작용 (퀘스트 등)
	struct CS_PACKET_NPC_INTERACT : PacketHeader {
		int64_t _npc_id;    // 상호작용한 NPC의 ID
		int32_t _quest_id;  // 특정 퀘스트 상호작용 시 ID (없으면 0)
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

	enum class DebugCommandType : uint8_t {
		PHYSICS_SNAPSHOT = 1, // 물리 녹화 시작
		PHYSICS_STOP = 2,     // 물리 녹화 중지
		CHANGE_SCENE_BOSS = 3, // 보스 씬으로 강제 전환
		KILL_MONSTERS_NEARBY = 4, // [추가] 300m 이내 모든 몬스터 즉사
	};

	struct CS_PACKET_DEBUG_COMMAND : PacketHeader {
		DebugCommandType _command;
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
		int32_t			_max_hp; // [추가]
		int32_t			_mp; // [추가]
		int32_t			_level;
		int32_t			_exp;
		//뒤에 가변크기 name
	};
	struct SC_PACKET_PLAYER_RESURRECT : PacketHeader {
		int64_t _id;
		Vec3    _position;
		common::Quat _rotation; // [추가] 회전값 동기화
		int32_t _hp;
	};

	// 보스전 카운트다운 패킷 (서버 -> 클라)
	struct SC_PACKET_COUNTDOWN : PacketHeader {
		int8_t _count; // 남은 카운트 (5, 4, 3, 2, 1, 0 == "FIGHT!")
	};

	// 스탯 동기화 패킷 (퀘스트 보상 등)
	struct SC_PACKET_PLAYER_STAT_SYNC : PacketHeader {
		int64_t _id;
		int32_t _max_hp;
		int32_t _hp;
		int32_t _damage;
	};
	// 플레이어 이동 패킷
	struct SC_PACKET_MOVE : PacketHeader
	{
		int64_t			_id; // long long
		Vec3			_position;
		Vec3			_velocity; // [추가] 물리 예측을 위한 속도
		common::Quat	_rotation;
		EntityState		_state;
		int32_t			_action_id; 
		uint32_t		_client_tick; // [추가] RTT 측정을 위한 클라이언트 타임스탬프 에코
		int64_t			_grabbed_by_id; // [추가] 나를 잡고 있는 객체의 ID (-1이면 없음)
		int8_t			_grab_slot;     // [추가] 잡힌 슬롯 (0: 왼손, 1: 오른손 등)
		int32_t			_hp;            // [추가] 실시간 체력 동기화
		int32_t			_mp;            // [추가] 실시간 마나 동기화
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

	struct SC_PACKET_SKILL_UNLOCKED : PacketHeader {
	};

	// 접속 종료 패킷
	struct SC_PACKET_LEAVE : PacketHeader
	{
		int64_t _id; // long long
	};
	struct SC_PACKET_CHANGE_SCENE : PacketHeader
	{
		// 이 뒤에 가변 길이 씬 이름 문자열(std::string)이 스트림으로 덧붙여집니다.
	};
	// 컷씬 재생 패킷
	struct SC_PACKET_PLAY_CUTSCENE : PacketHeader
	{
		int32_t _cutscene_id;
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
		int64_t			_grabbed_by_id; // [추가]
		int8_t			_grab_slot;     // [추가]
		int32_t			_hp;            // [추가]
	};

	struct SC_PACKET_NPC_MOVE_BATCH : PacketHeader {
		uint16_t _count; // 몬스터 수
		// 뒤에 NPCMoveData 배열이 옴
	};

	struct SC_PACKET_NPC_SPAWN : PacketHeader
	{
		int64_t _npc_id;	// NPC의 고유 ID
		NPCType _npc_type;	// NPC의 타입 (예: 몬스터 종류)
		Vec3    _position;  // NPC의 초기 위치
		Quat	_rotation;	// [추가] NPC의 초기 회전 (쿼터니언)
		int32_t _hp;        // NPC의 초기 HP
		int32_t _max_hp;    // [추가] NPC의 최대 HP
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
		int32_t		_hp;        // [추가]
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

	struct SC_PACKET_QUEST_INFO : PacketHeader {
		int32_t _quest_count;
		// 이후에 퀘스트 배열(QuestStateData * _quest_count)이 옴
	};

	struct SC_PACKET_INTERACT_ACK : PacketHeader {
		int64_t _object_id; // 상호작용 대상 ID
		int32_t _interact_type; // 상호작용 종류 (0: 레버 등)
	};


	//--------------------------------------- 애니메이션 --------------------------------------- //--- //
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
	struct SC_PACKET_DEBUG_SHAPE : PacketHeader {
		DebugShapeType	_shape_type;
		Vec3			_position;		
		Quat			_rotation;
		uint32_t		_triangle_count;
		// std::vector<Vec3> _vertices; // 삼각형 정점 데이터 (triangle_count * 3 개의 Vec3)
	};
	//------------------------------------------- DB/아이템 관련 패킷 ------------------------------------------ //
	// [S2C] 전체 인벤토리 정보 (로그인/방 입장 시)
	struct SC_PACKET_INVENTORY_INFO : public PacketHeader {
		uint16_t _material_count; // 재료 아이템 종류 수
		uint16_t _equip_count;    // 장비 아이템 수
	};

	// [S2C] 아이템 개별 업데이트 (게임 플레이 중)
	struct SC_PACKET_ITEM_UPDATE : public PacketHeader {
		InventoryUpdateType _update_type;
		uint32_t _item_id;   // 재료 아이템 ID
		uint32_t _amount;    // 변동된 수량 (또는 최종 수량)
	};

	// [S2C] 장비 아이템 업데이트 (장착 상태 변화 등)
	struct SC_PACKET_EQUIP_UPDATE : public PacketHeader {
		int64_t				_player_id;
		InventoryUpdateType _update_type;
		EquipItem			_equip_data;
	};

	// ------------------------------------------ 퀘스트 관련 패킷 ------------------------------------------ //
	struct QuestUpdateInfo {
		int32_t _quest_id = -1;
		QuestState _state = QuestState::NONE;
		int32_t _current_count = 0; // 현재 진행도 (예: 몬스터 처치 수)
		int32_t _target_count = 0;  // 목표 수치
	};

	// [S2C] 퀘스트 상태 개별 업데이트
	struct SC_PACKET_QUEST_UPDATE : PacketHeader {
		QuestUpdateInfo _quest_info;
	};


#pragma pack (pop)
}

namespace common::anim_speed // 수정하지 말것
{
	constexpr float player_walk_animation = 2.0f;
	constexpr float player_run_animation = 2.5f;
}

namespace common::move_speed // 실제 이동속도 이므로 더 빨라저야 한다면 수정할것
{
	constexpr float one_frame_max_speed = 50.f; // 한 프레임에 최대 갈 수 있는 거리

	constexpr float player_walk_speed = 8.0f;
	constexpr float player_run_speed = 50.f;
}
