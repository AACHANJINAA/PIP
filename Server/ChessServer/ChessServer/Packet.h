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

    /*struct cs_packet_move
    {
        unsigned char   _size;
        PacketType     _type;
        char            _direction;
    };*/

    /*struct sc_packet_leave
    {
        unsigned char   _size;
        PacketType      _type;
        long long       _id;
    };*/

    /*struct sc_packet_move
    {
        unsigned char  _size;
        PacketType     _type;
        long long      _id;
        short          _x, _y;
    };*/

    /*struct sc_packet_avatar_info
    {
        unsigned char   _size;
        PacketType      _type;
        long long       _id;
        short           _x, _y;
        short           _hp;
        short           _level;
        int             _exp;
    };*/

    /*struct cs_packet_login
    {
        unsigned char   _size;
        PacketType     _type;
        char            _name[MAX_ID_LENGTH];
    };*/

    /* struct sc_packet_enter
    {
        unsigned char   _size;
        PacketType      _type;
        long long       _id;
        char            _name[MAX_ID_LENGTH];
        char            _o_type;
        short           _x, _y;
    };*/
#pragma pack (pop)
}
