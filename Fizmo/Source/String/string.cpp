#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "string.hpp"

namespace fizmo {

auto String::assign_raw(const char* str, std::size_t len) -> void {
        m_data.clear();
        m_null_present = false;
        if (str && len > 0) {
            m_data.reserve(len + 1);
            for (std::size_t i = 0; i < len; ++i) { m_data.push_back(str[i]); }
            m_data.push_back('\0');
            m_null_present = true;
        }
    }

auto String::append_raw(const char* str, std::size_t len) -> void {
        if (!str || len == 0) return;
        if (m_null_present) { m_data.remove_back(); m_null_present = false; }
        m_data.ensure_capacity(m_data.size() + len + 1);
        for (std::size_t i = 0; i < len; ++i) { m_data.push_back(str[i]); }
        m_data.push_back('\0');
        m_null_present = true;
    }

auto String::str_equal(const char* a, std::size_t a_len, const char* b, std::size_t b_len) noexcept -> bool {
        if (a_len != b_len) return false;
        for (std::size_t i = 0; i < a_len; ++i) { if (a[i] != b[i]) return false; }
        return true;
    }

auto String::str_compare(const char* a, std::size_t a_len, const char* b, std::size_t b_len) noexcept -> int {
        std::size_t len = a_len < b_len ? a_len : b_len;
        for (std::size_t i = 0; i < len; ++i) {
            if (a[i] < b[i]) return -1;
            if (a[i] > b[i]) return 1;
        }
        if (a_len < b_len) return -1;
        if (a_len > b_len) return 1;
        return 0;
    }

auto String::collect_char_indices(char ch) const -> DynamicArray<std::size_t> {
        DynamicArray<std::size_t> idx;
        for (std::size_t i = 0; i < size(); ++i) { if (m_data[i] == ch) idx.push_back(i); }
        return idx;
    }

auto String::glob_match(const char* s, std::size_t slen, const char* p, std::size_t plen) noexcept -> bool {
        std::size_t si = 0, pi = 0, star_pi = npos, star_si = 0;
        while (si < slen) {
            if (pi < plen && (p[pi] == '?' || p[pi] == s[si])) { ++si; ++pi; }
            else if (pi < plen && p[pi] == '*') { star_pi = pi++; star_si = si; }
            else if (star_pi != npos) { pi = star_pi + 1; si = ++star_si; }
            else return false;
        }
        while (pi < plen && p[pi] == '*') ++pi;
        return pi == plen;
    }

String::String(String&& other) noexcept : m_data(std::move(other.m_data)), m_null_present(other.m_null_present) { other.m_null_present = false; }

String::String(char ch) : m_data(), m_null_present(false) {
        m_data.reserve(2);
        m_data.push_back(ch);
        m_data.push_back('\0');
        m_null_present = true;
    }

String::String(std::size_t count, char ch) : m_data(), m_null_present(false) {
        m_data.reserve(count + 1);
        for (std::size_t i = 0; i < count; ++i) { m_data.push_back(ch); }
        m_data.push_back('\0');
        m_null_present = true;
    }

auto String::operator=(String&& other) noexcept -> String& {
        if (this != &other) { m_data = std::move(other.m_data); m_null_present = other.m_null_present; other.m_null_present = false; }
        return *this;
    }

auto String::operator=(char ch) -> String& {
        m_data.clear();
        m_null_present = false;
        m_data.reserve(2);
        m_data.push_back(ch);
        m_data.push_back('\0');
        m_null_present = true;
        return *this;
    }

auto String::resize(std::size_t new_size) -> void {
        if (new_size == 0) { m_data.clear(); m_null_present = false; return; }
        std::size_t old_size = size();
        m_data.resize(new_size + 1);
        for (std::size_t i = old_size; i < new_size; ++i) { m_data[i] = '\0'; }
        m_data[new_size] = '\0';
        m_null_present = true;
    }

auto String::resize(std::size_t new_size, char ch) -> void {
        if (new_size == 0) { m_data.clear(); m_null_present = false; return; }
        std::size_t old_size = size();
        m_data.resize(new_size + 1);
        for (std::size_t i = old_size; i < new_size; ++i) { m_data[i] = ch; }
        m_data[new_size] = '\0';
        m_null_present = true;
    }

auto String::view(std::size_t start, std::size_t count) const noexcept -> StringView {
        if (start >= size()) return StringView(nullptr, 0);
        std::size_t remaining = size() - start;
        std::size_t len = count < remaining ? count : remaining;
        return StringView(c_str() + start, len);
    }

auto String::compare_ignore_case(StringView sv) const noexcept -> int {
        std::size_t len = size() < sv.size() ? size() : sv.size();
        for (std::size_t i = 0; i < len; ++i) {
            char a = impl_to_lower(m_data[i]);
            char b = impl_to_lower(sv[i]);
            if (a < b) return -1;
            if (a > b) return  1;
        }
        if (size() < sv.size()) return -1;
        if (size() > sv.size()) return  1;
        return 0;
    }

auto String::operator+=(char ch) -> String& {
        if (m_null_present) { m_data.remove_back(); m_null_present = false; }
        m_data.push_back(ch);
        m_data.push_back('\0');
        m_null_present = true;
        return *this;
    }

auto String::to_bool() const noexcept -> bool {
        StringView v = view().trimmed();
        if (v == StringView("true")  || v == StringView("1") || v == StringView("yes") || v == StringView("on"))  return true;
        if (v == StringView("false") || v == StringView("0") || v == StringView("no")  || v == StringView("off")) return false;
        return !empty();
    }

auto String::is_integer() const noexcept -> bool {
        if (empty()) return false;
        std::size_t i = 0;
        if (m_data[i] == '-' || m_data[i] == '+') ++i;
        if (i == size()) return false;
        for (; i < size(); ++i) { if (!std::isdigit(static_cast<unsigned char>(m_data[i]))) return false; }
        return true;
    }

auto String::is_float() const noexcept -> bool {
        if (empty()) return false;
        std::size_t i = 0;
        if (m_data[i] == '-' || m_data[i] == '+') ++i;
        bool has_digit = false, has_dot = false, has_exp = false;
        for (; i < size(); ++i) {
            char c = m_data[i];
            if (std::isdigit(static_cast<unsigned char>(c))) { has_digit = true; }
            else if (c == '.' && !has_dot && !has_exp) { has_dot = true; }
            else if ((c == 'e' || c == 'E') && has_digit && !has_exp) {
                has_exp = true;
                if (i + 1 < size() && (m_data[i + 1] == '+' || m_data[i + 1] == '-')) ++i;
            } else return false;
        }
        return has_digit;
    }

auto String::from_float(float v, int prec) -> String {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*f", prec, static_cast<double>(v));
        return String(buf);
    }

auto String::format(const char* fmt, ...) -> String {
        va_list args1, args2;
        va_start(args1, fmt);
        va_copy(args2, args1);
        int needed = std::vsnprintf(nullptr, 0, fmt, args1);
        va_end(args1);
        if (needed < 0) { va_end(args2); return String(); }
        String result;
        result.resize(static_cast<std::size_t>(needed));
        std::vsnprintf(result.c_str(), static_cast<std::size_t>(needed) + 1, fmt, args2);
        va_end(args2);
        return result;
    }

auto String::from_stream(std::istream& is) -> String {
        String result;
        char buf[4096];
        while (is.read(buf, sizeof(buf))) {
            result.append_raw(buf, static_cast<std::size_t>(is.gcount()));
        }
        if (is.gcount() > 0) result.append_raw(buf, static_cast<std::size_t>(is.gcount()));
        return result;
    }

auto String::to_hex() const -> String {
        static const char hex_chars[] = "0123456789abcdef";
        String result;
        result.reserve(size() * 2 + 1);
        for (std::size_t i = 0; i < size(); ++i) {
            unsigned char c = static_cast<unsigned char>(m_data[i]);
            result += hex_chars[c >> 4];
            result += hex_chars[c & 0xf];
        }
        return result;
    }

auto String::from_hex(StringView sv) -> String {
        String result;
        if (sv.size() % 2 != 0) return result;
        result.reserve(sv.size() / 2 + 1);
        for (std::size_t i = 0; i < sv.size(); i += 2) {
            auto hv = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hv(sv[i]), lo = hv(sv[i + 1]);
            if (hi < 0 || lo < 0) return String();
            result += static_cast<char>((hi << 4) | lo);
        }
        return result;
    }

auto String::to_base64() const -> String {
        static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::size_t n = size();
        String result;
        result.reserve(((n + 2) / 3) * 4 + 1);
        for (std::size_t i = 0; i < n; i += 3) {
            unsigned char a = static_cast<unsigned char>(m_data[i]);
            unsigned char b = (i + 1 < n) ? static_cast<unsigned char>(m_data[i + 1]) : 0;
            unsigned char c = (i + 2 < n) ? static_cast<unsigned char>(m_data[i + 2]) : 0;
            result += b64[a >> 2];
            result += b64[((a & 3) << 4) | (b >> 4)];
            result += (i + 1 < n) ? b64[((b & 0xf) << 2) | (c >> 6)] : '=';
            result += (i + 2 < n) ? b64[c & 0x3f] : '=';
        }
        return result;
    }

auto String::from_base64(StringView sv) -> String {
        auto bv = [](char c) -> int {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            return -1;
        };
        String result;
        std::size_t n = sv.size();
        result.reserve((n / 4) * 3 + 1);
        for (std::size_t i = 0; i < n; i += 4) {
            int a = bv(sv[i]);
            int b = (i + 1 < n) ? bv(sv[i + 1]) : 0;
            int c = (i + 2 < n && sv[i + 2] != '=') ? bv(sv[i + 2]) : 0;
            int d = (i + 3 < n && sv[i + 3] != '=') ? bv(sv[i + 3]) : 0;
            if (a < 0 || b < 0) return String();
            result += static_cast<char>((a << 2) | (b >> 4));
            if (i + 2 < n && sv[i + 2] != '=') result += static_cast<char>(((b & 0xf) << 4) | (c >> 2));
            if (i + 3 < n && sv[i + 3] != '=') result += static_cast<char>(((c & 3) << 6) | d);
        }
        return result;
    }

auto String::escape(StringView chars) const -> String {
        String result;
        result.reserve(size() + 1);
        for (std::size_t i = 0; i < size(); ++i) {
            if (m_data[i] == '\\' || chars.contains(m_data[i])) result += '\\';
            result += m_data[i];
        }
        return result;
    }

auto String::unescape() const -> String {
        String result;
        result.reserve(size() + 1);
        for (std::size_t i = 0; i < size(); ++i) {
            if (m_data[i] == '\\' && i + 1 < size()) { ++i; }
            result += m_data[i];
        }
        return result;
    }

auto String::url_encode() const -> String {
        static const char hex[] = "0123456789ABCDEF";
        String result;
        result.reserve(size() + 1);
        for (std::size_t i = 0; i < size(); ++i) {
            unsigned char c = static_cast<unsigned char>(m_data[i]);
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                result += static_cast<char>(c);
            } else {
                result += '%';
                result += hex[c >> 4];
                result += hex[c & 0xf];
            }
        }
        return result;
    }

