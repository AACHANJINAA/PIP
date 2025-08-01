#pragma once

namespace chess::packet{
    class PacketStream
    {
    public:
        PacketStream(size_t initialSize = 256);
        PacketStream(const char* data, size_t size);

		// Serialization 직렬화
        template <typename T>
            requires (std::is_trivially_copyable_v<T> && !std::is_same_v<T, std::string>)
        PacketStream& operator<<(const T& data)
        {
            const size_t dataSize = sizeof(T);
            _buffer.insert(_buffer.end(), reinterpret_cast<const char*>(&data), reinterpret_cast<const char*>(&data) + dataSize);
            return *this;
        }
        PacketStream& operator<<(const std::string& str);

        void Write(const char* data, size_t size);

        // Deserialization 역직렬화
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

        PacketStream& operator>>(std::string& str);
        

        const char* Data() const { return _buffer.data(); }
        char* mutable_data() { return _buffer.data(); } // [추가]
        size_t Size() const { return _buffer.size(); }
    private:
        std::vector<char> _buffer;
        size_t _pos;
    };
}
