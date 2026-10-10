#ifndef FIZMO_ARRAYS_THREADS_PORTABLE_HPP
#define FIZMO_ARRAYS_THREADS_PORTABLE_HPP

#include "../../config.hpp"

namespace fizmo {
namespace arrays {
namespace threads {
namespace platform {

inline unsigned hardware_threads() noexcept { return 1; }
struct Thread { bool started = false; };
inline bool start(Thread&, void* (*)(void*), void*) noexcept { return false; }
inline void join(Thread&) noexcept {}
inline const char* name() noexcept { return "none"; }

} // namespace platform
} // namespace threads
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_THREADS_PORTABLE_HPP