auto String::url_decode() const -> String {
        auto hv = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        String result;
        result.reserve(size() + 1);
        for (std::size_t i = 0; i < size(); ++i) {
            if (m_data[i] == '%' && i + 2 < size()) {
                int hi = hv(m_data[i + 1]), lo = hv(m_data[i + 2]);
                if (hi >= 0 && lo >= 0) { result += static_cast<char>((hi << 4) | lo); i += 2; continue; }
            } else if (m_data[i] == '+') { result += ' '; continue; }
            result += m_data[i];
        }
        return result;
    }

auto String::hash() const noexcept -> std::size_t {
        std::size_t h = 14695981039346656037ULL;
        for (std::size_t i = 0; i < size(); ++i) {
            h ^= static_cast<unsigned char>(m_data[i]);
            h *= 1099511628211ULL;
        }
        return h;
    }

auto String::substring(std::size_t pos, std::size_t count) const -> String {
        if (pos >= size()) return String();
        std::size_t len = (count > size() - pos) ? (size() - pos) : count;
        return String(c_str() + pos, len);
    }

auto String::append(std::size_t n, char ch) -> String& {
        if (m_null_present) { m_data.remove_back(); m_null_present = false; }
        m_data.reserve(m_data.size() + n + 1);
        for (std::size_t i = 0; i < n; ++i) m_data.push_back(ch);
        m_data.push_back('\0');
        m_null_present = true;
        return *this;
    }

