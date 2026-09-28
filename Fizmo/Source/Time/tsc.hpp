#ifndef FIZMO_TSC_UTIL_HPP
#define FIZMO_TSC_UTIL_HPP

#include "../Basic/basic_includes.hpp"

#if defined(FIZMO_TIME_USE_TSC) && defined(OS_LINUX)
    #if defined(ARCH_X86_64)
        #define FIZMO_TIME_TSC_BACKEND 1   // rdtsc
        #include <x86intrin.h>
        #include <cpuid.h>
    #elif defined(ARCH_ARM64)
        #define FIZMO_TIME_TSC_BACKEND 2   // cntvct_el0
    #else
        #define FIZMO_TIME_TSC_BACKEND 0
        #warning "FIZMO_TIME_USE_TSC: no cycle counter for this architecture; using CLOCK_MONOTONIC"
    #endif
#else
    #define FIZMO_TIME_TSC_BACKEND 0
#endif

#ifndef FIZMO_TIME_TSC_CALIBRATION_MS
    #define FIZMO_TIME_TSC_CALIBRATION_MS 10
#endif

namespace fizmo {
namespace time {

#if FIZMO_TIME_TSC_BACKEND
namespace detail {

inline std::uint64_t mono_ns_raw() noexcept {
    struct timespec ts;
    if (::clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts.tv_nsec);
}

#if FIZMO_TIME_TSC_BACKEND == 1   // x86-64 

inline std::uint64_t tsc_read() noexcept {
#ifdef FIZMO_TIME_TSC_NO_BARRIER
    return static_cast<std::uint64_t>(__rdtsc());
#else
    _mm_lfence();
    const std::uint64_t v = static_cast<std::uint64_t>(__rdtsc());
    _mm_lfence();
    return v;
#endif
}

inline bool tsc_is_invariant() noexcept {
    unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (__get_cpuid_max(0x80000000u, nullptr) < 0x80000007u) return false;
    if (!__get_cpuid(0x80000007u, &eax, &ebx, &ecx, &edx))    return false;
    return (edx & (1u << 8)) != 0;
}

inline std::uint64_t tsc_calibrate() noexcept {
    const std::uint64_t sleep_ns = static_cast<std::uint64_t>(FIZMO_TIME_TSC_CALIBRATION_MS) * 1000000ULL;
    struct timespec req, rem;
    req.tv_sec  = static_cast<time_t>(sleep_ns / 1000000000ULL);
    req.tv_nsec = static_cast<long>(sleep_ns % 1000000000ULL);
    const std::uint64_t t0 = mono_ns_raw();
    const std::uint64_t c0 = tsc_read();
    if (t0 == 0) return 0;
    while (::nanosleep(&req, &rem) != 0 && errno == EINTR) req = rem;
    const std::uint64_t c1 = tsc_read();
    const std::uint64_t t1 = mono_ns_raw();
    if (t1 <= t0 || c1 <= c0) return 0;
    const long double hz = static_cast<long double>(c1 - c0) * 1000000000.0L / static_cast<long double>(t1 - t0);
    if (hz < 1.0e6L || hz > 1.0e11L) return 0;   
    return static_cast<std::uint64_t>(hz + 0.5L);
}

#elif FIZMO_TIME_TSC_BACKEND == 2   // AArch64 

inline std::uint64_t tsc_read() noexcept {
    std::uint64_t v;
#ifdef FIZMO_TIME_TSC_NO_BARRIER
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(v) :: "memory");
#else
    __asm__ __volatile__("isb\n\tmrs %0, cntvct_el0" : "=r"(v) :: "memory");
#endif
    return v;
}

inline bool tsc_is_invariant() noexcept { return true; }

inline std::uint64_t tsc_calibrate() noexcept {
    std::uint64_t f;
    __asm__ __volatile__("mrs %0, cntfrq_el0" : "=r"(f));
    return f;   
}

#endif

struct tsc_state {
    std::uint64_t hz;
    bool          usable;
};

inline const tsc_state& tsc_info() noexcept {
    static const tsc_state s = []() noexcept -> tsc_state {
#ifdef FIZMO_TIME_TSC_HZ
        return tsc_state{ static_cast<std::uint64_t>(FIZMO_TIME_TSC_HZ), true };
#else
        if (!tsc_is_invariant()) return tsc_state{ 0, false };
        const std::uint64_t hz = tsc_calibrate();
        return tsc_state{ hz, hz != 0 };
#endif
    }();
    return s;
}

} // namespace detail

inline bool          tsc_active() noexcept { return detail::tsc_info().usable; }
inline std::uint64_t tsc_hz()     noexcept { return detail::tsc_info().hz; }
#else
inline bool          tsc_active() noexcept { return false; }
inline std::uint64_t tsc_hz()     noexcept { return 0; }
#endif

inline void tsc_warmup() noexcept { (void)tsc_active(); }

#if defined(FIZMO_TIME_USE_TSC) && defined(OS_LINUX) && defined(ARCH_X86_64)
inline std::uint64_t cycle_counter() noexcept {
    _mm_lfence();
    const std::uint64_t v = static_cast<std::uint64_t>(__rdtsc());
    _mm_lfence();
    return v;
}

inline bool cycle_counter_available() noexcept { return detail::tsc_is_invariant(); }
#endif

} // namespace time
} // namespace fizmo

#endif // FIZMO_TSC_UTIL_HPP