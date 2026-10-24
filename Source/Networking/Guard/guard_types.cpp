#include "fizmo_library.hpp"
#include "guard_types.hpp"
#include "../../Crypto/hash.hpp"
#include "../../Crypto/encoding.hpp"

#include <cstdio>
#include <ctime>

namespace fizmo {
namespace networking {
namespace guard {

std::string hash_secret(const std::string& secret) {
    const auto d = crypto::SHA256::hash(secret);
    return crypto::base64url_encode(d.data(), d.size());
}

std::string format_iso8601(WallTime t) {
    const auto since = t.time_since_epoch();
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(since);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(since - secs).count();
    if (ms < 0) ms += 1000;
    const std::time_t tt = static_cast<std::time_t>(secs.count());
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif // defined(_WIN32)
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms));
    return buf;
}

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);

    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }

    return out;
}

} // namespace guard
} // namespace networking
} // namespace fizmo