auto String::insert(std::size_t idx, char ch) -> String& {
        if (idx > size()) idx = size();
        strip_null_terminator();
        m_data.insert(idx, ch);
        restore_null_terminator();
        return *this;
    }

auto String::insert(std::size_t idx, const char* str, std::size_t len) -> String& {
        if (len == 0) return *this;
        if (idx > size()) idx = size();
        strip_null_terminator();
        m_data.insert(idx, str, len);
        restore_null_terminator();
        return *this;
    }

auto String::erase(std::size_t pos, std::size_t count) -> String& {
        if (pos >= size()) return *this;
        std::size_t n = (count > size() - pos) ? (size() - pos) : count;
        strip_null_terminator();
        for (std::size_t i = pos; i + n < m_data.size(); ++i) m_data[i] = m_data[i + n];
        m_data.resize(m_data.size() - n);
        restore_null_terminator();
        return *this;
    }

auto String::find(StringView sv, std::size_t pos) const noexcept -> std::size_t {
        if (sv.empty()) return pos <= size() ? pos : npos;
        if (sv.size() > size()) return npos;
        for (std::size_t i = pos; i <= size() - sv.size(); ++i) {
            bool ok = true;
            for (std::size_t j = 0; j < sv.size(); ++j) {
                if (m_data[i + j] != sv[j]) { ok = false; break; }
            }
            if (ok) return i;
        }
        return npos;
    }

auto String::find_last(char ch, std::size_t before) const noexcept -> std::size_t {
        std::size_t lim = (before < size()) ? before : size();
        for (std::size_t i = lim; i > 0; --i) { if (m_data[i - 1] == ch) return i - 1; }
        return npos;
    }

auto String::rfind(StringView sv, std::size_t before) const noexcept -> std::size_t {
        if (sv.empty()) return size();
        std::size_t lim = before < size() ? before : size();
        if (sv.size() > lim) return npos;
        for (std::size_t i = lim - sv.size() + 1; i > 0; --i) {
            std::size_t pos = i - 1;
            bool ok = true;
            for (std::size_t j = 0; j < sv.size(); ++j) {
                if (m_data[pos + j] != sv[j]) { ok = false; break; }
            }
            if (ok) return pos;
        }
        return npos;
    }

auto String::find_all(StringView sv) const -> DynamicArray<std::size_t> {
        DynamicArray<std::size_t> result;
        if (sv.empty() || sv.size() > size()) return result;
        std::size_t pos = 0;
        while ((pos = find(sv, pos)) != npos) { result.push_back(pos); pos += sv.size(); }
        return result;
    }

auto String::find_nth(char ch, std::size_t n) const noexcept -> std::size_t {
        if (n == 0) return npos;
        std::size_t count = 0;
        for (std::size_t i = 0; i < size(); ++i) {
            if (m_data[i] == ch && ++count == n) return i;
        }
        return npos;
    }

auto String::find_nth(StringView sv, std::size_t n) const noexcept -> std::size_t {
        if (sv.empty() || n == 0) return npos;
        std::size_t count = 0, pos = 0;
        while ((pos = find(sv, pos)) != npos) {
            if (++count == n) return pos;
            pos += sv.size();
        }
        return npos;
    }

auto String::find_between(StringView open, StringView close) const -> String {
        std::size_t s = find(open);
        if (s == npos) return String();
        s += open.size();
        std::size_t e = find(close, s);
        if (e == npos) return String();
        return substring(s, e - s);
    }

