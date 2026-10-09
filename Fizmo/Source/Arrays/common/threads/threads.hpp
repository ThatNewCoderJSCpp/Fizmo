#ifndef FIZMO_ARRAYS_THREADS_HPP
#define FIZMO_ARRAYS_THREADS_HPP

#include "../config.hpp"

#if defined(FIZMO_ARRAYS_FORCE_PORTABLE)
    #include "portable/threads.hpp"
#elif defined(FIZMO_ARRAYS_LINUX)
    #include "linux/threads.hpp"
#elif defined(FIZMO_ARRAYS_WINDOWS)
    #include "windows/threads.hpp"
#else
    #include "portable/threads.hpp"
#endif

#include <exception>

namespace fizmo {
namespace arrays {
namespace threads {

inline unsigned hardware_threads() noexcept { return platform::hardware_threads(); }
inline const char* backend_name() noexcept { return platform::name(); }

template <typename Fn>
struct TaskPack {
    Fn*                fn;
    std::size_t        index;
    std::exception_ptr error;
};

template <typename Fn>
inline void* run_task(void* p) {
    TaskPack<Fn>* t = static_cast<TaskPack<Fn>*>(p);
#if defined(FIZMO_ARRAYS_EXCEPTIONS)
    try { (*t->fn)(t->index); } catch (...) { t->error = std::current_exception(); }
#else
    (*t->fn)(t->index);
#endif
    return nullptr;
}

template <typename Fn>
inline void run(std::size_t count, Fn fn) {
    if (count == 0) return;
    if (count == 1) { fn(std::size_t(0)); return; }
    TaskPack<Fn> packs[64];
    platform::Thread handles[64];
    if (count > 64) count = 64;
    for (std::size_t i = 0; i < count; ++i) packs[i] = TaskPack<Fn>{ &fn, i, nullptr };
    std::size_t started = 0;
    for (std::size_t i = 1; i < count; ++i) {
        if (platform::start(handles[i], &run_task<Fn>, &packs[i])) ++started;
        else run_task<Fn>(&packs[i]);
    }
    run_task<Fn>(&packs[0]);
    for (std::size_t i = 1; i < count; ++i) platform::join(handles[i]);
    (void)started;
    for (std::size_t i = 0; i < count; ++i) if (packs[i].error) std::rethrow_exception(packs[i].error);
}

} // namespace threads
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_THREADS_HPP
