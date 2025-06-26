#pragma once
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