auto String::find_first_of(StringView charset, std::size_t pos) const noexcept -> std::size_t {
        for (std::size_t i = pos; i < size(); ++i) {
            for (std::size_t j = 0; j < charset.size(); ++j) {
                if (m_data[i] == charset[j]) return i;
            }
        }
        return npos;
    }

auto String::find_last_of(StringView charset, std::size_t before) const noexcept -> std::size_t {
        std::size_t lim = (before < size()) ? before : size();
        for (std::size_t i = lim; i > 0; --i) {
            for (std::size_t j = 0; j < charset.size(); ++j) {
                if (m_data[i - 1] == charset[j]) return i - 1;
            }
        }
        return npos;
    }

auto String::find_first_not_of(StringView charset, std::size_t pos) const noexcept -> std::size_t {
        for (std::size_t i = pos; i < size(); ++i) {
            bool found = false;
            for (std::size_t j = 0; j < charset.size(); ++j) {
                if (m_data[i] == charset[j]) { found = true; break; }
            }
            if (!found) return i;
        }
        return npos;
    }

auto String::find_last_not_of(StringView charset, std::size_t before) const noexcept -> std::size_t {
        std::size_t lim = (before < size()) ? before : size();
        for (std::size_t i = lim; i > 0; --i) {
            bool found = false;
            for (std::size_t j = 0; j < charset.size(); ++j) {
                if (m_data[i - 1] == charset[j]) { found = true; break; }
            }
            if (!found) return i - 1;
        }
        return npos;
    }

auto String::count_occurrences(StringView sv) const noexcept -> std::size_t {
        if (sv.empty() || sv.size() > size()) return 0;
        std::size_t n = 0, pos = 0;
        while ((pos = find(sv, pos)) != npos) { ++n; pos += sv.size(); }
        return n;
    }

auto String::is_alpha() const noexcept -> bool {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) { if (!std::isalpha(static_cast<unsigned char>(m_data[i]))) return false; }
        return true;
    }

auto String::is_digit() const noexcept -> bool {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) { if (!std::isdigit(static_cast<unsigned char>(m_data[i]))) return false; }
        return true;
    }

auto String::is_alnum() const noexcept -> bool {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) { if (!std::isalnum(static_cast<unsigned char>(m_data[i]))) return false; }
        return true;
    }

auto String::is_whitespace_only() const noexcept -> bool {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) { if (!is_whitespace(m_data[i])) return false; }
        return true;
    }

auto String::is_hex() const noexcept -> bool {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) {
            char c = m_data[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
        }
        return true;
    }

auto String::is_upper() const noexcept -> bool {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) {
            if (std::isalpha(static_cast<unsigned char>(m_data[i])) &&
                !std::isupper(static_cast<unsigned char>(m_data[i]))) return false;
        }
        return true;
    }

auto String::is_lower() const noexcept -> bool {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) {
            if (std::isalpha(static_cast<unsigned char>(m_data[i])) &&
                !std::islower(static_cast<unsigned char>(m_data[i]))) return false;
        }
        return true;
    }

auto String::is_palindrome() const noexcept -> bool {
        if (size() <= 1) return true;
        std::size_t lo = 0, hi = size() - 1;
        while (lo < hi) { if (m_data[lo++] != m_data[hi--]) return false; }
        return true;
    }

auto String::levenshtein_distance(StringView other) const -> std::size_t {
        std::size_t m = size(), n = other.size();
        DynamicArray<std::size_t> row(n + 1);
        for (std::size_t j = 0; j <= n; ++j) row[j] = j;
        for (std::size_t i = 1; i <= m; ++i) {
            std::size_t prev = i - 1;
            row[0] = i;
            for (std::size_t j = 1; j <= n; ++j) {
                std::size_t tmp = row[j];
                if (m_data[i - 1] == other[j - 1]) {
                    row[j] = prev;
                } else {
                    std::size_t a = prev, b = row[j], c = row[j - 1];
                    row[j] = 1 + (a < b ? (a < c ? a : c) : (b < c ? b : c));
                }
                prev = tmp;
            }
        }
        return row[n];
    }

auto String::common_prefix(StringView other) const noexcept -> StringView {
        std::size_t n = size() < other.size() ? size() : other.size();
        std::size_t i = 0;
        while (i < n && m_data[i] == other[i]) ++i;
        return StringView(c_str(), i);
    }

auto String::common_suffix(StringView other) const noexcept -> StringView {
        std::size_t n = size() < other.size() ? size() : other.size();
        std::size_t i = 0;
        while (i < n && m_data[size() - 1 - i] == other[other.size() - 1 - i]) ++i;
        return StringView(c_str() + size() - i, i);
    }

auto String::word_count() const noexcept -> std::size_t {
        std::size_t n = 0;
        bool in_word = false;
        for (std::size_t i = 0; i < size(); ++i) {
            if (!is_whitespace(m_data[i])) { if (!in_word) { ++n; in_word = true; } }
            else                           { in_word = false; }
        }
        return n;
    }

auto String::min() const noexcept -> char {
        if (empty()) return '\0';
        char m = m_data[0];
        for (std::size_t i = 1; i < size(); ++i) { if (m_data[i] < m) m = m_data[i]; }
        return m;
    }

auto String::max() const noexcept -> char {
        if (empty()) return '\0';
        char m = m_data[0];
        for (std::size_t i = 1; i < size(); ++i) { if (m_data[i] > m) m = m_data[i]; }
        return m;
    }

