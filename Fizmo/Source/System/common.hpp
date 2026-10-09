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

inline std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && static_cast<unsigned char>(s[a]) <= ' ') ++a;
    while (b > a && static_cast<unsigned char>(s[b - 1]) <= ' ') --b;
    return s.substr(a, b - a);
}

inline std::string lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

inline bool starts_with(const std::string& s, const char* prefix) noexcept {
    const std::size_t n = std::strlen(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

inline bool ends_with(const std::string& s, const char* suffix) noexcept {
    const std::size_t n = std::strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

inline std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out;
    std::size_t i = 0;

    while (i < s.size()) {
        while (i < s.size() && static_cast<unsigned char>(s[i]) <= ' ') ++i;
        const std::size_t start = i;
        while (i < s.size() && static_cast<unsigned char>(s[i]) > ' ') ++i;
        if (i > start) out.push_back(s.substr(start, i - start));
    }

    return out;
}

inline std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> out;
    std::size_t start = 0;

    while (start <= s.size()) {
        const std::size_t end = s.find('\n', start);
        if (end == std::string::npos) {
            if (start < s.size()) out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, end - start));
        start = end + 1;
    }

    return out;
}

inline std::optional<std::uint64_t> parse_u64(const std::string& s, int base = 10) noexcept {
    const std::string t = trim(s);
    if (t.empty()) return std::nullopt;
    char* end = nullptr;
    const unsigned long long v = std::strtoull(t.c_str(), &end, base);
    if (end == t.c_str()) return std::nullopt;
    return static_cast<std::uint64_t>(v);
}

inline std::optional<std::int64_t> parse_i64(const std::string& s) noexcept {
    const std::string t = trim(s);
    if (t.empty()) return std::nullopt;
    char* end = nullptr;
    const long long v = std::strtoll(t.c_str(), &end, 10);
    if (end == t.c_str()) return std::nullopt;
    return static_cast<std::int64_t>(v);
}

inline std::optional<double> parse_double(const std::string& s) noexcept {
    const std::string t = trim(s);
    if (t.empty()) return std::nullopt;
    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (end == t.c_str() || !std::isfinite(v)) return std::nullopt;
    return v;
}

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

inline Statistics summarize(std::vector<double> values) {
    Statistics s;
    if (values.empty()) return s;
    s.count = values.size();
    s.last  = values.back();
    double sum = 0.0;
    for (double v : values) sum += v;
    s.average = sum / static_cast<double>(values.size());
    std::sort(values.begin(), values.end());
    s.min = values.front();
    s.max = values.back();
    const std::size_t idx = static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(values.size()))) - 1;
    s.p95 = values[std::min(idx, values.size() - 1)];
    return s;
}

} // namespace detail

inline std::string format_bytes(std::uint64_t bytes) {
    static const char* units[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB" };
    double v = static_cast<double>(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 5) { v /= 1024.0; ++u; }
    return detail::fixed(v, u == 0 ? 0 : (v < 10.0 ? 2 : (v < 100.0 ? 1 : 0))) + " " + units[u];
}

inline std::string format_rate(double bytes_per_second) {
    if (!(bytes_per_second > 0.0)) return "0 B/s";
    return format_bytes(static_cast<std::uint64_t>(bytes_per_second + 0.5)) + "/s";
}

inline std::string format_bitrate(double bytes_per_second) {
    static const char* units[] = { "bit/s", "kbit/s", "Mbit/s", "Gbit/s", "Tbit/s" };
    double v = bytes_per_second > 0.0 ? bytes_per_second * 8.0 : 0.0;
    int u = 0;
    while (v >= 1000.0 && u < 4) { v /= 1000.0; ++u; }
    return detail::fixed(v, u == 0 ? 0 : (v < 10.0 ? 2 : (v < 100.0 ? 1 : 0))) + " " + units[u];
}

inline std::string format_optional(const std::optional<double>& v, int decimals, const char* unit) {
    if (!v) return "n/a";
    return detail::fixed(*v, decimals) + unit;
}

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_COMMON_HPP
