#pragma once
#include "CommonHeader.h"
namespace chess::packet
{
    enum class CommandType : std::uint8_t
    {
        MOVE_UP,
        MOVE_DOWN,
        MOVE_RIGHT,
        MOVE_LEFT,
        CONNECT,
        DISCONNECT,
        error
    };
    enum OperationType : std::uint8_t
    {
        OPERATION_CONNECT,
        OPERATION_DISCONNECT,
        OPERATION_MOVE_UP,
        OPERATION_MOVE_DOWN,
        OPERATION_MOVE_LEFT,
        OPERATION_MOVE_RIGHT,
        OPERATION_ERROR
	};
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
        friend std::ostream& operator<< (std::ostream& os, const CommandPacket& packet)
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
        }
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
}