auto String::min_max() const noexcept -> std::pair<char, char> {
        if (empty()) return { '\0', '\0' };
        char lo = m_data[0], hi = m_data[0];
        for (std::size_t i = 1; i < size(); ++i) {
            if (m_data[i] < lo) lo = m_data[i];
            if (m_data[i] > hi) hi = m_data[i];
        }
        return { lo, hi };
    }

auto String::to_upper(std::size_t start, std::size_t end_idx) -> void {
        if (end_idx > size()) end_idx = size();
        for (std::size_t i = start; i < end_idx; ++i) { m_data[i] = impl_to_upper(m_data[i]); }
    }

auto String::to_lower(std::size_t start, std::size_t end_idx) -> void {
        if (end_idx > size()) end_idx = size();
        for (std::size_t i = start; i < end_idx; ++i) { m_data[i] = impl_to_lower(m_data[i]); }
    }

auto String::title_case() -> void {
        bool new_word = true;
        for (std::size_t i = 0; i < size(); ++i) {
            if (is_whitespace(m_data[i])) { new_word = true; }
            else if (new_word) { m_data[i] = impl_to_upper(m_data[i]); new_word = false; }
            else { m_data[i] = impl_to_lower(m_data[i]); }
        }
    }

auto String::snake_to_camel() const -> String {
        String result;
        result.reserve(size() + 1);
        bool cap_next = false;
        for (std::size_t i = 0; i < size(); ++i) {
            if (m_data[i] == '_') { cap_next = true; }
            else if (cap_next) { result += impl_to_upper(m_data[i]); cap_next = false; }
            else { result += m_data[i]; }
        }
        return result;
    }

auto String::camel_to_snake() const -> String {
        String result;
        result.reserve(size() + size() / 4 + 1);
        for (std::size_t i = 0; i < size(); ++i) {
            if (std::isupper(static_cast<unsigned char>(m_data[i])) && i > 0) result += '_';
            result += impl_to_lower(m_data[i]);
        }
        return result;
    }

auto String::rtrim() -> void {
        std::size_t s = size();
        while (s > 0 && is_whitespace(m_data[s - 1])) --s;
        m_data.resize(s + 1);
        m_data[s] = '\0';
    }

auto String::normalize_whitespace() -> void {
        String result;
        result.reserve(size() + 1);
        bool in_ws = false;
        std::size_t i = 0;
        while (i < size() && is_whitespace(m_data[i])) ++i;
        for (; i < size(); ++i) {
            if (is_whitespace(m_data[i])) {
                if (!in_ws) { result += ' '; in_ws = true; }
            } else {
                result += m_data[i];
                in_ws = false;
            }
        }
        if (!result.empty() && is_whitespace(result.back())) result.remove_back();
        *this = std::move(result);
    }

auto String::indent(std::size_t n, char pad) const -> String {
        if (empty() || n == 0) return *this;
        String prefix(n, pad);
        DynamicArray<String> ls = lines(true);
        String result;
        result.reserve(size() + ls.size() * n + 1);
        for (std::size_t i = 0; i < ls.size(); ++i) {
            if (i > 0) result += '\n';
            result += prefix;
            result += ls[i];
        }
        return result;
    }

auto String::wrap(std::size_t width) const -> String {
        if (width == 0 || empty()) return *this;
        DynamicArray<String> words = split_whitespace();
        String result;
        std::size_t col = 0;
        for (std::size_t i = 0; i < words.size(); ++i) {
            std::size_t wlen = words[i].size();
            if (col > 0 && col + 1 + wlen > width) { result += '\n'; col = 0; }
            else if (col > 0) { result += ' '; ++col; }
            result += words[i];
            col += wlen;
        }
        return result;
    }

auto String::truncate_to(std::size_t max_len, StringView ellipsis) const -> String {
        if (size() <= max_len) return *this;
        if (ellipsis.size() >= max_len) return String(ellipsis.substring(0, max_len));
        return substring(0, max_len - ellipsis.size()) + ellipsis;
    }

auto String::remove_by_value(char ch, std::size_t count, Direction dir) -> std::size_t {
        strip_null_terminator();
        std::size_t n = m_data.remove_by_value(ch, count, dir);
        restore_null_terminator();
        return n;
    }

auto String::replace_by_value(char old_val, char new_val, std::size_t count, Direction dir) -> std::size_t {
        strip_null_terminator();
        std::size_t n = m_data.replace_by_value(old_val, new_val, count, dir);
        restore_null_terminator();
        return n;
    }

auto String::replace_str(StringView old_sv, StringView new_sv,
                             std::size_t max_count, Direction dir) -> std::size_t {
        if (old_sv.empty() || old_sv.size() > size()) return 0;

        DynamicArray<std::size_t> hits;
        std::size_t pos = 0;
        while (pos + old_sv.size() <= size()) {
            std::size_t found = find(old_sv, pos);
            if (found == npos) break;
            hits.push_back(found);
            pos = found + old_sv.size();
        }

        if (hits.empty()) return 0;

        if (max_count > 0 && max_count < hits.size()) {
            if (dir == Direction::Front) {
                hits.resize(max_count);
            } else if (dir == Direction::Back) {
                std::size_t start = hits.size() - max_count;
                for (std::size_t i = 0; i < max_count; ++i) hits[i] = hits[start + i];
                hits.resize(max_count);
            }
        }

        std::size_t count = hits.size();
        if (count == 0) return 0;

        std::size_t old_len = old_sv.size(), new_len = new_sv.size();
        std::size_t new_total = size() + count * new_len - count * old_len;
        String result;
        result.reserve(new_total + 1);

        std::size_t src = 0, hi = 0;
        while (src < size()) {
            if (hi < count && src == hits[hi]) {
                result.append(new_sv);
                src += old_len;
                ++hi;
            } else {
                result += m_data[src++];
            }
        }

        *this = std::move(result);
        return count;
    }

