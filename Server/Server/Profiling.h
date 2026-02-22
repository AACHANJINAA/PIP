#pragma once
namespace PIP::SERVER
{
    // --- 성능 측정용 구조체 추가 ---
    struct ProfileData
    {
        std::atomic<long long> total_time_ns{ 0 };
        std::atomic<int> call_count{ 0 };
        std::atomic<long long> max_time_ns{ 0 };

        void add(long long ns);

        void reset();
        
    };

    struct PerformanceStats
    {
        ProfileData job_profile;
        ProfileData timer_profile;
        ProfileData physics_profile;
        ProfileData logic_profile;
        ProfileData total_loop_profile;
    };
}
