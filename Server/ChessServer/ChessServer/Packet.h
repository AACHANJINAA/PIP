#pragma once
#include "CommonHeader.h"
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
	};

    constexpr char MAX_ID_LENGTH = 20;

    enum class MOVE_TYPE : char
    {
        error = 0,
        MOVE_UP,
        MOVE_DOWN,
        MOVE_RIGHT,
        MOVE_LEFT,
    };

    constexpr unsigned short MAP_HEIGHT = 8;
    constexpr unsigned short MAP_WIDTH = 8;

#pragma pack (push, 1)

    struct PacketHeader
    {
        uint16_t size;
        uint16_t type;
    };

    struct CS_LOGIN : public PacketHeader
    {
        // 생성자에서 헤더 자동 설정
        CS_LOGIN()
        {
            size = sizeof(CS_LOGIN); // 가변길이가 있다면 동적으로 계산 필요
            type = static_cast<uint16_t>(PacketType::C2S_P_LOGIN);
        }
        // 데이터 멤버
        // char name[MAX_NAME_LEN]; 대신 std::string 사용을 위한 준비
    };

    struct cs_packet_login
    {
        unsigned char   _size;
        PacketType     _type;
        char            _name[MAX_ID_LENGTH];
    };
    struct sc_packet_avatar_info
    {
        unsigned char   _size;
        PacketType     _type;
        long long       _id;
        short           _x, _y;
        short           _hp;
        short           _level;
        int             _exp;
    };

    struct sc_packet_move
    {
        unsigned char   _size;
        PacketType     _type;
        long long       _id;
        short           _x, _y;
    };

    struct sc_packet_enter
    {
        unsigned char   _size;
        PacketType     _type;
        long long       _id;
        char            _name[MAX_ID_LENGTH];
        char            _o_type;
        short           _x, _y;
    };

    struct sc_packet_leave
    {
        unsigned char   _size;
        PacketType     _type;
        long long       _id;
    };


    struct cs_packet_move
    {
        unsigned char   _size;
        PacketType     _type;
        char            _direction;
    };

}
