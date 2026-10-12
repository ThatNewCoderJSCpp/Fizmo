#include "fizmo_library.hpp"
#include "string.hpp"
#include "../Random/random_std_int.hpp"

#include <cstdlib>
#include <sstream>

namespace fizmo {

namespace {

bool ascii_alnum(unsigned char c) noexcept { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool is_line_break(char32_t cp) noexcept { return cp == '\n' || cp == '\r' || cp == 0x0B || cp == 0x0C || cp == 0x85 || cp == 0x2028 || cp == 0x2029; }

template <typename Rng>
void shuffle_parts(collections::DynamicArray<StringView>& parts, Rng next) {
    for (std::size_t i = parts.size(); i > 1; --i) {
        const std::size_t j = next(i - 1);
        std::swap(parts[i - 1], parts[j]);
    }
}

} // namespace

String::String(String&& other) noexcept : String() {
    if (other.is_small()) {
        std::memcpy(m_local, other.m_local, other.m_size + 1);
        m_size = other.m_size;
    } else {
        m_ptr      = other.m_ptr;
        m_size     = other.m_size;
        m_capacity = other.m_capacity;
    }
    other.m_ptr      = other.m_local;
    other.m_size     = 0;
    other.m_local[0] = '\0';
}

String& String::operator=(String&& other) noexcept {
    if (this == &other) return *this;
    release();
    if (other.is_small()) {
        std::memcpy(m_local, other.m_local, other.m_size + 1);
        m_size = other.m_size;
    } else {
        m_ptr      = other.m_ptr;
        m_size     = other.m_size;
        m_capacity = other.m_capacity;
    }
    other.m_ptr      = other.m_local;
    other.m_size     = 0;
    other.m_local[0] = '\0';
    return *this;
}

String::String(std::size_t count, char ch) : String() {
    reserve(count);
    std::memset(m_ptr, static_cast<unsigned char>(ch), count);
    set_size(count);
}

void String::reallocate(std::size_t new_capacity) {
    if (new_capacity < m_size) new_capacity = m_size;
    if (new_capacity <= small_capacity) {
        if (is_small()) return;
        char*             old  = m_ptr;
        const std::size_t size = m_size;
        m_ptr = m_local;
        std::memcpy(m_local, old, size + 1);
        delete[] old;
        return;
    }
    char* p = new char[new_capacity + 1];
    std::memcpy(p, m_ptr, m_size + 1);
    if (!is_small()) delete[] m_ptr;
    m_ptr      = p;
    m_capacity = new_capacity;
}

void String::grow_to(std::size_t needed) {
    const std::size_t cap = capacity();
    if (needed <= cap) return;
    std::size_t next = cap + cap / 2;
    if (next < needed) next = needed;
    if (next < 31) next = 31;
    reallocate(next);
}

void String::shrink() { reallocate(m_size); }

void String::assign_raw(const char* str, std::size_t len) {
    if (!str) len = 0;
    if (len && aliases(str)) {
        String tmp(str, len);
        *this = std::move(tmp);
        return;
    }
    if (len > capacity()) {
        set_size(0);
        reallocate(len);
    }
    if (len) std::memcpy(m_ptr, str, len);
    set_size(len);
}

void String::append_raw(const char* str, std::size_t len) {
    if (!str || len == 0) return;
    if (m_size + len > capacity()) {
        if (aliases(str)) {
            const std::size_t off = static_cast<std::size_t>(str - m_ptr);
            grow_to(m_size + len);
            str = m_ptr + off;
        } else {
            grow_to(m_size + len);
        }
    }
    std::memmove(m_ptr + m_size, str, len);
    set_size(m_size + len);
}

void String::splice(std::size_t pos, std::size_t count, const char* str, std::size_t len) {
    if (pos > m_size) pos = m_size;
    if (count > m_size - pos) count = m_size - pos;
    if (len && aliases(str)) {
        const String tmp(str, len);
        splice(pos, count, tmp.m_ptr, tmp.m_size);
        return;
    }
    const std::size_t new_size = m_size - count + len;
    if (new_size > capacity()) grow_to(new_size);
    std::memmove(m_ptr + pos + len, m_ptr + pos + count, m_size - pos - count);
    if (len) std::memcpy(m_ptr + pos, str, len);
    set_size(new_size);
}

String String::from_code_point(char32_t cp) {
    String r;
    r.append_code_point(cp);
    return r;
}

String String::from_code_points(const char32_t* cps, std::size_t n) {
    String r;
    r.reserve(n);
    for (std::size_t i = 0; i < n; ++i) r.append_code_point(cps[i]);
    return r;
}

String String::from_utf16(const char16_t* str, std::size_t len) {
    String r;
    if (!str) return r;
    r.reserve(len);
    for (std::size_t i = 0; i < len; ++i) {
        char32_t cp = str[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < len && str[i + 1] >= 0xDC00 && str[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (static_cast<char32_t>(str[i + 1]) - 0xDC00);
            ++i;
        }
        r.append_code_point(cp);
    }
    return r;
}

String String::from_wide(const wchar_t* str, std::size_t len) {
    if (!str) return String();
    if (sizeof(wchar_t) == 2) {
        std::u16string tmp(len, u'\0');
        for (std::size_t i = 0; i < len; ++i) tmp[i] = static_cast<char16_t>(str[i]);
        return from_utf16(tmp.data(), tmp.size());
    }
    String r;
    r.reserve(len);
    for (std::size_t i = 0; i < len; ++i) r.append_code_point(static_cast<char32_t>(str[i]));
    return r;
}

std::u16string String::to_utf16() const {
    std::u16string out;
    out.reserve(m_size);
    std::size_t pos = 0;
    while (pos < m_size) {
        const char32_t cp = unicode::decode_utf8(m_ptr, m_size, pos);
        if (cp >= 0x10000) {
            out.push_back(static_cast<char16_t>(0xD800 + ((cp - 0x10000) >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + ((cp - 0x10000) & 0x3FF)));
        } else {
            out.push_back(static_cast<char16_t>(cp));
        }
    }
    return out;
}

std::u32string String::to_utf32() const {
    std::u32string out;
    out.reserve(m_size);
    std::size_t pos = 0;
    while (pos < m_size) out.push_back(unicode::decode_utf8(m_ptr, m_size, pos));
    return out;
}

std::wstring String::to_wide() const {
    std::wstring out;
    if (sizeof(wchar_t) == 2) {
        const std::u16string u = to_utf16();
        out.assign(u.begin(), u.end());
    } else {
        const std::u32string u = to_utf32();
        out.assign(u.begin(), u.end());
    }
    return out;
}

void String::resize(std::size_t new_size, char ch) {
    if (new_size > m_size) {
        reserve(new_size);
        std::memset(m_ptr + m_size, static_cast<unsigned char>(ch), new_size - m_size);
    }
    set_size(new_size);
}

bool String::glob_match(const char* s, std::size_t slen, const char* p, std::size_t plen) noexcept {
    std::size_t si = 0, pi = 0, star_pi = npos, star_si = 0;
    while (si < slen) {
        if (pi < plen && p[pi] == '*') {
            star_pi = pi++;
            star_si = si;
            continue;
        }
        if (pi < plen && p[pi] == '?') {
            std::size_t next = si;
            unicode::decode_utf8(s, slen, next);
            si = next;
            ++pi;
            continue;
        }
        if (pi < plen && p[pi] == s[si]) {
            ++si;
            ++pi;
            continue;
        }
        if (star_pi != npos) {
            pi = star_pi + 1;
            std::size_t next = star_si;
            unicode::decode_utf8(s, slen, next);
            star_si = next;
            si      = next;
            continue;
        }
        return false;
    }
    while (pi < plen && p[pi] == '*') ++pi;
    return pi == plen;
}

collections::DynamicArray<StringView> String::grapheme_list() const {
    collections::DynamicArray<StringView> parts;
    const StringView v = view();
    std::size_t pos = 0;
    while (pos < m_size) {
        const std::size_t next = unicode::next_grapheme_break(v, pos);
        parts.push_back(v.substring(pos, next - pos));
        pos = next;
    }
    return parts;
}

void String::rebuild_from(const collections::DynamicArray<StringView>& parts) {
    String r;
    r.reserve(m_size);
    for (std::size_t i = 0; i < parts.size(); ++i) r.append_raw(parts[i].data(), parts[i].size());
    *this = std::move(r);
}

std::size_t String::floor_grapheme(std::size_t pos) const noexcept {
    if (pos >= m_size) return m_size;
    if (unicode::is_grapheme_boundary(view(), pos)) return pos;
    return unicode::prev_grapheme_break(view(), pos);
}

std::size_t String::ceil_grapheme(std::size_t pos) const noexcept {
    if (pos >= m_size) return m_size;
    if (unicode::is_grapheme_boundary(view(), pos)) return pos;
    return unicode::next_grapheme_break(view(), unicode::prev_grapheme_break(view(), pos));
}

StringView String::grapheme_at(std::size_t index) const noexcept {
    const StringView v = view();
    std::size_t pos = 0;
    for (std::size_t i = 0; pos < m_size; ++i) {
        const std::size_t next = unicode::next_grapheme_break(v, pos);
        if (i == index) return v.substring(pos, next - pos);
        pos = next;
    }
    return StringView();
}

String String::grapheme_substring(std::size_t first, std::size_t count) const {
    const StringView v = view();
    std::size_t pos = 0;
    for (std::size_t i = 0; i < first && pos < m_size; ++i) pos = unicode::next_grapheme_break(v, pos);
    std::size_t end = pos;
    for (std::size_t i = 0; i < count && end < m_size; ++i) end = unicode::next_grapheme_break(v, end);
    return String(v.substring(pos, end - pos));
}

bool String::is_ascii() const noexcept {
    for (std::size_t i = 0; i < m_size; ++i) {
        if (static_cast<unsigned char>(m_ptr[i]) >= 0x80) return false;
    }
    return true;
}

void String::sanitize_utf8() {
    if (is_valid_utf8()) return;
    String r;
    r.reserve(m_size + 8);
    std::size_t pos = 0;
    while (pos < m_size) r.append_code_point(unicode::decode_utf8(m_ptr, m_size, pos));
    *this = std::move(r);
}

bool String::to_bool() const noexcept {
    const StringView v = view().trimmed();
    if (unicode::equals_ignore_case(v, "true") || v == "1" || unicode::equals_ignore_case(v, "yes") || unicode::equals_ignore_case(v, "on")) return true;
    if (unicode::equals_ignore_case(v, "false") || v == "0" || unicode::equals_ignore_case(v, "no") || unicode::equals_ignore_case(v, "off")) return false;
    return !empty();
}

bool String::is_integer() const noexcept {
    std::size_t i = 0;
    if (i < m_size && (m_ptr[i] == '-' || m_ptr[i] == '+')) ++i;
    if (i == m_size) return false;
    for (; i < m_size; ++i) {
        if (m_ptr[i] < '0' || m_ptr[i] > '9') return false;
    }
    return true;
}

bool String::is_float() const noexcept {
    std::size_t i = 0;
    if (i < m_size && (m_ptr[i] == '-' || m_ptr[i] == '+')) ++i;
    bool has_digit = false, has_dot = false, has_exp = false, exp_digit = true;
    for (; i < m_size; ++i) {
        const char c = m_ptr[i];
        if (c >= '0' && c <= '9') {
            has_digit = true;
            exp_digit = true;
        } else if (c == '.' && !has_dot && !has_exp) {
            has_dot = true;
        } else if ((c == 'e' || c == 'E') && has_digit && !has_exp) {
            has_exp   = true;
            exp_digit = false;
            if (i + 1 < m_size && (m_ptr[i + 1] == '+' || m_ptr[i + 1] == '-')) ++i;
        } else {
            return false;
        }
    }
    return has_digit && exp_digit;
}

String String::vformat(const char* fmt, va_list args) {
    va_list copy;
    va_copy(copy, args);
    const int needed = std::vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) return String();
    String result;
    result.reserve(static_cast<std::size_t>(needed));
    std::vsnprintf(result.m_ptr, static_cast<std::size_t>(needed) + 1, fmt, args);
    result.m_size = static_cast<std::size_t>(needed);
    return result;
}

String String::from_long_double(long double v, int prec) {
    std::ostringstream os;
    os.setf(std::ios::fixed, std::ios::floatfield);
    os.precision(prec < 0 ? 6 : prec);
    os << v;
    return String(os.str());
}

String String::format(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    String r = vformat(fmt, args);
    va_end(args);
    return r;
}

String String::from_stream(std::istream& is) {
    String result;
    char   buf[4096];
    while (is.read(buf, sizeof(buf)) || is.gcount() > 0) {
        result.append_raw(buf, static_cast<std::size_t>(is.gcount()));
        if (!is) break;
    }
    return result;
}

String String::to_hex() const {
    static const char hex_chars[] = "0123456789abcdef";
    String result;
    result.reserve(m_size * 2);
    for (std::size_t i = 0; i < m_size; ++i) {
        const unsigned char c = static_cast<unsigned char>(m_ptr[i]);
        result.push_back(hex_chars[c >> 4]);
        result.push_back(hex_chars[c & 0xF]);
    }
    return result;
}

String String::from_hex(StringView sv) {
    String result;
    if (sv.size() % 2 != 0) return result;
    result.reserve(sv.size() / 2);
    for (std::size_t i = 0; i < sv.size(); i += 2) {
        const int hi = hex_value(sv[i]);
        const int lo = hex_value(sv[i + 1]);
        if (hi < 0 || lo < 0) return String();
        result.push_back(static_cast<char>((hi << 4) | lo));
    }
    return result;
}

String String::to_base64() const {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String result;
    result.reserve(((m_size + 2) / 3) * 4);
    for (std::size_t i = 0; i < m_size; i += 3) {
        const unsigned char a = static_cast<unsigned char>(m_ptr[i]);
        const unsigned char b = (i + 1 < m_size) ? static_cast<unsigned char>(m_ptr[i + 1]) : 0;
        const unsigned char c = (i + 2 < m_size) ? static_cast<unsigned char>(m_ptr[i + 2]) : 0;
        result.push_back(b64[a >> 2]);
        result.push_back(b64[((a & 3) << 4) | (b >> 4)]);
        result.push_back((i + 1 < m_size) ? b64[((b & 0xF) << 2) | (c >> 6)] : '=');
        result.push_back((i + 2 < m_size) ? b64[c & 0x3F] : '=');
    }
    return result;
}

String String::from_base64(StringView sv) {
    auto bv = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    };
    String        result;
    std::uint32_t acc  = 0;
    int           bits = 0;
    result.reserve(sv.size() * 3 / 4);
    for (std::size_t i = 0; i < sv.size(); ++i) {
        const char c = sv[i];
        if (c == '=') break;
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        const int v = bv(c);
        if (v < 0) return String();
        acc = (acc << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<char>((acc >> bits) & 0xFF));
        }
    }
    return result;
}

String String::escape(StringView chars) const {
    String result;
    result.reserve(m_size + m_size / 8);
    for (std::size_t i = 0; i < m_size; ++i) {
        if (m_ptr[i] == '\\' || chars.contains(m_ptr[i])) result.push_back('\\');
        result.push_back(m_ptr[i]);
    }
    return result;
}

String String::unescape() const {
    String result;
    result.reserve(m_size);
    for (std::size_t i = 0; i < m_size; ++i) {
        if (m_ptr[i] != '\\' || i + 1 >= m_size) {
            result.push_back(m_ptr[i]);
            continue;
        }
        const char c = m_ptr[++i];
        switch (c) {
            case 'n': result.push_back('\n'); break;
            case 't': result.push_back('\t'); break;
            case 'r': result.push_back('\r'); break;
            case '0': result.push_back('\0'); break;
            case 'a': result.push_back('\a'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'v': result.push_back('\v'); break;
            case 'e': result.push_back('\x1B'); break;
            case 'x':
            case 'u':
            case 'U': {
                const std::size_t digits = c == 'x' ? 2 : (c == 'u' ? 4 : 8);
                char32_t          value  = 0;
                std::size_t       k      = 0;
                while (k < digits && i + 1 + k < m_size && hex_value(m_ptr[i + 1 + k]) >= 0) {
                    value = (value << 4) | static_cast<char32_t>(hex_value(m_ptr[i + 1 + k]));
                    ++k;
                }
                if (k != digits) {
                    result.push_back(c);
                    break;
                }
                i += k;
                if (c == 'x') result.push_back(static_cast<char>(value));
                else          result.append_code_point(value);
                break;
            }
            default: result.push_back(c); break;
        }
    }
    return result;
}

String String::url_encode() const {
    static const char hex[] = "0123456789ABCDEF";
    String result;
    result.reserve(m_size + m_size / 2);
    for (std::size_t i = 0; i < m_size; ++i) {
        const unsigned char c = static_cast<unsigned char>(m_ptr[i]);
        if (ascii_alnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result.push_back(static_cast<char>(c));
        } else {
            result.push_back('%');
            result.push_back(hex[c >> 4]);
            result.push_back(hex[c & 0xF]);
        }
    }
    return result;
}

String String::url_decode() const {
    String result;
    result.reserve(m_size);
    for (std::size_t i = 0; i < m_size; ++i) {
        if (m_ptr[i] == '%' && i + 2 < m_size) {
            const int hi = hex_value(m_ptr[i + 1]);
            const int lo = hex_value(m_ptr[i + 2]);
            if (hi >= 0 && lo >= 0) {
                result.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        } else if (m_ptr[i] == '+') {
            result.push_back(' ');
            continue;
        }
        result.push_back(m_ptr[i]);
    }
    return result;
}

String& String::append(std::size_t n, char ch) {
    grow_to(m_size + n);
    std::memset(m_ptr + m_size, static_cast<unsigned char>(ch), n);
    set_size(m_size + n);
    return *this;
}

String& String::append_code_point(char32_t cp) {
    char buf[4];
    append_raw(buf, unicode::encode_utf8(cp, buf));
    return *this;
}

String& String::append_code_point(std::size_t n, char32_t cp) {
    char              buf[4];
    const std::size_t len = unicode::encode_utf8(cp, buf);
    grow_to(m_size + n * len);
    for (std::size_t i = 0; i < n; ++i) append_raw(buf, len);
    return *this;
}

String& String::insert(std::size_t idx, const char* str, std::size_t len) {
    if (!str || len == 0) return *this;
    splice(idx, 0, str, len);
    return *this;
}

String& String::erase(std::size_t pos, std::size_t count) {
    if (pos >= m_size) return *this;
    splice(pos, count, nullptr, 0);
    return *this;
}

String& String::replace(std::size_t pos, std::size_t count, StringView with) {
    splice(pos, count, with.data(), with.size());
    return *this;
}

collections::DynamicArray<std::size_t> String::find_all(StringView sv) const {
    collections::DynamicArray<std::size_t> result;
    if (sv.empty() || sv.size() > m_size) return result;
    std::size_t pos = 0;
    while ((pos = find(sv, pos)) != npos) {
        result.push_back(pos);
        pos += sv.size();
    }
    return result;
}

std::size_t String::find_nth(char ch, std::size_t n) const noexcept {
    if (n == 0) return npos;
    std::size_t count = 0;
    for (std::size_t i = 0; i < m_size; ++i) {
        if (m_ptr[i] == ch && ++count == n) return i;
    }
    return npos;
}

std::size_t String::find_nth(StringView sv, std::size_t n) const noexcept {
    if (sv.empty() || n == 0) return npos;
    std::size_t count = 0, pos = 0;
    while ((pos = find(sv, pos)) != npos) {
        if (++count == n) return pos;
        pos += sv.size();
    }
    return npos;
}

String String::find_between(StringView open, StringView close) const {
    std::size_t s = find(open);
    if (s == npos) return String();
    s += open.size();
    const std::size_t e = find(close, s);
    if (e == npos) return String();
    return substring(s, e - s);
}

std::size_t String::count_occurrences(StringView sv) const noexcept {
    if (sv.empty() || sv.size() > m_size) return 0;
    std::size_t n = 0, pos = 0;
    while ((pos = find(sv, pos)) != npos) {
        ++n;
        pos += sv.size();
    }
    return n;
}

bool String::is_alpha() const noexcept {
    if (empty()) return false;
    std::size_t pos = 0;
    while (pos < m_size) {
        const char32_t cp = unicode::decode_utf8(m_ptr, m_size, pos);
        if (!unicode::is_alphabetic(cp) && !unicode::is_mark(cp)) return false;
    }
    return true;
}

bool String::is_digit() const noexcept {
    if (empty()) return false;
    std::size_t pos = 0;
    while (pos < m_size) {
        if (!unicode::is_digit(unicode::decode_utf8(m_ptr, m_size, pos))) return false;
    }
    return true;
}

bool String::is_alnum() const noexcept {
    if (empty()) return false;
    std::size_t pos = 0;
    while (pos < m_size) {
        const char32_t cp = unicode::decode_utf8(m_ptr, m_size, pos);
        if (!unicode::is_alphanumeric(cp) && !unicode::is_mark(cp)) return false;
    }
    return true;
}

bool String::is_whitespace_only() const noexcept {
    if (empty()) return false;
    std::size_t pos = 0;
    while (pos < m_size) {
        if (!unicode::is_whitespace(unicode::decode_utf8(m_ptr, m_size, pos))) return false;
    }
    return true;
}

bool String::is_hex() const noexcept {
    if (empty()) return false;
    for (std::size_t i = 0; i < m_size; ++i) {
        if (hex_value(m_ptr[i]) < 0) return false;
    }
    return true;
}

bool String::is_upper() const noexcept {
    bool        cased = false;
    std::size_t pos   = 0;
    while (pos < m_size) {
        const char32_t cp = unicode::decode_utf8(m_ptr, m_size, pos);
        if (unicode::is_lowercase(cp) || unicode::is_titlecase(cp)) return false;
        if (unicode::is_cased(cp)) cased = true;
    }
    return cased;
}

bool String::is_lower() const noexcept {
    bool        cased = false;
    std::size_t pos   = 0;
    while (pos < m_size) {
        const char32_t cp = unicode::decode_utf8(m_ptr, m_size, pos);
        if (unicode::is_uppercase(cp) || unicode::is_titlecase(cp)) return false;
        if (unicode::is_cased(cp)) cased = true;
    }
    return cased;
}

bool String::is_palindrome() const {
    const collections::DynamicArray<StringView> parts = grapheme_list();
    if (parts.size() <= 1) return true;
    std::size_t lo = 0, hi = parts.size() - 1;
    while (lo < hi) {
        if (parts[lo++] != parts[hi--]) return false;
    }
    return true;
}

bool String::is_sorted() const noexcept {
    return is_sorted([](char32_t a, char32_t b) { return a < b; });
}

std::size_t String::levenshtein_distance(StringView other) const {
    const collections::DynamicArray<StringView> a = grapheme_list();
    collections::DynamicArray<StringView>       b;
    std::size_t pos = 0;
    while (pos < other.size()) {
        const std::size_t next = unicode::next_grapheme_break(other, pos);
        b.push_back(other.substring(pos, next - pos));
        pos = next;
    }
    const std::size_t m = a.size(), n = b.size();
    collections::DynamicArray<std::size_t> row;
    row.reserve(n + 1);
    for (std::size_t j = 0; j <= n; ++j) row.push_back(j);
    for (std::size_t i = 1; i <= m; ++i) {
        std::size_t prev = row[0];
        row[0]           = i;
        for (std::size_t j = 1; j <= n; ++j) {
            const std::size_t tmp = row[j];
            if (a[i - 1] == b[j - 1]) {
                row[j] = prev;
            } else {
                const std::size_t x = prev, y = row[j], z = row[j - 1];
                row[j] = 1 + (x < y ? (x < z ? x : z) : (y < z ? y : z));
            }
            prev = tmp;
        }
    }
    return row[n];
}

StringView String::common_prefix(StringView other) const noexcept {
    const std::size_t n = m_size < other.size() ? m_size : other.size();
    std::size_t i = 0;
    while (i < n && m_ptr[i] == other[i]) ++i;
    while (i > 0 && !(unicode::is_grapheme_boundary(view(), i) && unicode::is_grapheme_boundary(other, i))) i = unicode::prev_code_point(view(), i);
    return StringView(m_ptr, i);
}

StringView String::common_suffix(StringView other) const noexcept {
    const std::size_t n = m_size < other.size() ? m_size : other.size();
    std::size_t i = 0;
    while (i < n && m_ptr[m_size - 1 - i] == other[other.size() - 1 - i]) ++i;
    while (i > 0 && !(unicode::is_grapheme_boundary(view(), m_size - i) && unicode::is_grapheme_boundary(other, other.size() - i))) {
        i = m_size - unicode::next_code_point(view(), m_size - i);
    }
    return StringView(m_ptr + m_size - i, i);
}

std::size_t String::word_count() const noexcept {
    std::size_t n = 0;
    for (StringView w : words(true)) {
        (void)w;
        ++n;
    }
    return n;
}

char String::min() const noexcept {
    return min([](char a, char b) { return static_cast<unsigned char>(a) < static_cast<unsigned char>(b); });
}

char String::max() const noexcept {
    return max([](char a, char b) { return static_cast<unsigned char>(a) < static_cast<unsigned char>(b); });
}

std::pair<char, char> String::min_max() const noexcept { return { min(), max() }; }

void String::to_upper(std::size_t start, std::size_t end_idx) {
    if (end_idx > m_size) end_idx = m_size;
    if (start >= end_idx) return;
    while (!unicode::is_code_point_boundary(view(), start)) --start;
    while (!unicode::is_code_point_boundary(view(), end_idx)) ++end_idx;
    String mid;
    unicode::append_upper(view().substring(start, end_idx - start), mid);
    replace(start, end_idx - start, mid);
}

void String::to_lower(std::size_t start, std::size_t end_idx) {
    if (end_idx > m_size) end_idx = m_size;
    if (start >= end_idx) return;
    while (!unicode::is_code_point_boundary(view(), start)) --start;
    while (!unicode::is_code_point_boundary(view(), end_idx)) ++end_idx;
    String mid;
    unicode::append_lower(view(), start, end_idx, mid);
    replace(start, end_idx - start, mid);
}

void String::capitalize() {
    if (empty()) return;
    std::size_t    pos   = 0;
    const char32_t first = unicode::decode_utf8(m_ptr, m_size, pos);
    String r;
    r.reserve(m_size);
    char32_t m[3];
    const std::size_t n = unicode::full_to_title(first, m);
    for (std::size_t i = 0; i < n; ++i) r.append_code_point(m[i]);
    unicode::append_lower(view(), pos, m_size, r);
    *this = std::move(r);
}

String String::snake_to_camel() const {
    String result;
    result.reserve(m_size);
    bool        cap_next = false;
    std::size_t pos      = 0;
    while (pos < m_size) {
        const char32_t cp = unicode::decode_utf8(m_ptr, m_size, pos);
        if (cp == '_') {
            cap_next = true;
        } else if (cap_next) {
            char32_t m[3];
            const std::size_t n = unicode::full_to_upper(cp, m);
            for (std::size_t i = 0; i < n; ++i) result.append_code_point(m[i]);
            cap_next = false;
        } else {
            result.append_code_point(cp);
        }
    }
    return result;
}

String String::camel_to_snake() const {
    String result;
    result.reserve(m_size + m_size / 4);
    std::size_t pos = 0;
    while (pos < m_size) {
        const std::size_t start = pos;
        const char32_t    cp    = unicode::decode_utf8(m_ptr, m_size, pos);
        if ((unicode::is_uppercase(cp) || unicode::is_titlecase(cp)) && start > 0) result.push_back('_');
        char32_t m[3];
        const std::size_t n = unicode::full_to_lower(cp, m);
        for (std::size_t i = 0; i < n; ++i) result.append_code_point(m[i]);
    }
    return result;
}

void String::ltrim() {
    std::size_t pos = 0;
    while (pos < m_size) {
        std::size_t next = pos;
        if (!unicode::is_whitespace(unicode::decode_utf8(m_ptr, m_size, next))) break;
        pos = next;
    }
    if (pos) erase(0, pos);
}

void String::rtrim() {
    std::size_t end = m_size;
    while (end > 0) {
        const std::size_t prev = unicode::prev_code_point(view(), end);
        if (!unicode::is_whitespace(unicode::code_point_at(view(), prev))) break;
        end = prev;
    }
    set_size(end);
}

void String::trim(StringView chars) {
    std::size_t lo = view().find_first_not_of(chars);
    if (lo == npos) {
        clear();
        return;
    }
    const std::size_t hi = view().find_last_not_of(chars);
    set_size(hi + 1);
    if (lo) erase(0, lo);
}

void String::normalize_whitespace() {
    String      result;
    bool        pending = false;
    std::size_t pos     = 0;
    result.reserve(m_size);
    while (pos < m_size) {
        const std::size_t start = pos;
        const char32_t    cp    = unicode::decode_utf8(m_ptr, m_size, pos);
        if (unicode::is_whitespace(cp)) {
            pending = !result.empty();
            continue;
        }
        if (pending) result.push_back(' ');
        pending = false;
        result.append_raw(m_ptr + start, pos - start);
    }
    *this = std::move(result);
}

String String::indent(std::size_t n, char pad) const {
    if (empty() || n == 0) return *this;
    const String prefix(n, pad);
    const collections::DynamicArray<String> ls = lines(true);
    String result;
    result.reserve(m_size + ls.size() * n);
    for (std::size_t i = 0; i < ls.size(); ++i) {
        if (i > 0) result.push_back('\n');
        result += prefix;
        result += ls[i];
    }
    return result;
}

String String::wrap(std::size_t width) const {
    if (width == 0 || empty()) return *this;
    const collections::DynamicArray<String> words_ = split_whitespace();
    String      result;
    std::size_t col = 0;
    for (std::size_t i = 0; i < words_.size(); ++i) {
        const std::size_t w = words_[i].display_width();
        if (col > 0 && col + 1 + w > width) {
            result.push_back('\n');
            col = 0;
        } else if (col > 0) {
            result.push_back(' ');
            ++col;
        }
        result += words_[i];
        col += w;
    }
    return result;
}

String String::truncate_to(std::size_t max_width, StringView ellipsis) const {
    if (display_width() <= max_width) return *this;
    const std::size_t ew = unicode::display_width(ellipsis);
    const StringView  src = ew >= max_width ? ellipsis : view();
    const std::size_t budget = ew >= max_width ? max_width : max_width - ew;
    std::size_t pos = 0, used = 0;
    while (pos < src.size()) {
        const std::size_t next = unicode::next_grapheme_break(src, pos);
        const std::size_t w    = unicode::grapheme_width(src.substring(pos, next - pos));
        if (used + w > budget) break;
        used += w;
        pos = next;
    }
    String r(src.substring(0, pos));
    if (ew < max_width) r += ellipsis;
    return r;
}

void String::remove(std::size_t index) {
    if (index >= m_size) return;
    const std::size_t start = floor_grapheme(index);
    erase(start, unicode::next_grapheme_break(view(), start) - start);
}

void String::remove_front() {
    if (!empty()) erase(0, unicode::next_grapheme_break(view(), 0));
}

void String::remove_back() {
    if (!empty()) set_size(unicode::prev_grapheme_break(view(), m_size));
}

char32_t String::pop_front_char() {
    if (empty()) return 0;
    std::size_t    pos = 0;
    const char32_t cp  = unicode::decode_utf8(m_ptr, m_size, pos);
    erase(0, pos);
    return cp;
}

char32_t String::pop_back_char() {
    if (empty()) return 0;
    const std::size_t start = unicode::prev_code_point(view(), m_size);
    const char32_t    cp    = unicode::code_point_at(view(), start);
    set_size(start);
    return cp;
}

String String::pop_front_grapheme() {
    if (empty()) return String();
    const std::size_t end = unicode::next_grapheme_break(view(), 0);
    String r(m_ptr, end);
    erase(0, end);
    return r;
}

String String::pop_back_grapheme() {
    if (empty()) return String();
    const std::size_t start = unicode::prev_grapheme_break(view(), m_size);
    String r(m_ptr + start, m_size - start);
    set_size(start);
    return r;
}

std::size_t String::replace_str(StringView old_sv, StringView new_sv, std::size_t max_count, Direction dir) {
    if (old_sv.empty() || old_sv.size() > m_size) return 0;
    collections::DynamicArray<std::size_t> hits = find_all(old_sv);
    if (hits.empty()) return 0;
    std::size_t first = 0, last = hits.size();
    if (max_count > 0 && max_count < hits.size()) {
        if (dir == Direction::Front) last = max_count;
        else if (dir == Direction::Back) first = hits.size() - max_count;
    }
    const String repl(new_sv);
    String result;
    result.reserve(m_size + (last - first) * repl.size());
    std::size_t src = 0;
    for (std::size_t h = first; h < last; ++h) {
        result.append_raw(m_ptr + src, hits[h] - src);
        result.append_raw(repl.m_ptr, repl.m_size);
        src = hits[h] + old_sv.size();
    }
    result.append_raw(m_ptr + src, m_size - src);
    *this = std::move(result);
    return last - first;
}

void String::reverse() {
    collections::DynamicArray<StringView> parts = grapheme_list();
    std::reverse(parts.data(), parts.data() + parts.size());
    rebuild_from(parts);
}

void String::reverse(std::size_t start, std::size_t end_idx) {
    if (end_idx > m_size) end_idx = m_size;
    start   = floor_grapheme(start);
    end_idx = ceil_grapheme(end_idx);
    if (start >= end_idx) return;
    String mid(view().substring(start, end_idx - start));
    mid.reverse();
    replace(start, end_idx - start, mid);
}

void String::sort() { sort_code_points(0, m_size, [](char32_t a, char32_t b) { return a < b; }, false); }

void String::sort(std::size_t start, std::size_t end_idx) { sort_code_points(start, end_idx, [](char32_t a, char32_t b) { return a < b; }, false); }

void String::unique() {
    unique([](StringView a, StringView b) { return a == b; });
}

void String::deduplicate() { *this = distinct(); }

String String::distinct() const {
    const collections::DynamicArray<StringView> parts = grapheme_list();
    collections::DynamicArray<StringView>       kept;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        bool seen = false;
        for (std::size_t j = 0; j < kept.size() && !seen; ++j) seen = kept[j] == parts[i];
        if (!seen) kept.push_back(parts[i]);
    }
    String r;
    r.reserve(m_size);
    for (std::size_t i = 0; i < kept.size(); ++i) r.append_raw(kept[i].data(), kept[i].size());
    return r;
}

void String::rotate_left(std::size_t n) {
    collections::DynamicArray<StringView> parts = grapheme_list();
    if (parts.size() <= 1) return;
    n %= parts.size();
    std::rotate(parts.data(), parts.data() + n, parts.data() + parts.size());
    rebuild_from(parts);
}

void String::rotate_right(std::size_t n) {
    collections::DynamicArray<StringView> parts = grapheme_list();
    if (parts.size() <= 1) return;
    n %= parts.size();
    std::rotate(parts.data(), parts.data() + (parts.size() - n), parts.data() + parts.size());
    rebuild_from(parts);
}

void String::shuffle() { shuffle(0, m_size); }

void String::shuffle(std::size_t start, std::size_t end_idx) {
    if (end_idx > m_size) end_idx = m_size;
    start   = floor_grapheme(start);
    end_idx = ceil_grapheme(end_idx);
    if (start >= end_idx) return;
    String mid(view().substring(start, end_idx - start));
    collections::DynamicArray<StringView> parts = mid.grapheme_list();
    shuffle_parts(parts, [](std::size_t hi) { return random_int<std::size_t>(0, hi); });
    String out;
    for (std::size_t i = 0; i < parts.size(); ++i) out += parts[i];
    replace(start, end_idx - start, out);
}

void String::unsecure_shuffle() { unsecure_shuffle(0, m_size); }

void String::unsecure_shuffle(std::size_t start, std::size_t end_idx) {
    if (end_idx > m_size) end_idx = m_size;
    start   = floor_grapheme(start);
    end_idx = ceil_grapheme(end_idx);
    if (start >= end_idx) return;
    String mid(view().substring(start, end_idx - start));
    collections::DynamicArray<StringView> parts = mid.grapheme_list();
    shuffle_parts(parts, [](std::size_t hi) { return unsecure_random_int<std::size_t>(0, hi); });
    String out;
    for (std::size_t i = 0; i < parts.size(); ++i) out += parts[i];
    replace(start, end_idx - start, out);
}

collections::DynamicArray<String> String::split(char delim, bool keep_empty) const {
    collections::DynamicArray<String> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= m_size; ++i) {
        if (i == m_size || m_ptr[i] == delim) {
            if (keep_empty || i > start) parts.push_back(substring(start, i - start));
            start = i + 1;
        }
    }
    return parts;
}

collections::DynamicArray<String> String::split(StringView delim, bool keep_empty) const {
    collections::DynamicArray<String> parts;
    if (delim.empty()) {
        parts.push_back(*this);
        return parts;
    }
    std::size_t start = 0;
    for (;;) {
        const std::size_t pos = find(delim, start);
        if (pos == npos) {
            if (keep_empty || start < m_size) parts.push_back(substring(start));
            break;
        }
        if (keep_empty || pos > start) parts.push_back(substring(start, pos - start));
        start = pos + delim.size();
    }
    return parts;
}

collections::DynamicArray<String> String::split_whitespace() const {
    collections::DynamicArray<String> parts;
    std::size_t pos = 0;
    while (pos < m_size) {
        std::size_t start = pos;
        while (pos < m_size) {
            std::size_t next = pos;
            if (!unicode::is_whitespace(unicode::decode_utf8(m_ptr, m_size, next))) break;
            pos = next;
        }
        start = pos;
        while (pos < m_size) {
            std::size_t next = pos;
            if (unicode::is_whitespace(unicode::decode_utf8(m_ptr, m_size, next))) break;
            pos = next;
        }
        if (pos > start) parts.push_back(substring(start, pos - start));
    }
    return parts;
}

collections::DynamicArray<String> String::split_n(StringView delim, std::size_t max_parts) const {
    collections::DynamicArray<String> parts;
    if (delim.empty() || max_parts == 0) {
        parts.push_back(*this);
        return parts;
    }
    std::size_t start = 0;
    while (parts.size() + 1 < max_parts) {
        const std::size_t pos = find(delim, start);
        if (pos == npos) break;
        parts.push_back(substring(start, pos - start));
        start = pos + delim.size();
    }
    parts.push_back(substring(start));
    return parts;
}

collections::DynamicArray<String> String::rsplit(StringView delim, std::size_t n) const {
    collections::DynamicArray<String> parts;
    if (delim.empty() || n == 0) {
        parts.push_back(*this);
        return parts;
    }
    const collections::DynamicArray<std::size_t> all = find_all(delim);
    if (all.empty()) {
        parts.push_back(*this);
        return parts;
    }
    const std::size_t used  = all.size() < n ? all.size() : n;
    const std::size_t first = all.size() - used;
    std::size_t       prev  = 0;
    for (std::size_t i = first; i < all.size(); ++i) {
        parts.push_back(substring(prev, all[i] - prev));
        prev = all[i] + delim.size();
    }
    parts.push_back(substring(prev));
    return parts;
}

collections::DynamicArray<String> String::lines(bool keep_empty) const {
    collections::DynamicArray<String> result;
    std::size_t pos = 0, start = 0;
    while (pos < m_size) {
        const std::size_t here = pos;
        const char32_t    cp   = unicode::decode_utf8(m_ptr, m_size, pos);
        if (!is_line_break(cp)) continue;
        if (cp == '\r' && pos < m_size && m_ptr[pos] == '\n') ++pos;
        if (keep_empty || here > start) result.push_back(substring(start, here - start));
        start = pos;
    }
    if (keep_empty || start < m_size) result.push_back(substring(start));
    return result;
}

collections::DynamicArray<String> String::chunks(std::size_t n) const {
    collections::DynamicArray<String> result;
    if (n == 0) return result;
    const StringView v = view();
    std::size_t pos = 0;
    while (pos < m_size) {
        std::size_t end = pos;
        for (std::size_t k = 0; k < n && end < m_size; ++k) end = unicode::next_grapheme_break(v, end);
        result.push_back(substring(pos, end - pos));
        pos = end;
    }
    return result;
}

String String::join(const collections::DynamicArray<String>& parts, StringView sep) {
    String result;
    if (parts.empty()) return result;
    std::size_t total = sep.size() * (parts.size() - 1);
    for (std::size_t i = 0; i < parts.size(); ++i) total += parts[i].size();
    result.reserve(total);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) result += sep;
        result += parts[i];
    }
    return result;
}