auto String::distinct() const -> String {
        String result;
        for (std::size_t i = 0; i < size(); ++i) { if (result.find(m_data[i]) == npos) result += m_data[i]; }
        return result;
    }

auto String::rotate_left(std::size_t n) -> void {
        if (size() <= 1) return;
        strip_null_terminator(); m_data.rotate_left(n); restore_null_terminator();
    }

auto String::rotate_right(std::size_t n) -> void {
        if (size() <= 1) return;
        strip_null_terminator(); m_data.rotate_right(n); restore_null_terminator();
    }

auto String::unsecure_shuffle() -> void {
        if (size() <= 1) return;
        strip_null_terminator(); m_data.unsecure_shuffle(); restore_null_terminator();
    }

auto String::split(char delim, bool keep_empty) const -> DynamicArray<String> {
        DynamicArray<String> parts;
        std::size_t start = 0;
        for (std::size_t i = 0; i <= size(); ++i) {
            bool at_end = (i == size());
            if (at_end || m_data[i] == delim) {
                if (keep_empty || i > start)
                    parts.push_back(substring(start, i - start));
                start = i + 1;
            }
        }
        return parts;
    }

auto String::split(StringView delim, bool keep_empty) const -> DynamicArray<String> {
        DynamicArray<String> parts;
        if (delim.empty()) { parts.push_back(*this); return parts; }
        std::size_t start = 0;
        while (true) {
            std::size_t pos = find(delim, start);
            if (pos == npos) { if (keep_empty || start < size()) parts.push_back(substring(start)); break; }
            if (keep_empty || pos > start) parts.push_back(substring(start, pos - start));
            start = pos + delim.size();
        }
        return parts;
    }

auto String::split_whitespace() const -> DynamicArray<String> {
        DynamicArray<String> parts;
        std::size_t i = 0;
        while (i < size()) {
            while (i < size() && is_whitespace(m_data[i])) ++i;
            std::size_t start = i;
            while (i < size() && !is_whitespace(m_data[i])) ++i;
            if (i > start) parts.push_back(substring(start, i - start));
        }
        return parts;
    }

auto String::split_n(StringView delim, std::size_t max_parts) const -> DynamicArray<String> {
        DynamicArray<String> parts;
        if (delim.empty() || max_parts == 0) { parts.push_back(*this); return parts; }
        std::size_t start = 0;
        while (parts.size() + 1 < max_parts) {
            std::size_t pos = find(delim, start);
            if (pos == npos) break;
            parts.push_back(substring(start, pos - start));
            start = pos + delim.size();
        }
        parts.push_back(substring(start));
        return parts;
    }

auto String::rsplit(StringView delim, std::size_t n) const -> DynamicArray<String> {
        DynamicArray<String> parts;
        if (delim.empty() || n == 0) { parts.push_back(*this); return parts; }
        DynamicArray<std::size_t> all_pos = find_all(delim);
        if (all_pos.empty()) { parts.push_back(*this); return parts; }
        std::size_t split_count = all_pos.size() < n ? all_pos.size() : n;
        std::size_t first_split = all_pos.size() - split_count;
        std::size_t prev = 0;
        if (first_split > 0) {
            parts.push_back(substring(0, all_pos[first_split]));
            prev = all_pos[first_split] + delim.size();
            for (std::size_t i = first_split + 1; i < all_pos.size(); ++i) {
                parts.push_back(substring(prev, all_pos[i] - prev));
                prev = all_pos[i] + delim.size();
            }
        } else {
            for (std::size_t i = 0; i < all_pos.size(); ++i) {
                parts.push_back(substring(prev, all_pos[i] - prev));
                prev = all_pos[i] + delim.size();
            }
        }
        parts.push_back(substring(prev));
        return parts;
    }

auto String::lines(bool keep_empty) const -> DynamicArray<String> {
        DynamicArray<String> result;
        std::size_t i = 0, start = 0;
        while (i < size()) {
            if (m_data[i] == '\n') {
                std::size_t end = i;
                if (end > start && m_data[end - 1] == '\r') --end;
                if (keep_empty || end > start) result.push_back(substring(start, end - start));
                start = i + 1;
            }
            ++i;
        }
        if (keep_empty || start < size()) result.push_back(substring(start));
        return result;
    }

auto String::chunks(std::size_t n) const -> DynamicArray<String> {
        DynamicArray<String> result;
        if (n == 0) return result;
        for (std::size_t i = 0; i < size(); i += n) result.push_back(substring(i, n));
        return result;
    }

auto String::join(const DynamicArray<String>& parts, StringView sep) -> String {
        if (parts.empty()) return String();
        std::size_t total = sep.size() * (parts.size() - 1);
        for (std::size_t i = 0; i < parts.size(); ++i) total += parts[i].size();
        String result;
        result.reserve(total + 1);
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (i) result.append(sep);
            result.append(parts[i]);
        }
        return result;
    }

auto String::join(const DynamicArray<StringView>& parts, StringView sep) -> String {
        if (parts.empty()) return String();
        std::size_t total = sep.size() * (parts.size() - 1);
        for (std::size_t i = 0; i < parts.size(); ++i) total += parts[i].size();
        String result;
        result.reserve(total + 1);
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (i) result.append(sep);
            result.append(parts[i]);
        }
        return result;
    }

