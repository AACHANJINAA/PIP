#include "pch.h"
#include "BotSession.h"

using namespace PIP::BOT;

void WorkerThread(HANDLE iocp)
{
    while (true)
    {
        DWORD bytes_transferred = 0;
        ULONG_PTR key = 0;
        OVERLAPPED* over = nullptr;

        if (GetQueuedCompletionStatus(iocp, &bytes_transferred, &key, &over, INFINITE))
        {
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
    for (int i = 0; i < std::thread::hardware_concurrency(); ++i) {
        io_threads.emplace_back(WorkerThread, iocp);
    }

    // Connect Thread
    std::thread connect_thread([&]() {
        for (auto& bot : bots) {
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
        while (true) {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - last_time).count();
            last_time = now;

            for (auto& bot : bots) {
                bot->Update(dt);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    });

    BOT_LOG("Stress Test Running. Press Enter to stop.");
    std::cin.get();

    WSACleanup();
    return 0;
}
