#ifndef FIZMO_STRING_VIEW_HPP
#define FIZMO_STRING_VIEW_HPP

#include "../Arrays/common/traits.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>

namespace fizmo {

class String;

struct StringView {
private:
    const char* m_data;
    std::size_t m_size;

    static constexpr bool ascii_space(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    }

    static constexpr std::size_t length_of(const char* str) noexcept {
        if (!str) return 0;
        std::size_t len = 0;
        while (str[len]) ++len;
        return len;
    }

    static constexpr bool in_set(char c, StringView set) noexcept {
        for (std::size_t i = 0; i < set.m_size; ++i) {
            if (set.m_data[i] == c) return true;
        }
        return false;
    }

public:
    using value_type             = char;
    using size_type              = std::size_t;
    using const_iterator         = const char*;
    using iterator               = const char*;
    using const_reverse_iterator = std::reverse_iterator<const char*>;

    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    constexpr StringView() noexcept : m_data(""), m_size(0) {}
    constexpr StringView(const char* data, std::size_t size) noexcept : m_data(data ? data : ""), m_size(data ? size : 0) {}
    constexpr StringView(const char* str) noexcept : m_data(str ? str : ""), m_size(length_of(str)) {}
    constexpr StringView(std::string_view sv) noexcept : m_data(sv.data() ? sv.data() : ""), m_size(sv.size()) {}
    StringView(const std::string& s) noexcept : m_data(s.data()), m_size(s.size()) {}

    constexpr std::size_t size() const noexcept { return m_size; }
    constexpr std::size_t length() const noexcept { return m_size; }
    constexpr bool empty() const noexcept { return m_size == 0; }
    constexpr const char* data() const noexcept { return m_data; }
    constexpr const char& operator[](std::size_t index) const noexcept { return m_data[index]; }
    constexpr const char* at(std::size_t index) const noexcept { return index < m_size ? &m_data[index] : nullptr; }
    constexpr const char& front() const noexcept { return m_data[0]; }
    constexpr const char& back() const noexcept { return m_data[m_size - 1]; }

    constexpr const char* begin() const noexcept { return m_data; }
    constexpr const char* end() const noexcept { return m_data + m_size; }
    constexpr const char* cbegin() const noexcept { return m_data; }
    constexpr const char* cend() const noexcept { return m_data + m_size; }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

    constexpr StringView substring(std::size_t pos, std::size_t count = npos) const noexcept {
        if (pos >= m_size) return StringView(m_data + m_size, 0);
        const std::size_t len = (count > m_size - pos) ? (m_size - pos) : count;
        return StringView(m_data + pos, len);
    }

    constexpr StringView substr(std::size_t pos, std::size_t count = npos) const noexcept { return substring(pos, count); }

    constexpr StringView first(std::size_t n) const noexcept { return substring(0, n); }
    constexpr StringView last(std::size_t n) const noexcept { return n >= m_size ? *this : StringView(m_data + m_size - n, n); }

    constexpr void remove_prefix(std::size_t n) noexcept {
        if (n > m_size) n = m_size;
        m_data += n;
        m_size -= n;
    }

    constexpr void remove_suffix(std::size_t n) noexcept {
        if (n > m_size) n = m_size;
        m_size -= n;
    }

    constexpr int compare(StringView other) const noexcept {
        const std::size_t len = m_size < other.m_size ? m_size : other.m_size;
        for (std::size_t i = 0; i < len; ++i) {
            const unsigned char a = static_cast<unsigned char>(m_data[i]);
            const unsigned char b = static_cast<unsigned char>(other.m_data[i]);
            if (a < b) return -1;
            if (a > b) return 1;
        }
        if (m_size < other.m_size) return -1;
        if (m_size > other.m_size) return 1;
        return 0;
    }

    constexpr bool equals(StringView other) const noexcept {
        if (m_size != other.m_size) return false;
        for (std::size_t i = 0; i < m_size; ++i) {
            if (m_data[i] != other.m_data[i]) return false;
        }
        return true;
    }

    constexpr bool starts_with(char ch) const noexcept { return m_size > 0 && m_data[0] == ch; }
    constexpr bool starts_with(StringView sv) const noexcept { return sv.m_size <= m_size && StringView(m_data, sv.m_size).equals(sv); }
    constexpr bool starts_with(const char* s) const noexcept { return starts_with(StringView(s)); }
    constexpr bool ends_with(char ch) const noexcept { return m_size > 0 && m_data[m_size - 1] == ch; }
    constexpr bool ends_with(StringView sv) const noexcept { return sv.m_size <= m_size && StringView(m_data + m_size - sv.m_size, sv.m_size).equals(sv); }
    constexpr bool ends_with(const char* s) const noexcept { return ends_with(StringView(s)); }

    constexpr std::size_t find(char ch, std::size_t pos = 0) const noexcept {
        for (std::size_t i = pos; i < m_size; ++i) {
            if (m_data[i] == ch) return i;
        }
        return npos;
    }