String String::join(const collections::DynamicArray<StringView>& parts, StringView sep) {
    String result;
    if (parts.empty()) return result;
    std::size_t total = sep.size() * (parts.size() - 1);
    for (std::size_t i = 0; i < parts.size(); ++i) total += parts[i].size();
    result.reserve(total);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) result += sep;
        result += parts[i];
    }
    return result;
}

String String::repeated(std::size_t n) const {
    String result;
    if (n == 0 || empty()) return result;
    result.reserve(m_size * n);
    for (std::size_t i = 0; i < n; ++i) result.append_raw(m_ptr, m_size);
    return result;
}

String String::padded_left(std::size_t total_width, char32_t pad) const {
    const std::size_t w = display_width();
    if (w >= total_width) return *this;
    const std::size_t pw    = static_cast<std::size_t>(unicode::code_point_width(pad) > 0 ? unicode::code_point_width(pad) : 1);
    const std::size_t room  = total_width - w;
    String r;
    r.append(room % pw, ' ');
    r.append_code_point(room / pw, pad);
    r += *this;
    return r;
}

String String::padded_right(std::size_t total_width, char32_t pad) const {
    const std::size_t w = display_width();
    if (w >= total_width) return *this;
    const std::size_t pw   = static_cast<std::size_t>(unicode::code_point_width(pad) > 0 ? unicode::code_point_width(pad) : 1);
    const std::size_t room = total_width - w;
    String r(*this);
    r.append_code_point(room / pw, pad);
    r.append(room % pw, ' ');
    return r;
}

