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
#include <mutex>

#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "MSWSock.LIB")

#define ENABLE_DEBUG_LOG // 주석 처리로 껏다켯다하면서 사용할것

#ifdef ENABLE_DEBUG_LOG

#define LOG_HELPER(file, line, message) std::cout << "[" << file << ":" << line << "] " << message << std::endl
#define LOG(message) LOG_HELPER(__FILE__, __LINE__, message)
#define ERROR(message) do { LOG_HELPER(__FILE__, __LINE__, message); __debugbreak(); } while(0)

#else 
#define LOG(message)
#define ERROR(message)
#endif
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
