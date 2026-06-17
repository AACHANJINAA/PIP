#pragma once
#include "Packet.h"


namespace common::packet
{
    class PacketStream
    {
    public:
        PacketStream(size_t initialSize = 256) : _pos(0)
        {
            _buffer.reserve(initialSize);
        }

        PacketStream(const char* data, size_t size) : _buffer(data, data + size), _pos(0)
        {}

        // Serialization
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

        // Deserialization
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
            str.assign(reinterpret_cast<const char*>(&_buffer[_pos]), len);
            _pos += len;
            return *this;
        }
        PacketHeader PeekHeader() const
        {
            if (_buffer.size() < sizeof(PacketHeader))
            {
                return { 0, PacketType::error };
            }
            return *reinterpret_cast<const PacketHeader*>(_buffer.data());
        }
        const char* constable_data() const { return _buffer.data(); }
        char* mutable_data() { return _buffer.data(); }
        size_t Size() const { return _buffer.size(); }
        size_t Pos() const { return _pos; }
        void Clear()
        {
            _buffer.clear();
            _pos = 0;
        };

    private:
        std::vector<char> _buffer;
        size_t _pos;
    };

}