String String::padded_center(std::size_t total_width, char32_t pad) const {
    const std::size_t w = display_width();
    if (w >= total_width) return *this;
    const std::size_t pw    = static_cast<std::size_t>(unicode::code_point_width(pad) > 0 ? unicode::code_point_width(pad) : 1);
    const std::size_t room  = total_width - w;
    const std::size_t left  = room / 2;
    const std::size_t right = room - left;
    String r;
    r.append(left % pw, ' ');
    r.append_code_point(left / pw, pad);
    r += *this;
    r.append_code_point(right / pw, pad);
    r.append(right % pw, ' ');
    return r;
}

String String::zfill(std::size_t total_width) const {
    const std::size_t w = display_width();
    if (w >= total_width) return *this;
    String r;
    std::size_t sign = 0;
    if (!empty() && (m_ptr[0] == '-' || m_ptr[0] == '+')) {
        r.push_back(m_ptr[0]);
        sign = 1;
    }
    r.append(total_width - w, '0');
    r.append_raw(m_ptr + sign, m_size - sign);
    return r;
}

void StringWhereProxy::erase_indices() {
    String&           s     = m_str;
    const std::size_t n     = m_indices.size();
    if (n == 0) return;
    std::size_t write = m_indices[0];
    std::size_t k     = 0;
    for (std::size_t read = m_indices[0]; read < s.m_size; ++read) {
        if (k < n && m_indices[k] == read) {
            ++k;
            continue;
        }
        s.m_ptr[write++] = s.m_ptr[read];
    }
    s.set_size(write);
}

