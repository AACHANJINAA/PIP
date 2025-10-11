#pragma once


namespace PIP::server
{
	using namespace std::chrono;
	struct TimerJob
	{
		steady_clock::time_point	_execute_time;
		std::function<void()>		_task;
		bool operator>(const TimerJob& other) const
		{
			return _execute_time > other._execute_time;
		}
	};
	class Timer : public Singleton<Timer>
	{
		friend class Singleton<Timer>;
	private:
		Timer() = default;
		~Timer();
	public:
		void Initialize();
		void Stop();

		void AddTimerJob(milliseconds delay, std::function<void()> task);
	private:
		void Run();
	private:
		std::atomic<bool> _isRunning = false;
		std::thread _timerThread;

		// min-heap
		std::priority_queue<TimerJob, std::vector<TimerJob>, std::greater<TimerJob>> _jobQueue;

		std::mutex _mutex;
		std::condition_variable _cv;
	};
}


