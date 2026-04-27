#include "pch.h"
#include "PacketManager.h"
#include "PhysicsManager.h"
#include "server.h"


using namespace PIP;
int main()
{
	// I/O 스레드는 2개, 로직 스레드는 나머지 CPU 코어 수만큼 할당합니다.
	// (최소 1개의 로직 스레드는 보장)
	#include <timeapi.h>
	#pragma comment(lib, "winmm.lib")
	timeBeginPeriod(1);

	std::wcout.imbue(std::locale("korean"));
	std::vector<int> p_cores = GetPerformanceCoreIndices();

	int total_cores = p_cores.empty() ? static_cast<int>(std::thread::hardware_concurrency()) : static_cast<int>(p_cores.size());
	int io_worker_thread_count = 2;
	int logic_worker_thread_count = std::max(1, total_cores - io_worker_thread_count - 1);

	MYLOG("[System] Detected P-Cores (Logical): " << total_cores << ", Logic Threads: " << logic_worker_thread_count 
			<< ", DB Threads: " << 1 << std::endl);

	SERVER::Server::Instance()->initialize();
	// 서버 스탈트!
	SERVER::Server::Instance()->Start(io_worker_thread_count, logic_worker_thread_count);

	// 서버가 종료될 때까지 대기 (콘솔에서 Enter 키를 누르면 종료)
	std::cout << "Press Enter to stop the server..." << std::endl;
	std::cin.get();

	SERVER::Server::Instance()->Stop();
	WSACleanup();
	
	return 0;
}