StringWhereProxy& StringWhereProxy::last(std::size_t n) {
    if (n < m_indices.size()) {
        const std::size_t start = m_indices.size() - n;
        for (std::size_t i = 0; i < n; ++i) m_indices[i] = m_indices[start + i];
        m_indices.resize(n);
    }
    return *this;
}

StringWhereProxy& StringWhereProxy::range(std::size_t start, std::size_t end_idx) {
    std::size_t write = 0;
    for (std::size_t i = 0; i < m_indices.size(); ++i) {
        if (m_indices[i] >= start && m_indices[i] < end_idx) m_indices[write++] = m_indices[i];
    }
    m_indices.resize(write);
    return *this;
}

StringWhereProxy& StringWhereProxy::skip(std::size_t n) {
    if (n >= m_indices.size()) {
        m_indices.clear();
        return *this;
    }
    const std::size_t count = m_indices.size() - n;
    for (std::size_t i = 0; i < count; ++i) m_indices[i] = m_indices[n + i];
    m_indices.resize(count);
    return *this;
}

StringWhereProxy& StringWhereProxy::at(std::size_t n) {
    if (n < m_indices.size()) {
        m_indices[0] = m_indices[n];
        m_indices.resize(1);
    } else {
        m_indices.clear();
    }
    return *this;
}

