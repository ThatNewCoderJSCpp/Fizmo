#include "fizmo_library.hpp"
#include "clock.hpp"

#ifdef OS_WINDOWS

namespace fizmo {
namespace time {
namespace detail {

std::uint64_t windows_uptime_ms() noexcept { return static_cast<std::uint64_t>(GetTickCount64()); }

std::uint64_t windows_filetime_now() noexcept {
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart  = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
}

void windows_performance_counter(std::uint64_t& counter, std::uint64_t& frequency) noexcept {
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    counter = static_cast<std::uint64_t>(c.QuadPart);
    frequency = static_cast<std::uint64_t>(f.QuadPart);
}

bool windows_time_increment(std::uint64_t& increment) noexcept {
    DWORD time_adjustment, time_increment;
    BOOL disabled;
    if (!GetSystemTimeAdjustment(&time_adjustment, &time_increment, &disabled)) return false;
    increment = static_cast<std::uint64_t>(time_increment);
    return true;
}

} // namespace detail
} // namespace time
} // namespace fizmo

#endif
