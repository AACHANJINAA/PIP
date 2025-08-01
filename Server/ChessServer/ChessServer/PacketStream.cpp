#include "pch.h"
#include "PacketStream.h"
namespace chess::packet
{
	PacketStream::PacketStream(size_t initialSize) : _pos(0)
    {
        _buffer.reserve(initialSize);
    }

	PacketStream::PacketStream(const char* data, size_t size) : _buffer(data, data + size), _pos(0)
    {}

	PacketStream& PacketStream::operator<<(const std::string& str)
    {
        uint16_t len = static_cast<uint16_t>(str.length());
        *this << len;

        _buffer.insert(_buffer.end(), str.begin(), str.end());
        return *this;
    }

	void PacketStream::Write(const char* data, size_t size)
    {
        _buffer.insert(_buffer.end(), data, data + size);
    }

	PacketStream& PacketStream::operator>>(std::string& str)
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
}