auto String::repeated(std::size_t n) const -> String {
        if (n == 0 || empty()) return String();
        String result;
        result.reserve(size() * n + 1);
        for (std::size_t i = 0; i < n; ++i) result.append(*this);
        return result;
    }

auto String::padded_left(std::size_t total_width, char pad) const -> String {
        if (size() >= total_width) return *this;
        String r(total_width - size(), pad);
        r.append(*this);
        return r;
    }

auto String::padded_right(std::size_t total_width, char pad) const -> String {
        if (size() >= total_width) return *this;
        String r(*this);
        r.append(total_width - size(), pad);
        return r;
    }

auto String::padded_center(std::size_t total_width, char pad) const -> String {
        if (size() >= total_width) return *this;
        std::size_t left = (total_width - size()) / 2;
        std::size_t right = total_width - size() - left;
        String r(left, pad);
        r.append(*this);
        r.append(right, pad);
        return r;
    }

auto StringWhereProxy::last(std::size_t n) -> StringWhereProxy& {
        if (n < m_indices.size()) {
            std::size_t start = m_indices.size() - n;
            for (std::size_t i = 0; i < n; ++i) m_indices[i] = m_indices[start + i];
            m_indices.resize(n);
        }
        return *this;
    }

auto StringWhereProxy::range(std::size_t start, std::size_t end_idx) -> StringWhereProxy& {
        std::size_t write = 0;
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            if (m_indices[i] >= start && m_indices[i] < end_idx) m_indices[write++] = m_indices[i];
        }
        m_indices.resize(write);
        return *this;
    }

auto StringWhereProxy::skip(std::size_t n) -> StringWhereProxy& {
        if (n >= m_indices.size()) { m_indices.clear(); }
        else {
            std::size_t new_count = m_indices.size() - n;
            for (std::size_t i = 0; i < new_count; ++i) m_indices[i] = m_indices[n + i];
            m_indices.resize(new_count);
        }
        return *this;
    }

auto StringWhereProxy::at(std::size_t n) -> StringWhereProxy& {
        if (n < m_indices.size()) { m_indices[0] = m_indices[n]; m_indices.resize(1); }
        else m_indices.clear();
        return *this;
    }

auto StringWhereProxy::slice(std::size_t start, std::size_t end_idx) -> StringWhereProxy& {
        if (start >= m_indices.size()) { m_indices.clear(); return *this; }
        if (end_idx > m_indices.size()) end_idx = m_indices.size();
        if (start >= end_idx) { m_indices.clear(); return *this; }
        std::size_t count = end_idx - start;
        for (std::size_t i = 0; i < count; ++i) m_indices[i] = m_indices[start + i];
        m_indices.resize(count);
        return *this;
    }

auto StringWhereProxy::even() -> StringWhereProxy& {
        std::size_t write = 0;
        for (std::size_t i = 0; i < m_indices.size(); i += 2) m_indices[write++] = m_indices[i];
        m_indices.resize(write);
        return *this;
    }

auto StringWhereProxy::odd() -> StringWhereProxy& {
        std::size_t write = 0;
        for (std::size_t i = 1; i < m_indices.size(); i += 2) m_indices[write++] = m_indices[i];
        m_indices.resize(write);
        return *this;
    }

auto StringWhereProxy::step(std::size_t n) -> StringWhereProxy& {
        if (n <= 1) return *this;
        std::size_t write = 0;
        for (std::size_t i = 0; i < m_indices.size(); i += n) m_indices[write++] = m_indices[i];
        m_indices.resize(write);
        return *this;
    }

auto StringWhereProxy::except(const DynamicArray<std::size_t>& positions) -> StringWhereProxy& {
        std::size_t write = 0;
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            if (!positions.contains(i)) m_indices[write++] = m_indices[i];
        }
        m_indices.resize(write);
        return *this;
    }

auto StringWhereProxy::invert() -> StringWhereProxy& {
        std::size_t sz = m_str.size();
        DynamicArray<std::size_t> inv;
        inv.reserve(sz > m_indices.size() ? sz - m_indices.size() : 0);
        for (std::size_t i = 0; i < sz; ++i) {
            if (!m_indices.contains(i)) inv.push_back(i);
        }
        m_indices = std::move(inv);
        return *this;
    }

auto StringWhereProxy::unique() -> StringWhereProxy& {
        DynamicArray<char> seen;
        std::size_t write = 0;
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            char c = m_str.m_data[m_indices[i]];
            if (!seen.contains(c)) { seen.push_back(c); m_indices[write++] = m_indices[i]; }
        }
        m_indices.resize(write);
        return *this;
    }

auto StringWhereProxy::contains(char ch) const noexcept -> bool {
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            if (m_str.m_data[m_indices[i]] == ch) return true;
        }
        return false;
    }

auto StringWhereProxy::values() const -> String {
        String result;
        result.reserve(m_indices.size() + 1);
        for (std::size_t i = 0; i < m_indices.size(); ++i) result += m_str.m_data[m_indices[i]];
        return result;
    }

auto StringWhereProxy::remove() -> std::size_t {
        std::size_t n = m_indices.size();
        if (n == 0) return 0;
        m_str.strip_null_terminator();
        for (std::size_t i = n; i > 0; --i) m_str.m_data.remove(m_indices[i - 1]);
        m_str.restore_null_terminator();
        return n;
    }

