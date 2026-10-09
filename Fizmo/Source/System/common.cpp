#include "fizmo_library.hpp"
#include "common.hpp"

namespace fizmo {
namespace system {
namespace detail {

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && static_cast<unsigned char>(s[a]) <= ' ') ++a;
    while (b > a && static_cast<unsigned char>(s[b - 1]) <= ' ') --b;
    return s.substr(a, b - a);
}

bool ends_with(const std::string& s, const char* suffix) noexcept {
    const std::size_t n = std::strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

std::vector<std::string> split_ws(const std::string& s) {
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

std::vector<std::string> split_lines(const std::string& s) {
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

std::optional<std::uint64_t> parse_u64(const std::string& s, int base) noexcept {
    const std::string t = trim(s);
    if (t.empty()) return std::nullopt;
    char* end = nullptr;
    const unsigned long long v = std::strtoull(t.c_str(), &end, base);
    if (end == t.c_str()) return std::nullopt;
    return static_cast<std::uint64_t>(v);
}

std::optional<std::int64_t> parse_i64(const std::string& s) noexcept {
    const std::string t = trim(s);
    if (t.empty()) return std::nullopt;
    char* end = nullptr;
    const long long v = std::strtoll(t.c_str(), &end, 10);
    if (end == t.c_str()) return std::nullopt;
    return static_cast<std::int64_t>(v);
}

std::optional<double> parse_double(const std::string& s) noexcept {
    const std::string t = trim(s);
    if (t.empty()) return std::nullopt;
    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (end == t.c_str() || !std::isfinite(v)) return std::nullopt;
    return v;
}

Statistics summarize(std::vector<double> values) {
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
} // namespace system
} // namespace fizmo

namespace fizmo {
namespace system {

std::string format_bytes(std::uint64_t bytes) {
    static const char* units[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB" };
    double v = static_cast<double>(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 5) { v /= 1024.0; ++u; }
    return detail::fixed(v, u == 0 ? 0 : (v < 10.0 ? 2 : (v < 100.0 ? 1 : 0))) + " " + units[u];
}

std::string format_rate(double bytes_per_second) {
    if (!(bytes_per_second > 0.0)) return "0 B/s";
    return format_bytes(static_cast<std::uint64_t>(bytes_per_second + 0.5)) + "/s";
}

std::string format_bitrate(double bytes_per_second) {
    static const char* units[] = { "bit/s", "kbit/s", "Mbit/s", "Gbit/s", "Tbit/s" };
    double v = bytes_per_second > 0.0 ? bytes_per_second * 8.0 : 0.0;
    int u = 0;
    while (v >= 1000.0 && u < 4) { v /= 1000.0; ++u; }
    return detail::fixed(v, u == 0 ? 0 : (v < 10.0 ? 2 : (v < 100.0 ? 1 : 0))) + " " + units[u];
}

} // namespace system
} // namespace fizmo
