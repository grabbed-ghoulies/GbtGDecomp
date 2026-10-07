#include "gbtg/clock.h"
#include <bit>
#include <chrono>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace gbtg
{

namespace
{

std::int64_t host_performance_counter()
{
#ifdef _WIN32
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
#else
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

} // namespace

void initialize_performance_counter(PerformanceCounterState& counter)
{
#ifdef _WIN32
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    counter.frequency = frequency.QuadPart;
#else
    counter.frequency = 1000000000;
#endif
    counter.baseline = host_performance_counter();
}

// Xbox 0x001077D0. Unsigned subtraction preserves the original low-word
// subtraction and high-word borrow; the result is then interpreted as signed.
long double performance_counter_seconds(const PerformanceCounterState& counter)
{
    const std::uint64_t difference = std::uint64_t(host_performance_counter()) -
                                     std::uint64_t(counter.baseline);
    return static_cast<long double>(std::bit_cast<std::int64_t>(difference)) /
           static_cast<long double>(counter.frequency);
}

// Xbox 0x00032680. The deadline is rounded to float before the first poll.
void wait_performance_counter(const PerformanceCounterState& counter, float seconds)
{
    const float deadline = float(performance_counter_seconds(counter) + seconds);
    long double current;
    do
    {
        const std::uint64_t difference = std::uint64_t(host_performance_counter()) -
                                         std::uint64_t(counter.baseline);
        current = static_cast<long double>(std::bit_cast<std::int64_t>(difference)) /
                  static_cast<long double>(counter.frequency);
    } while (current < static_cast<long double>(deadline));
}

} // namespace gbtg