StringWhereProxy& StringWhereProxy::slice(std::size_t start, std::size_t end_idx) {
    if (end_idx > m_indices.size()) end_idx = m_indices.size();
    if (start >= end_idx) {
        m_indices.clear();
        return *this;
    }
    const std::size_t count = end_idx - start;
    for (std::size_t i = 0; i < count; ++i) m_indices[i] = m_indices[start + i];
    m_indices.resize(count);
    return *this;
}

StringWhereProxy& StringWhereProxy::even() {
    std::size_t write = 0;
    for (std::size_t i = 0; i < m_indices.size(); i += 2) m_indices[write++] = m_indices[i];
    m_indices.resize(write);
    return *this;
}

StringWhereProxy& StringWhereProxy::odd() {
    std::size_t write = 0;
    for (std::size_t i = 1; i < m_indices.size(); i += 2) m_indices[write++] = m_indices[i];
    m_indices.resize(write);
    return *this;
}

StringWhereProxy& StringWhereProxy::step(std::size_t n) {
    if (n <= 1) return *this;
    std::size_t write = 0;
    for (std::size_t i = 0; i < m_indices.size(); i += n) m_indices[write++] = m_indices[i];
    m_indices.resize(write);
    return *this;
}

StringWhereProxy& StringWhereProxy::except(const collections::DynamicArray<std::size_t>& positions) {
    std::size_t write = 0;
    for (std::size_t i = 0; i < m_indices.size(); ++i) {
        bool skip_it = false;
        for (std::size_t j = 0; j < positions.size() && !skip_it; ++j) skip_it = positions[j] == i;
        if (!skip_it) m_indices[write++] = m_indices[i];
    }
    m_indices.resize(write);
    return *this;
}

