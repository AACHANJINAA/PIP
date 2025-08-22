#pragma once
#define NOMINMAX
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <iostream>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <thread>
#include <array>
#include <ranges>
#include <vector>
#include <concurrent_unordered_map.h>
#include <concurrent_queue.h>
#include <mutex>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <type_traits>
#include <filesystem>
#include <DirectXMath.h>
#include <DirectXCollision.h>
using namespace DirectX;

#include "Packet.h"
#include "Vector3.h"
using namespace common;
#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "MSWSock.LIB")

#define ENABLE_DEBUG_LOG // 주석 처리로 껏다켯다하면서 사용할것


#ifdef _DEBUG
	#ifdef ENABLE_DEBUG_LOG
		#define __FILENAME__ (std::filesystem::path(__FILE__).filename().string())

		#define MYLOG_HELPER(file, line, message) std::cout << "[" << std::filesystem::path(file).filename().string() << ":" << line << "] " << message << std::endl
		#define MYLOG(message) MYLOG_HELPER(__FILE__, __LINE__, message)
		#define MYERROR(message) do { MYLOG_HELPER(__FILE__, __LINE__, message); __debugbreak(); } while(0)
	#else
		#define MYLOG(message)
		#define MYERROR(message)
	#endif // ENABLE_DEBUG_LOG
#else 
	#define MYLOG(message)
	#define MYERROR(message)
#endif // DEBUG

template <typename T>
class Singleton
{
protected:
	Singleton() = default;
	virtual ~Singleton() = default;

public:
	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;

	static T* Instance()
	{
		static T instance;
		return &instance;
	}
};

namespace chess
{
	extern void print_error(const char* msg, int err_no);
}
