#include "fizmo_library.hpp"
#include "value.hpp"
#include "../Collections/hash.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace fizmo {
namespace database {

namespace {

constexpr std::uint8_t kTagNull = 0x01;
constexpr std::uint8_t kTagBool = 0x02;
constexpr std::uint8_t kTagInt  = 0x03;
constexpr std::uint8_t kTagReal = 0x04;
constexpr std::uint8_t kTagText = 0x05;
constexpr std::uint8_t kTagBlob = 0x06;

int rank_of(ValueType t) noexcept {
    switch (t) {
        case ValueType::Null:    return 0;
        case ValueType::Boolean: return 1;
        case ValueType::Integer:
        case ValueType::Real:    return 2;
        case ValueType::Text:    return 3;
        default:                 return 4;
    }
}

int cmp_int_real(std::int64_t i, double r) noexcept {
    if (std::isnan(r)) return 1;
    if (r < -9223372036854775808.0) return 1;
    if (r >= 9223372036854775808.0) return -1;
    const double t = std::trunc(r);
    const std::int64_t ti = static_cast<std::int64_t>(t);
    if (i < ti) return -1;
    if (i > ti) return 1;
    const double frac = r - t;
    if (frac > 0) return -1;
    if (frac < 0) return 1;
    return 0;
}

int cmp_real(double a, double b) noexcept {
    const bool na = std::isnan(a), nb = std::isnan(b);
    if (na || nb) return na == nb ? 0 : (na ? -1 : 1);
    return a < b ? -1 : (a > b ? 1 : 0);
}

int cmp_bytes(const std::string& a, const std::string& b) noexcept {
    const int c = a.compare(b);
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

void put_u64_be(std::string& out, std::uint64_t v) {
    char buf[8];
    for (int i = 7; i >= 0; --i) { buf[i] = static_cast<char>(v & 0xff); v >>= 8; }
    out.append(buf, 8);
}

void put_escaped(std::string& out, const std::string& bytes) {
    for (char c : bytes) {
        out += c;
        if (c == '\0') out += '\xff';
    }
}

std::string format_real(double d) {
    if (std::isnan(d)) return "NaN";
    if (std::isinf(d)) return d > 0 ? "Infinity" : "-Infinity";
    char buf[40];
    if (d == std::trunc(d) && std::fabs(d) < 1e16) {
        std::snprintf(buf, sizeof(buf), "%.0f", d);
        return buf;
    }
    std::snprintf(buf, sizeof(buf), "%.17g", d);
    double back = 0;
    char shorter[40];

    for (int p = 1; p <= 17; ++p) {
        std::snprintf(shorter, sizeof(shorter), "%.*g", p, d);
        back = std::strtod(shorter, nullptr);
        if (back == d) return shorter;
    }

    return buf;
}

std::string json_escape(const std::string& s) {
    std::string out;

    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
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

} // namespace

const char* to_string(ValueType type) noexcept {
    switch (type) {
        case ValueType::Null:    return "NULL";
        case ValueType::Boolean: return "BOOLEAN";
        case ValueType::Integer: return "INTEGER";
        case ValueType::Real:    return "REAL";
        case ValueType::Text:    return "TEXT";
        default:                 return "BLOB";
    }
}

bool Value::as_bool() const {
    switch (type()) {
        case ValueType::Boolean: return get_bool();
        case ValueType::Integer: return get_int() != 0;
        case ValueType::Real:    return get_real() != 0.0;
        default: throw TypeError(std::string("cannot read ") + database::to_string(type()) + " as BOOLEAN");
    }
}

std::int64_t Value::as_int() const {
    switch (type()) {
        case ValueType::Integer: return get_int();
        case ValueType::Boolean: return get_bool() ? 1 : 0;
        case ValueType::Real: {
            const double d = get_real();
            if (!(d >= -9223372036854775808.0 && d < 9223372036854775808.0)) throw TypeError("REAL value out of INTEGER range");
            return static_cast<std::int64_t>(d);
        }
        default: throw TypeError(std::string("cannot read ") + database::to_string(type()) + " as INTEGER");
    }
}

double Value::as_real() const {
    switch (type()) {
        case ValueType::Real:    return get_real();
        case ValueType::Integer: return static_cast<double>(get_int());
        case ValueType::Boolean: return get_bool() ? 1.0 : 0.0;
        default: throw TypeError(std::string("cannot read ") + database::to_string(type()) + " as REAL");
    }
}

const std::string& Value::as_text() const {
    if (!is_text()) throw TypeError(std::string("cannot read ") + database::to_string(type()) + " as TEXT");
    return get_text();
}

const Blob& Value::as_blob() const {
    if (!is_blob()) throw TypeError(std::string("cannot read ") + database::to_string(type()) + " as BLOB");
    return get_blob();
}

double Value::numeric() const noexcept {
    switch (type()) {
        case ValueType::Integer: return static_cast<double>(get_int());
        case ValueType::Real:    return get_real();
        case ValueType::Boolean: return get_bool() ? 1.0 : 0.0;
        default:                 return std::numeric_limits<double>::quiet_NaN();
    }
}

std::string Value::to_string() const {
    switch (type()) {
        case ValueType::Null:    return "NULL";
        case ValueType::Boolean: return get_bool() ? "true" : "false";
        case ValueType::Integer: return std::to_string(get_int());
        case ValueType::Real:    return format_real(get_real());
        case ValueType::Text:    return get_text();
        default: {
            static const char* hex = "0123456789abcdef";
            std::string out = "x'";
            for (unsigned char c : get_blob().bytes) { out += hex[c >> 4]; out += hex[c & 15]; }
            return out + "'";
        }
    }
}

std::string Value::to_json() const {
    switch (type()) {
        case ValueType::Null:    return "null";
        case ValueType::Boolean: return get_bool() ? "true" : "false";
        case ValueType::Integer: return std::to_string(get_int());
        case ValueType::Real: {
            const double d = get_real();
            if (std::isnan(d) || std::isinf(d)) return "null";
            return format_real(d);
        }
        case ValueType::Text:    return "\"" + json_escape(get_text()) + "\"";
        default:                 return "\"" + to_string() + "\"";
    }
}

std::size_t Value::hash() const noexcept {
    switch (type()) {
        case ValueType::Null:    return 0x51ed270b27d1c7a5ULL;
        case ValueType::Boolean: return get_bool() ? 0x2f1d3b8e9a5c7d01ULL : 0x6c8e9cf570932bd5ULL;
        case ValueType::Integer: return static_cast<std::size_t>(collections::mix64(static_cast<std::uint64_t>(get_int())));
        case ValueType::Real: {
            const double d = get_real();
            if (std::isnan(d)) return 0x7ff8dead7ff8beefULL;
            if (d >= -9223372036854775808.0 && d < 9223372036854775808.0 && std::trunc(d) == d) {
                return static_cast<std::size_t>(collections::mix64(static_cast<std::uint64_t>(static_cast<std::int64_t>(d))));
            }
            std::uint64_t bits;
            std::memcpy(&bits, &d, sizeof(bits));
            return static_cast<std::size_t>(collections::mix64(bits));
        }
        case ValueType::Text:    return collections::hash_bytes(get_text().data(), get_text().size());
        default:                 return collections::hash_bytes(get_blob().bytes.data(), get_blob().bytes.size()) ^ 0x9e3779b97f4a7c15ULL;
    }
}

int compare(const Value& a, const Value& b) noexcept {
    const ValueType ta = a.type(), tb = b.type();

    if (ta == tb) {
        switch (ta) {
            case ValueType::Null:    return 0;
            case ValueType::Boolean: return a.get_bool() == b.get_bool() ? 0 : (a.get_bool() ? 1 : -1);
            case ValueType::Integer: return a.get_int() < b.get_int() ? -1 : (a.get_int() > b.get_int() ? 1 : 0);
            case ValueType::Real:    return cmp_real(a.get_real(), b.get_real());
            case ValueType::Text:    return cmp_bytes(a.get_text(), b.get_text());
            default:                 return cmp_bytes(a.get_blob().bytes, b.get_blob().bytes);
        }
    }

    const int ra = rank_of(ta), rb = rank_of(tb);
    if (ra != rb) return ra < rb ? -1 : 1;
    if (ta == ValueType::Integer) return cmp_int_real(a.get_int(), b.get_real());
    return -cmp_int_real(b.get_int(), a.get_real());
}

void encode_key_part(std::string& out, const Value& v) {
    switch (v.type()) {
        case ValueType::Null:
            out += static_cast<char>(kTagNull);
            break;
        case ValueType::Boolean:
            out += static_cast<char>(kTagBool);
            out += v.get_bool() ? '\x01' : '\x00';
            break;
        case ValueType::Integer:
            out += static_cast<char>(kTagInt);
            put_u64_be(out, static_cast<std::uint64_t>(v.get_int()) ^ 0x8000000000000000ULL);
            break;
        case ValueType::Real: {
            out += static_cast<char>(kTagReal);
            double d = v.get_real();
            if (d == 0.0) d = 0.0;
            std::uint64_t bits = 0;

            if (!std::isnan(d)) {
                std::memcpy(&bits, &d, sizeof(bits));
                bits = (bits & 0x8000000000000000ULL) ? ~bits : (bits | 0x8000000000000000ULL);
            }

            put_u64_be(out, bits);
            break;
        }
        case ValueType::Text:
            out += static_cast<char>(kTagText);
            put_escaped(out, v.get_text());
            out.append("\0\0", 2);
            break;
        default:
            out += static_cast<char>(kTagBlob);
            put_escaped(out, v.get_blob().bytes);
            out.append("\0\0", 2);
            break;
    }
}

std::string encode_key(const Value& v) {
    std::string out;
    encode_key_part(out, v);
    return out;
}

std::string encode_key(const std::vector<Value>& parts) {
    std::string out;
    for (const auto& p : parts) encode_key_part(out, p);
    return out;
}

std::string key_successor(std::string prefix) {
    while (!prefix.empty() && static_cast<unsigned char>(prefix.back()) == 0xff) prefix.pop_back();
    if (prefix.empty()) return prefix;
    prefix.back() = static_cast<char>(static_cast<unsigned char>(prefix.back()) + 1);
    return prefix;
}

std::string encode_text_prefix(const std::string& prefix) {
    std::string out(1, static_cast<char>(kTagText));
    put_escaped(out, prefix);
    return out;
}

} // namespace database
} // namespace fizmo
