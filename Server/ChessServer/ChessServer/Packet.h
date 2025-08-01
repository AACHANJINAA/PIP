#pragma once
#include "Room.h"
#include "PacketStream.h"

namespace chess::packet
{
    constexpr short SERVER_PORT = 3000;

	enum class PacketType : uint16_t
    {
        error = 0,
        S2C_P_AVATAR_INFO = 1,
        S2C_P_MOVE = 2,
        S2C_P_ENTER = 3,
        S2C_P_LEAVE = 4,
        C2S_P_LOGIN = 5,
        C2S_P_MOVE = 6,
        C2S_P_ATTACK = 101,
        S2C_P_ATTACK = 102,
        C2S_P_ENTER_ROOM = 201,
		S2C_P_ENTER_ROOM_ACK = 202,
        C2S_P_ROOM_LIST = 203,
        S2C_P_ROOM_LIST_ACK = 204,
        C2S_P_CHAT_IN_ROOM = 301,    // 클라 -> 서버: 방 내부 채팅 메시지
        S2C_P_CHAT_IN_ROOM = 302,    // 서버 -> 클라: 방 내부 채팅 메시지 전달
	};

    constexpr char MAX_ID_LENGTH = 20;

	enum class MOVE_TYPE : uint16_t
    {
        error = 0,
        MOVE_UP = 1,
        MOVE_DOWN = 2,
        MOVE_RIGHT = 3,
        MOVE_LEFT = 4,
    };

    constexpr unsigned short MAP_HEIGHT = 8;
    constexpr unsigned short MAP_WIDTH = 8;

    enum class AttackDirection : uint8_t
    {
        UP,
        DOWN,
        LEFT,
        RIGHT
    };

    
#pragma pack (push, 1)

    struct PacketHeader
    {
        uint16_t _size;
        PacketType _type;
    };

    struct CS_PACKET_CHAT_IN_ROOM : PacketHeader
    {
        uint16_t _message_length;
    };

    struct SC_PACKET_CHAT_IN_ROOM : PacketHeader
    {
        long long _sender_id;
        uint16_t _message_length;
    };

    struct CS_PACKET_ENTER_ROOM : PacketHeader
    {
        int _room_id;
    };

    struct SC_PACKET_ENTER_ROOM_ACK : PacketHeader
    {
        bool _success; // 방 들어갈수 있는 지 없는지 (true: 가능, false: 불가능)
		int _room_id;  // 들어갈 방 아이디 ( false면 의미 없음)
    };
	
    struct CS_PACKET_ROOM_LIST : PacketHeader
    {
        // 실제로는 아무것도 필요하지않는 클라에서 리스트 보여줘 요청
    };

	/// <summary>
	/// 실제 메모리 구조: 
	/// [ SC_PACKET_ROOM_LIST_ACK 룸 갯수 ] [ RoomInfo 룸 정보 구조체 ]
    struct SC_PACKET_ROOM_LIST_ACK : PacketHeader
    {
		uint16_t _room_count; // 방의 갯수 ( 이 뒤에 RoomInfo 구조체가 _room_count 만큼 반복됨 )
    };

    struct SC_PACKET_ENTER : PacketHeader
    {
		int64_t _id; // long long
        short   _x;
        short   _y;
		// 뒤에 _name이 따라옴 가변 크기
    };

    struct CS_PACKET_ATTACK : PacketHeader
    {
        //int64_t _id; // �̹� ������ �� �÷��̾��� id�� �˰� �ִ�.
        //AttackDirection _direction; // ������ 4���� �����Ұ��� -> �������� ����
	};

    struct SC_PACKET_ATTACK : PacketHeader
	{
        int64_t     _attacker_id;
        int64_t     _target_id;
        int32_t     _damage;
        int32_t     _target_current_hp;
    };

    struct CS_PACKET_LOGIN : PacketHeader
    {
		//name�� ���� ���̷� , PacketStream���� ó��
	};

    struct SC_PACKET_AVATAR_INFO : PacketHeader
    {
        int64_t _id; // long long
        short   _x;
        short   _y;
        short   _hp;
        short   _level;
        int     _exp;
	};

    struct SC_PACKET_MOVE : PacketHeader
    {
        int64_t _id; // long long
        short   _x;
        short   _y;
	};

    struct SC_PACKET_LEAVE : PacketHeader
    {
        int64_t _id; // long long
	};

    struct CS_PACKET_MOVE : PacketHeader
    {
        MOVE_TYPE _direction;
    };

#pragma pack (pop)
}
