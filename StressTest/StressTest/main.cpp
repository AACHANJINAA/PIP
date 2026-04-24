#include "pch.h"
#include "BotSession.h"

using namespace PIP::BOT;

std::atomic<bool> g_stop{ false };

void WorkerThread(HANDLE iocp)
{
    while (!g_stop)
    {
        DWORD bytes_transferred = 0;
        ULONG_PTR key = 0;
        OVERLAPPED* over = nullptr;

        // 타임아웃(100ms)을 주어 g_stop 체크 기회를 제공
        if (GetQueuedCompletionStatus(iocp, &bytes_transferred, &key, &over, 100))
        {
            if (key == 0) continue; // 종료 시그널

            BotSession* session = reinterpret_cast<BotSession*>(key);
            OVERLAPPED_EX* over_ex = reinterpret_cast<OVERLAPPED_EX*>(over);

            if (bytes_transferred == 0)
            {
                session->Stop();
                if (over_ex->_op == IO_SEND) delete over_ex;
                continue;
            }

            switch (over_ex->_op)
            {
            case IO_RECV:
                session->OnRecv(bytes_transferred);
                break;
            case IO_SEND:
                session->OnSend(bytes_transferred);
                delete over_ex;
                break;
            }
        }
        else
        {
            if (over) {
                BotSession* session = reinterpret_cast<BotSession*>(key);
                session->Stop();
                OVERLAPPED_EX* over_ex = reinterpret_cast<OVERLAPPED_EX*>(over);
                if (over_ex->_op == IO_SEND) delete over_ex;
            }
        }
    }
}

int main()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    std::vector<std::shared_ptr<BotSession>> bots;
    const int BOT_COUNT = 400;
    const int ROOM_COUNT = 100;

    std::string server_ip = "127.0.0.1";
    short server_port = common::packet::SERVER_PORT;

    BOT_LOG("Native IOCP Stress Test Bot Client Starting...");
    
    for (int i = 0; i < BOT_COUNT; ++i) {
        bots.push_back(std::make_shared<BotSession>(30000 + i, i / 4));
    }

    // IO Threads
    std::vector<std::thread> io_threads;
    int io_thread_count = std::thread::hardware_concurrency();
    for (int i = 0; i < io_thread_count; ++i) {
        io_threads.emplace_back(WorkerThread, iocp);
    }

    // Connect Thread
    std::thread connect_thread([&]() {
        for (auto& bot : bots) {
            if (g_stop) break;
            if (!bot->Start(iocp, server_ip, server_port)) {
                BOT_LOG("Failed to connect bot.");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        BOT_LOG("All bots connection attempts finished.");
    });

    // Logic Thread (Update Loop)
    std::thread logic_thread([&]() {
        auto last_time = std::chrono::steady_clock::now();
        auto last_print_time = last_time;

        while (!g_stop) {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - last_time).count();
            last_time = now;

            uint64_t total_latency = 0;
            int ingame_bots = 0;
            std::unordered_map<BotState, int> state_counts;

            for (auto& bot : bots) {
                bot->Update(dt);
                BotState state = bot->GetState();
                state_counts[state]++;
                
                if (state == BotState::INGAME) {
                    total_latency += bot->GetLastLatency();
                    ingame_bots++;
                }
            }

            // 1초마다 상태 요약 및 평균 지연 시간 출력
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_print_time).count();
            if (elapsed_ms >= 1000) {
                std::lock_guard<std::mutex> lock(g_log_mutex);
                std::cout << "[Bot Status] Total: " << bots.size() 
                          << ", INGAME: " << ingame_bots 
                          << ", WAIT: " << state_counts[BotState::ROOM_WAIT]
                          << ", READY: " << state_counts[BotState::READY]
                          << ", LOGIN: " << state_counts[BotState::LOGGING_IN];

                if (ingame_bots > 0) {
                    float avg_latency = static_cast<float>(total_latency) / ingame_bots;
                    std::cout << " | Avg RTT: " << avg_latency << " ms";
                }
                std::cout << std::endl;

                last_print_time = now;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    });

    BOT_LOG("Stress Test Running. Press Enter to stop.");
    std::cin.get();

    // 종료 시작
    g_stop = true;

    // 모든 IO 스레드를 깨우기 위해 시그널 전송
    for (int i = 0; i < io_thread_count; ++i) {
        PostQueuedCompletionStatus(iocp, 0, 0, NULL);
    }

    // 모든 스레드 종료 대기
    if (connect_thread.joinable()) connect_thread.join();
    if (logic_thread.joinable()) logic_thread.join();
    for (auto& t : io_threads) {
        if (t.joinable()) t.join();
    }

    BOT_LOG("Stress Test Stopped Safely.");

    WSACleanup();
    return 0;
}
