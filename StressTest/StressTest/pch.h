#pragma once

#define NOMINMAX
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

// Common Headers
#include "../../Common/Packet.h"
#include "../../Common/Vector3.h"
#include "../../Common/PacketStream.h"

// Logging Helper
inline std::mutex g_log_mutex;
#define BOT_LOG(msg) { \
    std::lock_guard<std::mutex> lock(g_log_mutex); \
    std::cout << "[Bot] " << msg << std::endl; \
}

namespace PIP::BOT
{
    enum IO_OP : std::uint8_t
    {
        IO_RECV = 0,
        IO_SEND = 1,
        IO_CONNECT = 2
    };

    struct OVERLAPPED_EX
    {
        WSAOVERLAPPED _over;
        IO_OP         _op;
        WSABUF        _wsabuf;
        char          _buffer[4096];

        OVERLAPPED_EX(IO_OP op) : _op(op) {
            ZeroMemory(&_over, sizeof(_over));
            _wsabuf.buf = _buffer;
            _wsabuf.len = sizeof(_buffer);
        }
    };
}