    constexpr std::size_t find(StringView sv, std::size_t pos = 0) const noexcept {
        if (sv.m_size == 0) return pos <= m_size ? pos : npos;
        if (sv.m_size > m_size) return npos;
        for (std::size_t i = pos; i + sv.m_size <= m_size; ++i) {
            if (m_data[i] == sv.m_data[0] && StringView(m_data + i, sv.m_size).equals(sv)) return i;
        }
        return npos;
    }

    constexpr std::size_t find(const char* s, std::size_t pos = 0) const noexcept { return find(StringView(s), pos); }

    constexpr std::size_t rfind(char ch, std::size_t before = npos) const noexcept {
        const std::size_t lim = before < m_size ? before : m_size;
        for (std::size_t i = lim; i > 0; --i) {
            if (m_data[i - 1] == ch) return i - 1;
        }
        return npos;
    }

    constexpr std::size_t rfind(StringView sv, std::size_t before = npos) const noexcept {
        const std::size_t lim = before < m_size ? before : m_size;
        if (sv.m_size > lim) return npos;
        for (std::size_t i = lim - sv.m_size + 1; i > 0; --i) {
            if (StringView(m_data + i - 1, sv.m_size).equals(sv)) return i - 1;
        }
        return npos;
    }

    constexpr std::size_t rfind(const char* s, std::size_t before = npos) const noexcept { return rfind(StringView(s), before); }

    constexpr std::size_t find_first_of(StringView set, std::size_t pos = 0) const noexcept {
        for (std::size_t i = pos; i < m_size; ++i) {
            if (in_set(m_data[i], set)) return i;
        }
        return npos;
    }

    constexpr std::size_t find_last_of(StringView set, std::size_t before = npos) const noexcept {
        const std::size_t lim = before < m_size ? before : m_size;
        for (std::size_t i = lim; i > 0; --i) {
            if (in_set(m_data[i - 1], set)) return i - 1;
        }
        return npos;
    }

    constexpr std::size_t find_first_not_of(StringView set, std::size_t pos = 0) const noexcept {
        for (std::size_t i = pos; i < m_size; ++i) {
            if (!in_set(m_data[i], set)) return i;
        }
        return npos;
    }

    constexpr std::size_t find_last_not_of(StringView set, std::size_t before = npos) const noexcept {
        const std::size_t lim = before < m_size ? before : m_size;
        for (std::size_t i = lim; i > 0; --i) {
            if (!in_set(m_data[i - 1], set)) return i - 1;
        }
        return npos;
    }

    constexpr bool contains(char ch) const noexcept { return find(ch) != npos; }
    constexpr bool contains(StringView sv) const noexcept { return find(sv) != npos; }
    constexpr bool contains(const char* s) const noexcept { return find(StringView(s)) != npos; }

    constexpr std::size_t count(char ch) const noexcept {
        std::size_t n = 0;
        for (std::size_t i = 0; i < m_size; ++i) {
            if (m_data[i] == ch) ++n;
        }
        return n;
    }

    constexpr StringView ltrimmed() const noexcept {
        std::size_t lo = 0;
        while (lo < m_size && ascii_space(m_data[lo])) ++lo;
        return StringView(m_data + lo, m_size - lo);
    }

    constexpr StringView rtrimmed() const noexcept {
        std::size_t hi = m_size;
        while (hi > 0 && ascii_space(m_data[hi - 1])) --hi;
        return StringView(m_data, hi);
    }

    constexpr StringView trimmed() const noexcept { return ltrimmed().rtrimmed(); }

    constexpr std::size_t hash() const noexcept {
        std::uint64_t h = 14695981039346656037ULL;
        for (std::size_t i = 0; i < m_size; ++i) {
            h ^= static_cast<unsigned char>(m_data[i]);
            h *= 1099511628211ULL;
        }
        return static_cast<std::size_t>(h);
    }

    inline String to_string() const;
    inline String to_owned() const;

    std::string to_std_string() const { return std::string(m_data, m_size); }

    constexpr operator std::string_view() const noexcept { return std::string_view(m_data, m_size); }
    explicit operator std::string() const { return std::string(m_data, m_size); }

    friend constexpr bool operator==(StringView a, StringView b) noexcept { return a.equals(b); }
    friend constexpr bool operator!=(StringView a, StringView b) noexcept { return !a.equals(b); }
    friend constexpr bool operator<(StringView a, StringView b) noexcept { return a.compare(b) < 0; }
    friend constexpr bool operator<=(StringView a, StringView b) noexcept { return a.compare(b) <= 0; }
    friend constexpr bool operator>(StringView a, StringView b) noexcept { return a.compare(b) > 0; }
    friend constexpr bool operator>=(StringView a, StringView b) noexcept { return a.compare(b) >= 0; }

    friend std::ostream& operator<<(std::ostream& os, StringView sv) {
        return os.write(sv.m_data, static_cast<std::streamsize>(sv.m_size));
    }
};

inline namespace literals {
inline namespace string_literals {

constexpr StringView operator""_sv(const char* s, std::size_t n) noexcept { return StringView(s, n); }

} // namespace string_literals
} // namespace literals

} // namespace fizmo

namespace std {
template <>
struct hash<fizmo::StringView> {
    std::size_t operator()(fizmo::StringView s) const noexcept { return s.hash(); }
};
}

#endif // FIZMO_STRING_VIEW_HPP