auto StringWhereProxy::remove_unordered() -> std::size_t {
        std::size_t n = m_indices.size();
        if (n == 0) return 0;
        m_str.strip_null_terminator();
        for (std::size_t i = n; i > 0; --i) m_str.m_data.remove_unordered(m_indices[i - 1]);
        m_str.restore_null_terminator();
        return n;
    }

auto StringWhereProxy::pop() -> String {
        std::size_t n = m_indices.size();
        String result;
        if (n == 0) return result;
        result.reserve(n + 1);
        for (std::size_t i = 0; i < n; ++i) result += m_str.m_data[m_indices[i]];
        m_str.strip_null_terminator();
        for (std::size_t i = n; i > 0; --i) m_str.m_data.remove(m_indices[i - 1]);
        m_str.restore_null_terminator();
        return result;
    }

auto StringWhereProxy::pop_unordered() -> String {
        std::size_t n = m_indices.size();
        String result;
        if (n == 0) return result;
        result.reserve(n + 1);
        m_str.strip_null_terminator();
        for (std::size_t i = n; i > 0; --i) {
            std::size_t idx = m_indices[i - 1];
            result += m_str.m_data[idx];
            m_str.m_data.remove_unordered(idx);
        }
        m_str.restore_null_terminator();
        return result;
    }

auto StringWhereProxy::sort() -> StringWhereProxy& {
        std::size_t n = m_indices.size();
        if (n <= 1) return *this;
        DynamicArray<char> tmp;
        tmp.reserve(n);
        for (std::size_t i = 0; i < n; ++i) tmp.push_back(m_str.m_data[m_indices[i]]);
        std::sort(tmp.data(), tmp.data() + n);
        for (std::size_t i = 0; i < n; ++i) m_str.m_data[m_indices[i]] = tmp[i];
        return *this;
    }

auto StringWhereProxy::reverse() -> StringWhereProxy& {
        std::size_t n = m_indices.size();
        if (n <= 1) return *this;
        std::size_t lo = 0, hi = n - 1;
        while (lo < hi) {
            char tmp = m_str.m_data[m_indices[lo]];
            m_str.m_data[m_indices[lo]] = m_str.m_data[m_indices[hi]];
            m_str.m_data[m_indices[hi]] = tmp;
            ++lo; --hi;
        }
        return *this;
    }

auto StringWhereProxy::shuffle() -> StringWhereProxy& {
        std::size_t n = m_indices.size();
        if (n <= 1) return *this;
        for (std::size_t i = n - 1; i > 0; --i) {
            std::size_t j = random_int<std::size_t>(0, i);
            char tmp = m_str.m_data[m_indices[i]];
            m_str.m_data[m_indices[i]] = m_str.m_data[m_indices[j]];
            m_str.m_data[m_indices[j]] = tmp;
        }
        return *this;
    }

auto StringWhereProxy::to_upper() -> StringWhereProxy& {
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            char& c = m_str.m_data[m_indices[i]];
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return *this;
    }

auto StringWhereProxy::to_lower() -> StringWhereProxy& {
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            char& c = m_str.m_data[m_indices[i]];
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return *this;
    }

auto StringWhereProxy::uppercased() const -> String {
        String r(m_str);
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            r.m_data[m_indices[i]] = static_cast<char>(std::toupper(static_cast<unsigned char>(r.m_data[m_indices[i]])));
        }
        return r;
    }

auto StringWhereProxy::lowercased() const -> String {
        String r(m_str);
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            r.m_data[m_indices[i]] = static_cast<char>(std::tolower(static_cast<unsigned char>(r.m_data[m_indices[i]])));
        }
        return r;
    }

auto StringWhereProxy::copy_to(String& dest) const -> void {
        dest.strip_null_terminator();
        for (std::size_t i = 0; i < m_indices.size(); ++i) dest.m_data.push_back(m_str.m_data[m_indices[i]]);
        dest.restore_null_terminator();
    }

auto StringWhereProxy::move_to(String& dest) -> void {
        dest.strip_null_terminator();
        for (std::size_t i = 0; i < m_indices.size(); ++i) dest.m_data.push_back(m_str.m_data[m_indices[i]]);
        dest.restore_null_terminator();
        m_str.strip_null_terminator();
        for (std::size_t i = m_indices.size(); i > 0; --i) m_str.m_data.remove(m_indices[i - 1]);
        m_str.restore_null_terminator();
    }

auto StringWhereProxy::insert_before(char value) -> void {
        if (m_indices.empty()) return;
        m_str.strip_null_terminator();
        for (std::size_t i = m_indices.size(); i > 0; --i) {
            std::size_t idx = m_indices[i - 1] + (i - 1);
            m_str.m_data.insert(idx, value);
        }
        m_str.restore_null_terminator();
    }

auto StringWhereProxy::insert_after(char value) -> void {
        if (m_indices.empty()) return;
        m_str.strip_null_terminator();
        for (std::size_t i = m_indices.size(); i > 0; --i) {
            std::size_t idx = m_indices[i - 1] + i;
            m_str.m_data.insert(idx, value);
        }
        m_str.restore_null_terminator();
    }

auto StringWhereProxy::swap_with(char value) -> String {
        String result;
        result.reserve(m_indices.size() + 1);
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            result += m_str.m_data[m_indices[i]];
            m_str.m_data[m_indices[i]] = value;
        }
        return result;
    }

} // namespace fizmo
