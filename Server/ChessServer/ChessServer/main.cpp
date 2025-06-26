#include "pch.h"
#include "CommonHeader.h"
#include "Packet.h"
#include "PacketManager.h"
#include "server.h"

constexpr short SERVER_PORT = 3000; // 내가 가지고 있어야함

namespace chess
{
    HANDLE g_iocp = nullptr;
    SOCKET g_s_socket = INVALID_SOCKET;
    std::atomic<int> g_new_id = 0;
    concurrency::concurrent_unordered_map< long long, std::shared_ptr<server::SESSION>> g_users;
}


using namespace chess;
int main()
{
    std::wcout.imbue(std::locale("korean"));
    WSAData wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    // 1. IOCP 생성
    g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    // 2. 리슨 소켓 생성 및 IOCP와 연결
    g_s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
    CreateIoCompletionPort(reinterpret_cast<HANDLE>(chess::g_s_socket), chess::g_iocp, 0, 0);
    // ... bind, listen ...
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY; // 모든 인터페이스에서 수신
	bind(g_s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(g_s_socket, SOMAXCONN);

    // <<< 여기에 PacketManager 초기화 코드를 추가합니다 >>>
    packet::PacketManager::Instance()->Initialize();
    std::cout << "PacketManager Initialized." << std::endl;

    // 3. 워커 스레드 생성
    std::vector<std::thread> worker_threads;
    int num_threads = std::thread::hardware_concurrency();
    for (int i = 0; i < num_threads; ++i)
    {
        worker_threads.emplace_back(server::worker);
    }

    // 4. 최초의 비동기 Accept 요청
    do_accept(g_s_socket, server::g_accept_over);

    std::cout << "Server Started. Listening on port " << packet::SERVER_PORT << std::endl;

    // 5. 메인 스레드는 워커 스레드들이 종료될 때까지 대기
    for (auto& th : worker_threads)
    {
        th.join();
    }

    closesocket(g_s_socket);
    WSACleanup();
    return 0;
}