StringWhereProxy& StringWhereProxy::invert() {
    collections::DynamicArray<std::size_t> inv;
    const std::size_t sz = m_str.size();
    inv.reserve(sz > m_indices.size() ? sz - m_indices.size() : 0);
    std::size_t k = 0;
    for (std::size_t i = 0; i < sz; ++i) {
        while (k < m_indices.size() && m_indices[k] < i) ++k;
        if (k < m_indices.size() && m_indices[k] == i) continue;
        inv.push_back(i);
    }
    m_indices = std::move(inv);
    return *this;
}

StringWhereProxy& StringWhereProxy::unique() {
    bool        seen[256] = {};
    std::size_t write     = 0;
    for (std::size_t i = 0; i < m_indices.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(m_str.m_ptr[m_indices[i]]);
        if (!seen[c]) {
            seen[c]            = true;
            m_indices[write++] = m_indices[i];
        }
    }
    m_indices.resize(write);
    return *this;
}

bool StringWhereProxy::contains(char ch) const noexcept {
    for (std::size_t i = 0; i < m_indices.size(); ++i) {
        if (m_str.m_ptr[m_indices[i]] == ch) return true;
    }
    return false;
}

String StringWhereProxy::values() const {
    String result;
    result.reserve(m_indices.size());
    for (std::size_t i = 0; i < m_indices.size(); ++i) result.push_back(m_str.m_ptr[m_indices[i]]);
    return result;
}

