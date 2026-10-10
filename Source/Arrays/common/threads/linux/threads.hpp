#ifndef FIZMO_ARRAYS_THREADS_LINUX_HPP
#define FIZMO_ARRAYS_THREADS_LINUX_HPP

#include "../../config.hpp"
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

namespace fizmo {
namespace arrays {
namespace threads {
namespace platform {

inline unsigned hardware_threads() noexcept {
#if defined(CPU_COUNT)
    cpu_set_t set;
    if (::sched_getaffinity(0, sizeof(set), &set) == 0) {
        const int c = CPU_COUNT(&set);
        if (c > 0) return static_cast<unsigned>(c);
    }
#endif
    const long n = ::sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? static_cast<unsigned>(n) : 1u;
}

struct Thread {
    pthread_t handle{};
    bool      started = false;
};

inline bool start(Thread& t, void* (*fn)(void*), void* arg) noexcept {
    pthread_attr_t attr;
    if (::pthread_attr_init(&attr) != 0) return false;
    ::pthread_attr_setstacksize(&attr, std::size_t(1) << 20);
    t.started = ::pthread_create(&t.handle, &attr, fn, arg) == 0;
    ::pthread_attr_destroy(&attr);
    return t.started;
}

inline void join(Thread& t) noexcept {
    if (t.started) ::pthread_join(t.handle, nullptr);
    t.started = false;
}

inline const char* name() noexcept { return "pthreads"; }

} // namespace platform
} // namespace threads
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_THREADS_LINUX_HPP
