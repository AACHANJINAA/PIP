#pragma once
#include "pch.h"
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
        C2S_P_ATTACK = 101,
        S2C_P_ATTACK = 102,
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

    class PacketStream
    {
    public:
        PacketStream(size_t initialSize = 256) : _pos(0)
        {
            _buffer.reserve(initialSize);
        }

        PacketStream(const char* data, size_t size) : _buffer(data, data + size), _pos(0)
    	{}


        template <typename T>
			requires (std::is_trivially_copyable_v<T> && !std::is_same_v<T, std::string>)
        PacketStream& operator<<(const T& data)
        {
	        const size_t dataSize = sizeof(T);
	        _buffer.insert(_buffer.end(), reinterpret_cast<const char*>(&data), reinterpret_cast<const char*>(&data) + dataSize);
            return *this;
        }

        PacketStream& operator<<(const std::string& str)
        {
            uint16_t len = static_cast<uint16_t>(str.length());
            *this << len;

            _buffer.insert(_buffer.end(), str.begin(), str.end());
            return *this;
        }

        void Write(const char* data, size_t size)
        {
            _buffer.insert(_buffer.end(), data, data + size);
        }

		// Deserialization (읽기) 연산자
        template <typename T>
            requires (std::is_trivially_copyable_v<T> && !std::is_same_v<T, std::string>)
        PacketStream& operator>>(T& data)
        {
            const size_t dataSize = sizeof(T);
            if (_pos + dataSize > _buffer.size())
            {
                throw std::runtime_error("PacketStream read overflow");
            }
            memcpy(&data, &_buffer[_pos], dataSize);
            _pos += dataSize;
            return *this;
        }

        PacketStream& operator>>(std::string& str)
        {
            uint16_t len;
            *this >> len;

            if (_pos + len > _buffer.size())
            {
                throw std::runtime_error("PacketStream read(string) overflow");
            }
            str.assign(&_buffer[_pos], len);
            _pos += len;
            return *this;
        }

        const char* Data() const { return _buffer.data(); }
        size_t Size() const { return _buffer.size(); }
    private:
        std::vector<char> _buffer;
        size_t _pos; // 읽기 위치
    };
#pragma pack (push, 1)

    struct PacketHeader
    {
        uint16_t _size;
        uint16_t _type;
    };

    struct CS_PACKET_ATTACK : PacketHeader
    {
        //int64_t _id; // 이미 서버는 이 플레이어의 id는 알고 있다.
        //AttackDirection _direction; // 지금은 4방향 공격할거임 -> 서버에서 결정
	};

    struct SC_PACKET_ATTACK : PacketHeader
	{
        uint32_t    _attacker_id;
        uint32_t    _target_id;
        int32_t     _damage;
        int32_t     _target_current_hp;
    };

    struct CS_PACKET_LOGIN : PacketHeader
    {
		//name은 가변 길이로 , PacketStream에서 처리
	};

    struct SC_PACKET_ENTER : PacketHeader
    {
        int64_t _id; // long long
        char    _type;
        short   _x;
        short   _y;
		// name은 가변 길이로, PacketStream에서 처리
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
