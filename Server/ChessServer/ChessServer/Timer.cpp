#include "pch.h"
#include "Timer.h"

#include "server.h"

namespace PIP::server
{
	Timer::~Timer()
	{
		if (_isRunning)
		{
			Stop();
		}
	}

	void Timer::Initialize()
	{
		_isRunning = true;
		// 멤버 함수를 스레드로 실행하기 위해선, 어떤 객체의 멤버 함수인지 알려줘야 함 (this)
		_timerThread = std::thread(&Timer::Run, this);
	}

	void Timer::Stop()
	{
		_isRunning = false;
		_cv.notify_one(); // 대기 중인 스레드를 깨워서 종료 처리

		if (_timerThread.joinable())
		{
			_timerThread.join();
		}
	}

	void Timer::AddTimerJob(milliseconds delay, std::function<void()> task)
	{
		TimerJob newJob;
		newJob._execute_time = std::chrono::steady_clock::now() + delay;
		newJob._task = std::move(task);

		{
			std::lock_guard<std::mutex> lock(_mutex);
			_jobQueue.push(std::move(newJob));
		}

		// 대기 중인 Run 스레드를 깨워서, 새로 추가된 작업이 더 빠른지 확인하도록 함
		_cv.notify_one();
	}

	void Timer::Run()
	{
		static std::atomic<int> balancer = 0; // 로직 스레드에 작업을 분배하기 위한 카운터

		while (_isRunning)
		{
			std::unique_lock<std::mutex> lock(_mutex);

			if (_jobQueue.empty())
			{
				// 큐가 비었으면 새 작업이 추가되거나 Stop()이 호출될 때까지 무한정 대기
				_cv.wait(lock, [this]() { return !_isRunning || !_jobQueue.empty(); });
			}
			else
			{
				// 큐에 작업이 있으면, 가장 가까운 작업의 실행 시간까지 대기
				auto& topJob = _jobQueue.top();
				_cv.wait_until(lock, topJob._execute_time, [this]() { return !_isRunning || !_jobQueue.empty(); });
			}

			if (!_isRunning) break;
			if (_jobQueue.empty()) continue;

			auto& topJob = _jobQueue.top();
			if (topJob._execute_time > std::chrono::steady_clock::now())
			{
				continue;
			}

			TimerJob jobToExecute = std::move(const_cast<TimerJob&>(topJob));
			_jobQueue.pop();

			lock.unlock();

			// --- 작업 분배 ---
			// 로직 스레드 개수를 가져와서 라운드-로빈 방식으로 분배
			int workerCount = server::Server::Instance()->GetLogicWorkerCount();
			if (workerCount > 0)
			{
				int workerIdx = balancer.fetch_add(1) % workerCount;
				server::Server::Instance()->get_logic_queue(workerIdx)->push({ std::move(jobToExecute._task) });
			}
		}
	}
}