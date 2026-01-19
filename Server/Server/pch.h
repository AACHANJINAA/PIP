#pragma once
#define NOMINMAX
// 윈도우 헤더
#include <WS2tcpip.h>
#include <MSWSock.h>
#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "MSWSock.LIB")

// STL 헤더
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
#include <concurrent_priority_queue.h>
#include <mutex>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <type_traits>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <queue>
#include <algorithm>
#include <variant>
#include <typeindex>

//LUA 헤더
extern "C" {
#include "lua-5.4.2_Win64_dll17_lib/include/lua.h"
#include "lua-5.4.2_Win64_dll17_lib/include/lualib.h"
#include "lua-5.4.2_Win64_dll17_lib/include/lauxlib.h"
}

// DirectX 헤더
#include <DirectXMath.h>
#include <DirectXCollision.h>
using namespace DirectX;

// Common 헤더
#include "Packet.h"
#include "Vector3.h"
#include "PacketStream.h"


// JSON 헤더
#include "json.hpp"

// Jolt Physics 헤더
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h> // 단일 스레드 모드
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h> // 지형용
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Character/CharacterVirtual.h> // 캐릭터 컨트롤러용
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

// Jolt 관련 편의를 위한 using
using namespace JPH::literals; // 1.0_r 같은 리터럴 사용 시

// 1. 로그용 전역 뮤텍스는 그대로 사용합니다.
inline std::mutex g_log_mutex;

// 디버그 빌드에서만 로그가 동작하도록 설정
#ifdef _DEBUG
#define ENABLE_DEBUG_LOG // 주석 처리로 껏다켯다하면서 사용할것
#endif

#ifdef _DEBUG
	#ifdef ENABLE_DEBUG_LOG
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

namespace PIP
{
	extern void print_error(const char* msg, int err_no);
}



// OS 별 헤더 포함
#ifdef _WIN32
	#include <windows.h>
	#include <processthreadsapi.h>
#else
	#include <pthread.h>
	#include <sched.h>
	#include <unistd.h>
	#include <fstream>
	#include <string>
	#include <filesystem>
#endif

// 로깅용
inline void core_log(const std::string& msg) {
	MYLOG("[System] " << msg << std::endl;)
}

// 고성능 코어의 인덱스(Logical Processor ID) 목록을 가져오는 함수
inline std::vector<int> GetPerformanceCoreIndices() {
    std::vector<int> p_cores;

#ifdef _WIN32
    // Windows 10/11 방식 (Intel Hybrid, AMD, ARM64 지원)
    ULONG returnLength = 0;
    GetSystemCpuSetInformation(nullptr, 0, &returnLength, GetConsoleWindow(), 0);

    if (returnLength == 0) return {}; // 실패 시 빈 벡터 반환

    std::vector<char> buffer(returnLength);
    PSYSTEM_CPU_SET_INFORMATION cpuSets = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.data());

    if (!GetSystemCpuSetInformation(cpuSets, returnLength, &returnLength, GetConsoleWindow(), 0)) {
        return {};
    }

    // 1단계: 가장 높은 EfficiencyClass(성능 등급) 값을 찾음
    int maxEfficiencyClass = -1;
    int count = returnLength / sizeof(SYSTEM_CPU_SET_INFORMATION);

    for (int i = 0; i < count; ++i) {
        if (cpuSets[i].Type == CpuSetInformation) {
            if ((int)cpuSets[i].CpuSet.EfficiencyClass > maxEfficiencyClass) {
                maxEfficiencyClass = (int)cpuSets[i].CpuSet.EfficiencyClass;
            }
        }
    }

    // 2단계: 가장 높은 등급의 코어 ID만 수집 (이것들이 P코어/Big코어)
    for (int i = 0; i < count; ++i) {
        if (cpuSets[i].Type == CpuSetInformation) {
            if (cpuSets[i].CpuSet.EfficiencyClass == maxEfficiencyClass) {
                p_cores.push_back(cpuSets[i].CpuSet.Id);
            }
        }
    }

#else
    // Linux / Android (ARM big.LITTLE, Intel Hybrid, AMD)
    // /sys/devices/system/cpu/cpuN/cpu_capacity 값을 읽어서 판단
    // capacity가 없으면 동종(Homogeneous) CPU로 간주

    std::vector<std::pair<int, int>> core_capacities; // {core_id, capacity}
    int max_capacity = -1;
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);

    for (int i = 0; i < num_cpus; ++i) {
        std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpu_capacity";
        std::ifstream file(path);
        int capacity = 0;

        if (file.is_open()) {
            file >> capacity;
        }
        else {
            // capacity 파일이 없으면(구형 커널 등) 모든 코어를 동일하게 취급
            capacity = 1024;
        }

        if (capacity > max_capacity) max_capacity = capacity;
        core_capacities.push_back({ i, capacity });
    }

    // 가장 높은 capacity를 가진 코어만 추출
    for (const auto& pair : core_capacities) {
        // 약간의 오차 범위를 고려할 수도 있으나, 보통 정확히 일치함
        if (pair.second >= max_capacity) {
            p_cores.push_back(pair.first);
        }
    }
#endif

    return p_cores;
}

// 현재 스레드를 고성능 코어에만 할당하는 함수
inline bool PinThreadToPerformanceCores() {
    std::vector<int> p_cores = GetPerformanceCoreIndices();

    if (p_cores.empty()) {
        core_log("고성능 코어를 식별할 수 없습니다. 기본 스케줄러를 사용합니다.");
        return false;
    }

    std::string core_list_str = "";
    for (int id : p_cores) core_list_str += std::to_string(id) + " ";
    core_log("식별된 고성능 코어 ID: " + core_list_str);

#ifdef _WIN32
    // Windows: Affinity Mask 설정
    DWORD_PTR mask = 0;
    for (int core_id : p_cores) {
        // 64코어 이상인 경우 Processor Group 처리가 추가로 필요하지만,
        // 일반적인 데스크탑/모바일 환경을 가정하여 단순 마스크 사용
        if (core_id < 64) {
            mask |= (static_cast<DWORD_PTR>(1) << core_id);
        }
    }

    HANDLE hThread = GetCurrentThread();
    DWORD_PTR result = SetThreadAffinityMask(hThread, mask);
    return (result != 0);

#else
    // Linux: pthread_setaffinity_np 설정
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    for (int core_id : p_cores) {
        CPU_SET(core_id, &cpuset);
    }

    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    return (rc == 0);
#endif
}