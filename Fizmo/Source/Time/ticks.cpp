#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "ticks.hpp"

namespace fizmo {
namespace time {

std::uint64_t tick_frequency() noexcept {
#ifdef OS_WINDOWS
    static std::uint64_t freq = 0;
    if (freq == 0) {
        LARGE_INTEGER li;
        if (QueryPerformanceFrequency(&li) && li.QuadPart > 0) { freq = static_cast<std::uint64_t>(li.QuadPart); }
    }
    return freq;
#else
    return 1000000000ULL;
#endif
}

std::uint64_t tick_counter() noexcept {
#if defined(OS_WINDOWS)
    LARGE_INTEGER li;
    if (!QueryPerformanceCounter(&li)) return 0;
    return static_cast<std::uint64_t>(li.QuadPart);
#else
    return detail::linux_now_ns(FIZMO_STEADY_CLOCK_ID);
#endif
}

std::uint64_t wall_clock_ns() noexcept {
#if defined(OS_WINDOWS)
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart  = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) * 100ULL;
#else
    return detail::linux_now_ns(CLOCK_REALTIME);
#endif
}

std::uint64_t tick_resolution_ns() noexcept {
#if defined(OS_WINDOWS)
    const std::uint64_t f = tick_frequency();
    return f == 0 ? 0 : 1000000000ULL / f;
#else
    struct timespec ts;
    if (::clock_getres(FIZMO_STEADY_CLOCK_ID, &ts) != 0) return 0;
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts.tv_nsec);
#endif
}

std::uint64_t ns_to_ticks(std::uint64_t ns) noexcept {
    const std::uint64_t freq = tick_frequency();
    if (freq == 0) return 0ULL;
    return (ns * freq) / 1000000000ULL;
}

std::uint64_t ticks_to_ns(std::uint64_t ticks) noexcept {
    const std::uint64_t f = tick_frequency();
    if (f == 0) return 0;
    return (ticks / f) * 1000000000ULL + ((ticks % f) * 1000000000ULL) / f;
}

} // namespace time
} // namespace fizmo
