#include "pch.h"
#include "Profiling.h"

namespace PIP::SERVER
{
	void ProfileData::add(long long ns)
	{
        total_time_ns += ns;
        ++call_count;
        long long current_max = max_time_ns.load();
        while (ns > current_max && !max_time_ns.compare_exchange_weak(current_max, ns));
    }

	void ProfileData::reset()
	{
        total_time_ns = 0;
        call_count = 0;
        max_time_ns = 0;
    }
}