std::size_t StringWhereProxy::remove() {
    const std::size_t n = m_indices.size();
    erase_indices();
    m_indices.clear();
    return n;
}

std::size_t StringWhereProxy::remove_unordered() {
    const std::size_t n = m_indices.size();
    for (std::size_t i = n; i > 0; --i) {
        const std::size_t idx = m_indices[i - 1];
        m_str.m_ptr[idx] = m_str.m_ptr[m_str.m_size - 1];
        m_str.set_size(m_str.m_size - 1);
    }
    m_indices.clear();
    return n;
}

String StringWhereProxy::pop() {
    String result = values();
    erase_indices();
    m_indices.clear();
    return result;
}

String StringWhereProxy::pop_unordered() {
    String result;
    result.reserve(m_indices.size());
    for (std::size_t i = m_indices.size(); i > 0; --i) {
        const std::size_t idx = m_indices[i - 1];
        result.push_back(m_str.m_ptr[idx]);
        m_str.m_ptr[idx] = m_str.m_ptr[m_str.m_size - 1];
        m_str.set_size(m_str.m_size - 1);
    }
    m_indices.clear();
    return result;
}

StringWhereProxy& StringWhereProxy::sort() {
    return sort([](char a, char b) { return static_cast<unsigned char>(a) < static_cast<unsigned char>(b); });
}

