#ifndef FIZMO_ARRAYS_THREADS_WINDOWS_HPP
#define FIZMO_ARRAYS_THREADS_WINDOWS_HPP

#include "../../config.hpp"
#include "../../../../Basic/fizmo_defines.hpp"

namespace fizmo {
namespace arrays {
namespace threads {
namespace platform {

struct Thread {
    void* handle = nullptr;
};

unsigned hardware_threads() noexcept;
bool start(Thread& t, void* (*fn)(void*), void* arg) noexcept;
void join(Thread& t) noexcept;

inline const char* name() noexcept { return "win32"; }

} // namespace platform
} // namespace threads
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_THREADS_WINDOWS_HPP
