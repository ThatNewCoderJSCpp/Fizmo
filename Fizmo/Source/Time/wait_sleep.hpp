#ifndef FIZMO_CHRONO_WAIT_SLEEP_HPP
#define FIZMO_CHRONO_WAIT_SLEEP_HPP

#include "date_time_util.hpp"

namespace fizmo {
namespace time {

template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
void sleep(const T time_value) {
#ifdef OS_WINDOWS
    Duration dur(time_value);
    const millisecond ms = dur.to_milliseconds();
    
    if (ms.value() == 0 && dur.to_nanoseconds().value() > 0) {
        Sleep(1);
        return;
    }
    
    std::uint64_t remaining_ms = ms.value();

    while (remaining_ms > 0) {
        const DWORD sleep_ms = (remaining_ms > 0xFFFFFFFE) ? 0xFFFFFFFE : static_cast<DWORD>(remaining_ms);
        Sleep(sleep_ms);
        remaining_ms -= sleep_ms;
    }
#endif
}

void sleep(DURATION_PARAM dur) {
#ifdef OS_WINDOWS
    const millisecond ms = dur.to_milliseconds();
    std::uint64_t remaining_ms = ms.value();

    if (remaining_ms == 0 && dur.to_nanoseconds().value() > 0) {
        Sleep(1);
        return;
    }

    while (remaining_ms > 0) {
        const DWORD sleep_ms = (remaining_ms > 0xFFFFFFFE) ? 0xFFFFFFFE : static_cast<DWORD>(remaining_ms);
        Sleep(sleep_ms);
        remaining_ms -= sleep_ms;
    }
#endif
}

void sleep(const CompleteDuration& dur) { sleep(dur.to_millisecond()); }

template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
void wait(const T time_value) {
#ifdef OS_WINDOWS
    Duration dur(time_value);
    const nanosecond target_ns = dur.to_nanoseconds();
    LARGE_INTEGER freq;
    LARGE_INTEGER start;
    LARGE_INTEGER current;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    const std::uint64_t target_ticks = (target_ns.value() * freq.QuadPart) / 1000000000;
    
    do {
        QueryPerformanceCounter(&current);
    } while (static_cast<std::uint64_t>(current.QuadPart - start.QuadPart) < target_ticks);
#endif
}

void wait(DURATION_PARAM dur) {
#ifdef OS_WINDOWS
    const nanosecond target_ns = dur.to_nanoseconds();
    LARGE_INTEGER freq;
    LARGE_INTEGER start;
    LARGE_INTEGER current;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    const std::uint64_t target_ticks = (target_ns.value() * freq.QuadPart) / 1000000000;

    do {
        QueryPerformanceCounter(&current);
    } while (static_cast<std::uint64_t>(current.QuadPart - start.QuadPart) < target_ticks);
#endif
}

void wait(const CompleteDuration& dur) { wait(dur.to_nanosecond()); }

template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
void precise_wait(const T time_value) {
#ifdef OS_WINDOWS
    Duration dur(time_value);
    const nanosecond total_ns = dur.to_nanoseconds();
    LARGE_INTEGER freq, start, current;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);  
    
    if (total_ns.value() > 10000000) { 
        const millisecond sleep_ms((total_ns.value() / 1000000) - 2); 
        if (sleep_ms.value() > 0) { sleep(sleep_ms); }
    }
    
    QueryPerformanceCounter(&current);
    const std::uint64_t elapsed_ns = ((current.QuadPart - start.QuadPart) * 1000000000ULL) / freq.QuadPart;
    const std::uint64_t remaining_ns = (total_ns.value() > elapsed_ns) ? (total_ns.value() - elapsed_ns) : 0;
    
    if (remaining_ns > 0) {
        const std::uint64_t target_ticks = (remaining_ns * freq.QuadPart) / 1000000000ULL;
        std::uint64_t elapsed_ticks;
        
        do {
            QueryPerformanceCounter(&current);
            elapsed_ticks = static_cast<std::uint64_t>(current.QuadPart - start.QuadPart);
        } while (elapsed_ticks < ((total_ns.value() * freq.QuadPart) / 1000000000ULL));
    }
#endif
}

void precise_wait(DURATION_PARAM dur) {
#ifdef OS_WINDOWS
    const nanosecond total_ns = dur.to_nanoseconds();

    if (total_ns.value() > 10000000) {
        const millisecond sleep_ms((total_ns.value() / 1000000) - 2);
        if (sleep_ms.value() > 0) { sleep(sleep_ms); }
    }

    LARGE_INTEGER freq, start, current;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    const std::uint64_t elapsed_ns = ((current.QuadPart - start.QuadPart) * 1000000000) / freq.QuadPart;
    const std::uint64_t remaining_ns = (total_ns.value() > elapsed_ns) ? (total_ns.value() - elapsed_ns) : 0;

    if (remaining_ns > 0) {
        const std::uint64_t target_ticks = (remaining_ns * freq.QuadPart) / 1000000000;
        std::uint64_t elapsed_ticks;

        do {
            QueryPerformanceCounter(&current);
            elapsed_ticks = static_cast<std::uint64_t>(current.QuadPart - start.QuadPart);
        } while (elapsed_ticks < target_ticks);
    }
#endif
}

void precise_wait(const CompleteDuration& dur) { precise_wait(dur.to_nanosecond()); }

inline void yield() {
#ifdef OS_WINDOWS
    Sleep(0);  
#endif
}
    
} // namespace time
} // namespace fizmo

#endif // FIZMO_CHRONO_WAIT_SLEEP_HPP