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
        mutable std::mutex mutex_; // 이동 후에도 뮤텍스는 유효해야 하므로 mutable
        std::condition_variable cv_;

    public:
        ConcurrentQueue() = default;
        ConcurrentQueue(const ConcurrentQueue&) = delete; // 복사 생성자 비활성화
        ConcurrentQueue& operator=(const ConcurrentQueue&) = delete; // 복사 할당 비활성화

        // 이동 생성자
        ConcurrentQueue(ConcurrentQueue&& other) noexcept
        {
            std::lock_guard<std::mutex> lock(other.mutex_);
            queue_ = std::move(other.queue_);
            // cv_는 이동 불가, mutex_는 이동할 필요 없음 (새로 생성된 것을 사용)
        }

        // 이동 할당 연산자
        ConcurrentQueue& operator=(ConcurrentQueue&& other) noexcept
        {
            if (this != &other)
            {
                std::unique_lock<std::mutex> lock_this(mutex_, std::defer_lock);
                std::unique_lock<std::mutex> lock_other(other.mutex_, std::defer_lock);
                std::lock(lock_this, lock_other); // 데드락 방지를 위해 std::lock 사용
                queue_ = std::move(other.queue_);
            }
            return *this;
        }

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


	
	
}