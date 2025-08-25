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
#include <fstream>
#include <sstream> // std::ostringstream 추가

#include <DirectXMath.h>
#include <DirectXCollision.h>
using namespace DirectX;

#include "Packet.h"
#include "Vector3.h"
using namespace common;
#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "MSWSock.LIB")

#include "json.hpp"


// 1. 로그용 전역 뮤텍스는 그대로 사용합니다.
inline std::mutex g_log_mutex;

// 디버그 빌드에서만 로그가 동작하도록 설정
#ifdef _DEBUG
#define ENABLE_DEBUG_LOG // 주석 처리로 껏다켯다하면서 사용할것
#endif

// 2. 로그 레벨을 나타내는 열거형
enum class LogLevel { Info, Error };

// 3. 핵심 로거 클래스 정의
class Logger
{
public:
    Logger(LogLevel level, const char* file, int line)
        : _level(level), _file(file), _line(line)
    {}

    // 소멸자에서 잠금을 걸고 전체 로그 메시지를 출력
    ~Logger()
    {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        if (_level == LogLevel::Error)
        {
            std::cout << "[ERROR in ";
        }
        else
        {
            std::cout << "[";
        }
        std::cout << std::filesystem::path(_file).filename().string() << ":" << _line << "] "
            << _buffer.str() << std::endl;

        // 에러 레벨이면 디버거를 멈춤
        if (_level == LogLevel::Error)
        {
            __debugbreak();
        }
    }

    // `MYLOG << ...` 와 같은 스트림 연산을 가능하게 해주는 함수
    std::ostringstream& stream() { return _buffer; }

private:
    std::ostringstream _buffer;
    LogLevel _level;
    const char* _file;
    int _line;
};

#ifdef _DEBUG
	#ifdef ENABLE_DEBUG_LOG
		#define MYLOG(msg) Logger(LogLevel::Info, __FILE__, __LINE__).stream() << msg
		#define MYERROR(msg) Logger(LogLevel::Error, __FILE__, __LINE__).stream() << msg
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
