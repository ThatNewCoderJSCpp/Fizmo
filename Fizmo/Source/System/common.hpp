#ifndef FIZMO_SYSTEM_COMMON_HPP
#define FIZMO_SYSTEM_COMMON_HPP

#include "../Basic/fizmo_defines.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace fizmo {
namespace system {

struct Statistics {
    std::size_t count   = 0;
    double      min     = 0.0;
    double      max     = 0.0;
    double      average = 0.0;
    double      p95     = 0.0;
    double      last    = 0.0;
};

struct FrameSummary {
    std::uint64_t frames            = 0;
    double        fps               = 0.0;
    double        frame_ms          = 0.0;
    double        frame_ms_min      = 0.0;
    double        frame_ms_max      = 0.0;
    double        frame_ms_p99      = 0.0;
    double        low_1_percent_fps = 0.0;
    double        cpu_ms            = 0.0;
    double        cpu_ms_max        = 0.0;
    double        present_ms        = 0.0;
    double        present_ms_max    = 0.0;
    bool          gpu_valid         = false;
    double        gpu_ms            = 0.0;
    double        gpu_ms_max        = 0.0;
};

namespace detail {

using Clock = std::chrono::steady_clock;

inline double seconds_between(Clock::time_point a, Clock::time_point b) noexcept {
    return std::chrono::duration<double>(b - a).count();
}

inline double per_second(std::uint64_t now, std::uint64_t before, double seconds) noexcept {
    if (seconds <= 0.0 || now < before) return 0.0;
    return static_cast<double>(now - before) / seconds;
}

inline double clamp_percent(double v) noexcept {
    if (!(v > 0.0)) return 0.0;
    return v > 100.0 ? 100.0 : v;
}

 std::string trim(const std::string& s);

inline std::string lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

inline bool starts_with(const std::string& s, const char* prefix) noexcept {
    const std::size_t n = std::strlen(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

 bool ends_with(const std::string& s, const char* suffix) noexcept;

 std::vector<std::string> split_ws(const std::string& s);

 std::vector<std::string> split_lines(const std::string& s);

 std::optional<std::uint64_t> parse_u64(const std::string& s, int base = 10) noexcept;

 std::optional<std::int64_t> parse_i64(const std::string& s) noexcept;

 std::optional<double> parse_double(const std::string& s) noexcept;

inline std::string fixed(double v, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

inline void push_value(std::vector<double>& out, double v) { if (std::isfinite(v)) out.push_back(v); }
inline void push_value(std::vector<double>& out, float v) { push_value(out, static_cast<double>(v)); }

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
inline void push_value(std::vector<double>& out, T v) { out.push_back(static_cast<double>(v)); }

template <typename T>
inline void push_value(std::vector<double>& out, const std::optional<T>& v) { if (v) push_value(out, *v); }

 Statistics summarize(std::vector<double> values);

} // namespace detail

 std::string format_bytes(std::uint64_t bytes);

 std::string format_rate(double bytes_per_second);

 std::string format_bitrate(double bytes_per_second);

inline std::string format_optional(const std::optional<double>& v, int decimals, const char* unit) {
    if (!v) return "n/a";
    return detail::fixed(*v, decimals) + unit;
}

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_COMMON_HPP
