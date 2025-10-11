#include "pch.h"
#include "PacketManager.h"
#include "server.h"

namespace PIP::server
{
    HANDLE g_iocp = nullptr;
    std::atomic<int> g_new_id = 0;
}


using namespace PIP;
int main()
{
    std::wcout.imbue(std::locale("korean"));
    WSAData wsadata;
    if(WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
    {
		return -1; // WSAStartup 실패
    }

    PIP::packet::PacketManager::Instance()->Initialize();
	MYLOG("PacketManager Initialized." << std::endl);


    // I/O 스레드는 2개, 로직 스레드는 나머지 CPU 코어 수만큼 할당합니다.
	// (최소 1개의 로직 스레드는 보장)
	int total_cores = static_cast<int>(std::thread::hardware_concurrency() * 1.5);
	int worker_thread_count = std::max(1, total_cores - 2);
	int io_thread_count = 2;

    // 서버 스탈트!
    server::Server::Instance()->Start(io_thread_count, worker_thread_count);

    // 서버가 종료될 때까지 대기 (콘솔에서 Enter 키를 누르면 종료)
	std::cout << "Press Enter to stop the server..." << std::endl;
    std::cin.get();

    server::Server::Instance()->Stop();
    WSACleanup();
    
    return 0;
}


