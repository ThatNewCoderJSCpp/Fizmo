#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "wait_sleep.hpp"

namespace fizmo {
namespace time {
namespace detail {

#if defined(OS_LINUX)
std::uint64_t linux_now_ns(clockid_t id) noexcept {
    struct timespec ts;
    if (::clock_gettime(id, &ts) != 0) return 0;
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts.tv_nsec);
}
#endif

} // namespace detail
} // namespace time
} // namespace fizmo

namespace fizmo {
namespace time {

void cpu_relax() noexcept {
#if defined(OS_WINDOWS)
    YieldProcessor();
#elif defined(ARCH_X86_64) || defined(ARCH_X86_32)
    __builtin_ia32_pause();
#elif defined(ARCH_ARM64) || defined(ARCH_ARMV7_OR_GREATER)
    __asm__ __volatile__("yield" ::: "memory");
#endif
}

} // namespace time
} // namespace fizmo

namespace fizmo {
namespace time {
namespace impl {

#if defined(OS_LINUX)
std::uint64_t mono_ns() noexcept {
    struct timespec ts;
    if (::clock_gettime(FIZMO_STEADY_CLOCK_ID, &ts) != 0) return 0;
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts.tv_nsec);
}
#endif

#if defined(OS_LINUX)
bool sleep_until_ns(std::uint64_t deadline_ns) noexcept {
    struct timespec ts;
    ts.tv_sec  = static_cast<time_t>(deadline_ns / 1000000000ULL);
    ts.tv_nsec = static_cast<long>(deadline_ns % 1000000000ULL);
    int rc;
    while ((rc = ::clock_nanosleep(FIZMO_STEADY_CLOCK_ID, TIMER_ABSTIME, &ts, nullptr)) == EINTR) {}
    return rc == 0;
}
#endif

bool do_sleep(std::uint64_t ms, std::uint64_t ns) noexcept {
#ifdef OS_WINDOWS
    if (ms == 0 && ns > 0) {
        Sleep(1);
        return true;
    }

    std::uint64_t remaining = ms;

    while (remaining > 0) {
        const DWORD chunk = (remaining > 0xFFFFFFFE) ? 0xFFFFFFFE : static_cast<DWORD>(remaining);
        Sleep(chunk);
        remaining -= chunk;
    }

    return true;
#elif defined(OS_LINUX)
    const std::uint64_t total_ns = (ns > 0) ? ns : ms * 1000000ULL;
    if (total_ns == 0) return true;
    return sleep_until_ns(mono_ns() + total_ns);
#else
    return false;
#endif
}

bool do_wait(std::uint64_t target_ns) noexcept {
#ifdef OS_WINDOWS
    LARGE_INTEGER freq, start, current;
    if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0) return false;
    if (!QueryPerformanceCounter(&start)) return false;
    const std::uint64_t target_ticks = (target_ns * freq.QuadPart) / 1000000000ULL;

    do {
        if (!QueryPerformanceCounter(&current)) return false;
    } while (static_cast<std::uint64_t>(current.QuadPart - start.QuadPart) < target_ticks);

    return true;
#elif defined(OS_LINUX)
    const std::uint64_t deadline = mono_ns() + target_ns;
    while (mono_ns() < deadline) cpu_relax();
    return true;
#else
    return false;
#endif
}

bool do_precise_wait(std::uint64_t total_ns) noexcept {
#ifdef OS_WINDOWS
    LARGE_INTEGER freq, start, current;
    if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0) return false;
    if (!QueryPerformanceCounter(&start)) return false;

    if (total_ns > 10000000ULL) {
        const std::uint64_t sleep_ms = (total_ns / 1000000ULL) - 2;
        if (sleep_ms > 0) do_sleep(sleep_ms, sleep_ms);
    }

    const std::uint64_t total_ticks = (total_ns * freq.QuadPart) / 1000000000ULL;

    do {
        if (!QueryPerformanceCounter(&current)) return false;
    } while (static_cast<std::uint64_t>(current.QuadPart - start.QuadPart) < total_ticks);

    return true;
#elif defined(OS_LINUX)
    const std::uint64_t deadline = mono_ns() + total_ns;
    const std::uint64_t guard = 250000ULL;      
    if (total_ns > guard * 2) sleep_until_ns(deadline - guard);
    while (mono_ns() < deadline) cpu_relax();
    return true;
#else
    return false;
#endif
}

} // namespace impl
} // namespace time
} // namespace fizmo

namespace fizmo {
namespace time {

bool yield() noexcept {
#ifdef OS_WINDOWS
    Sleep(0);
    return true;
#elif defined(OS_LINUX)
    return ::sched_yield() == 0;
#else
    return false;
#endif
}

} // namespace time
} // namespace fizmo
