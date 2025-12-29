#include "pch.h"
#include "PacketManager.h"
#include "PhysicsManager.h"
#include "server.h"

using namespace PIP;
int main()
{
    // PhysicsManager 초기화 (Jolt 콜백 등록 및 팩토리 생성)

	//// ================= Jolt 테스트 코드 시작 =================
    // I/O 스레드는 2개, 로직 스레드는 나머지 CPU 코어 수만큼 할당합니다.
	// (최소 1개의 로직 스레드는 보장)
	int total_cores = static_cast<int>(std::thread::hardware_concurrency());
	int logic_worker_thread_count = std::max(1, total_cores - 2);
	int io_worker_thread_count = 2;

    PIP::PhysicsManager::Instance()->Initialize();
    // 서버 스탈트!
    server::Server::Instance()->Start(io_worker_thread_count, logic_worker_thread_count);

    // 서버가 종료될 때까지 대기 (콘솔에서 Enter 키를 누르면 종료)
	MYLOG("Press Enter to stop the server..." << std::endl);
    std::cin.get();

    server::Server::Instance()->Stop();
    WSACleanup();
    
    return 0;
}


