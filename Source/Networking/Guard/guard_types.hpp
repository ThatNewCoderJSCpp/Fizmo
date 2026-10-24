#ifndef FIZMO_GUARD_TYPES_HPP
#define FIZMO_GUARD_TYPES_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace fizmo {
namespace networking {
namespace guard {

using SteadyClock = std::chrono::steady_clock;
using SteadyTime  = SteadyClock::time_point;
using WallClock   = std::chrono::system_clock;
using WallTime    = WallClock::time_point;
using NowFn       = std::function<SteadyTime()>;
using WallNowFn   = std::function<WallTime()>;

inline NowFn default_now() { return [] { return SteadyClock::now(); }; }
inline WallNowFn default_wall_now() { return [] { return WallClock::now(); }; }

std::string hash_secret(const std::string& secret);
std::string format_iso8601(WallTime t);
std::string json_escape(const std::string& s);

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_TYPES_HPP
