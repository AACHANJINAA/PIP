#pragma once
#include "pch.h"
#include <queue>
#include <mutex>
#include <condition_variable>

namespace chess
{
	extern HANDLE g_iocp;
	extern SOCKET g_s_socket;
	extern std::atomic<int> g_new_id;

    template<typename T>
    class ConcurrentQueue
    {
    private:
        std::queue<T> queue_;
        std::mutex mutex_;
        std::condition_variable cv_;

    public:
        void Push(T value)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(value));
            cv_.notify_one();
        }

        bool TryPop(T& value)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty())
            {
                return false;
            }
            value = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        void WaitPop(T& value)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty(); });
            value = std::move(queue_.front());
            queue_.pop();
        }
    };
	namespace packet
	{
		struct PositionPacket;
	}

	namespace server
	{
		class SESSION;
	}

	extern std::unordered_map<long long, chess::packet::PositionPacket> g_positions;
	extern concurrency::concurrent_unordered_map< long long, std::shared_ptr<server::SESSION>> g_users;

	
	
}