StringWhereProxy& StringWhereProxy::reverse() {
    const std::size_t n = m_indices.size();
    if (n <= 1) return *this;
    std::size_t lo = 0, hi = n - 1;
    while (lo < hi) std::swap(m_str.m_ptr[m_indices[lo++]], m_str.m_ptr[m_indices[hi--]]);
    return *this;
}

StringWhereProxy& StringWhereProxy::shuffle() {
    const std::size_t n = m_indices.size();
    for (std::size_t i = n; i > 1; --i) {
        const std::size_t j = random_int<std::size_t>(0, i - 1);
        std::swap(m_str.m_ptr[m_indices[i - 1]], m_str.m_ptr[m_indices[j]]);
    }
    return *this;
}

StringWhereProxy& StringWhereProxy::to_upper() {
    for (std::size_t i = 0; i < m_indices.size(); ++i) {
        char& c = m_str.m_ptr[m_indices[i]];
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    }
    return *this;
}

StringWhereProxy& StringWhereProxy::to_lower() {
    for (std::size_t i = 0; i < m_indices.size(); ++i) {
        char& c = m_str.m_ptr[m_indices[i]];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    }
    return *this;
}

String StringWhereProxy::uppercased() const {
    String r(m_str);
    for (std::size_t i = 0; i < m_indices.size(); ++i) {
        char& c = r.m_ptr[m_indices[i]];
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    }
    return r;
}

String StringWhereProxy::lowercased() const {
    String r(m_str);
    for (std::size_t i = 0; i < m_indices.size(); ++i) {
        char& c = r.m_ptr[m_indices[i]];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    }
    return r;
}

void StringWhereProxy::copy_to(String& dest) const {
    dest.reserve(dest.size() + m_indices.size());
    for (std::size_t i = 0; i < m_indices.size(); ++i) dest.push_back(m_str.m_ptr[m_indices[i]]);
}

void StringWhereProxy::move_to(String& dest) {
    copy_to(dest);
    erase_indices();
    m_indices.clear();
}

void StringWhereProxy::insert_before(char value) {
    for (std::size_t i = m_indices.size(); i > 0; --i) m_str.insert(m_indices[i - 1], value);
}

void StringWhereProxy::insert_after(char value) {
    for (std::size_t i = m_indices.size(); i > 0; --i) m_str.insert(m_indices[i - 1] + 1, value);
}

String StringWhereProxy::swap_with(char value) {
    String result;
    result.reserve(m_indices.size());
    for (std::size_t i = 0; i < m_indices.size(); ++i) {
        result.push_back(m_str.m_ptr[m_indices[i]]);
        m_str.m_ptr[m_indices[i]] = value;
    }
    return result;
}

} // namespace fizmo
