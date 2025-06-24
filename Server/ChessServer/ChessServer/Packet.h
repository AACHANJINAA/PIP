#pragma once
#include "CommonHeader.h"
namespace chess::packet
{
    constexpr short SERVER_PORT = 3000;

    enum class PACKET_TYPE : char
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

    struct CommandPacket
    {
        CommandPacket() : command{ CommandType::MOVE_UP } {}
        CommandPacket(int command) : command{ static_cast<CommandType>(command) } {}
        CommandPacket(CommandType command) : command{ command } {}
        CommandType command;

        std::vector<char> serialize() const
        {
            std::vector<char> data(sizeof(command));
            memcpy(data.data(), &command, sizeof(command));
            return data;
        }

        static CommandPacket deserialize(const char* data, size_t size)
        {
            CommandPacket packet;
            if (size >= sizeof(packet.command))
            {
                memcpy(&packet.command, data, sizeof(packet.command));
            }
            return packet;
        }

        //debugging용
        /*friend std::ostream& operator<< (std::ostream& os, const CommandPacket& packet)
        {
            switch (packet.command)
            {
                case CommandType::MOVE_DOWN:
                    os << "move down" << std::endl;
                    break;
                case CommandType::MOVE_UP:
                    os << "move up" << std::endl;
                    break;
                case CommandType::MOVE_LEFT:
                    os << "move left" << std::endl;
                    break;
                case CommandType::MOVE_RIGHT:
                    os << "move right" << std::endl;
                    break;
                default:
                    os << "패킷 에러?" << std::endl;
                    break;
            }
            return os;
        }*/
    };

    struct PositionPacket
    {
        char size;
        long long id;
        UINT x = 0;
        UINT y = 0;

        std::vector<char> serialize() const
        {
            std::vector<char> data(sizeof(id) + sizeof(x) + sizeof(y));
            memcpy(data.data(), &id, sizeof(id)); // id를 data에 복사
            memcpy(data.data() + sizeof(id), &x, sizeof(x)); // id 복사한 offset에 x를 복사
            memcpy(data.data() + sizeof(id) + sizeof(x), &y, sizeof(y)); // x 복사한 offset에 y를 복사
            return data;
        }
        static PositionPacket deserialize(const char* data, size_t size)
        {
            PositionPacket packet;
            if (size >= sizeof(packet.id) + sizeof(packet.x) + sizeof(packet.y))
            {
                memcpy(&packet.id, data, sizeof(packet.id));
                memcpy(&packet.x, data + sizeof(packet.id), sizeof(packet.x));
                memcpy(&packet.y, data + sizeof(packet.id) + sizeof(packet.x), sizeof(packet.y));
            }
            return packet;
        }
    };

    struct sc_packet_avatar_info
    {
        unsigned char   _size;
        PACKET_TYPE     _type;
        long long       _id;
        short           _x, _y;
        short           _hp;
        short           _level;
        int             _exp;
    };

    struct sc_packet_move
    {
        unsigned char   _size;
        PACKET_TYPE     _type;
        long long       _id;
        short           _x, _y;
    };

    struct sc_packet_enter
    {
        unsigned char   _size;
        PACKET_TYPE     _type;
        long long       _id;
        char            _name[MAX_ID_LENGTH];
        char            _o_type;
        short           _x, _y;
    };

    struct sc_packet_leave
    {
        unsigned char   _size;
        PACKET_TYPE     _type;
        long long       _id;
    };

    struct cs_packet_login
    {
        unsigned char   _size;
        PACKET_TYPE     _type;
        char            _name[MAX_ID_LENGTH];
    };

    struct cs_packet_move
    {
        unsigned char   _size;
        PACKET_TYPE     _type;
        char            _direction;
    };

}
