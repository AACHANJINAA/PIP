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
#include <string>
#include <stdexcept>
#include <cstdint>
#include <type_traits>
#include <filesystem>
#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "MSWSock.LIB")

#define ENABLE_DEBUG_LOG // 주석 처리로 껏다켯다하면서 사용할것

#ifdef ENABLE_DEBUG_LOG

// 파일명만 추출하는 헬퍼 (C++17 이상 필요)
#include <filesystem>
#define __FILENAME__ (std::filesystem::path(__FILE__).filename().string())

#define LOG_HELPER(file, line, message) std::cout << "[" << std::filesystem::path(file).filename().string() << ":" << line << "] " << message << std::endl
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
