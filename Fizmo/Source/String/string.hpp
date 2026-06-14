#ifndef FIZMO_CUSTOM_STRING_CLASS_HPP
#define FIZMO_CUSTOM_STRING_CLASS_HPP

#include "../Arrays/array.hpp"
#include "../Basic/fizmo_defines.hpp"
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <ostream>
#include <istream>

#ifdef CPP17_OR_GREATER
    #include <string_view>
#endif

namespace fizmo {

class String;

struct StringView {
private:
    const char* m_data;
    std::size_t m_size;

public:
    constexpr StringView() noexcept : m_data(nullptr), m_size(0) {}
    constexpr StringView(const char* data, std::size_t size) noexcept : m_data(data), m_size(size) {}
    constexpr StringView(const char* str) noexcept : m_data(str), m_size(const_strlen(str)) {}
    constexpr std::size_t size() const noexcept { return m_size; }
    constexpr std::size_t length() const noexcept { return m_size; }
    constexpr bool empty() const noexcept { return m_size == 0; }
    constexpr const char* data() const noexcept { return m_data; }
    constexpr const char& operator[](std::size_t index) const noexcept { return m_data[index]; }
    constexpr const char* at(std::size_t index) const noexcept { return index < m_size ? &m_data[index] : nullptr; }
    constexpr const char& front() const noexcept { return m_data[0]; }
    constexpr const char& back() const noexcept { return m_data[m_size - 1]; }

    constexpr StringView substring(std::size_t pos, std::size_t count = npos) const noexcept {
        if (pos >= m_size) return StringView(nullptr, 0);
        std::size_t len = (count > m_size - pos) ? (m_size - pos) : count;
        return StringView(m_data + pos, len);
    }

    constexpr void remove_prefix(std::size_t n) noexcept {
        if (n > m_size) n = m_size;
        m_data += n;
        m_size -= n;
    }

    constexpr void remove_suffix(std::size_t n) noexcept {
        if (n > m_size) n = m_size;
        m_size -= n;
    }

    constexpr bool starts_with(char ch) const noexcept { return m_size > 0 && m_data[0] == ch; }

    constexpr bool starts_with(StringView sv) const noexcept {
        if (sv.m_size > m_size) return false;
        for (std::size_t i = 0; i < sv.m_size; ++i) { if (m_data[i] != sv.m_data[i]) return false; }
        return true;
    }

    constexpr bool ends_with(char ch) const noexcept { return m_size > 0 && m_data[m_size - 1] == ch; }

    constexpr bool ends_with(StringView sv) const noexcept {
        if (sv.m_size > m_size) return false;
        std::size_t off = m_size - sv.m_size;
        for (std::size_t i = 0; i < sv.m_size; ++i) { if (m_data[off + i] != sv.m_data[i]) return false; }
        return true;
    }

    constexpr std::size_t find(char ch, std::size_t pos = 0) const noexcept {
        for (std::size_t i = pos; i < m_size; ++i) { if (m_data[i] == ch) return i; }
        return npos;
    }

    constexpr std::size_t find(StringView sv, std::size_t pos = 0) const noexcept {
        if (sv.m_size == 0) return pos <= m_size ? pos : npos;
        if (sv.m_size > m_size) return npos;
        for (std::size_t i = pos; i <= m_size - sv.m_size; ++i) {
            bool match = true;
            for (std::size_t j = 0; j < sv.m_size; ++j) {
                if (m_data[i + j] != sv.m_data[j]) { match = false; break; }
            }
            if (match) return i;
        }
        return npos;
    }

    constexpr std::size_t rfind(char ch) const noexcept {
        for (std::size_t i = m_size; i > 0; --i) { if (m_data[i - 1] == ch) return i - 1; }
        return npos;
    }

    constexpr bool contains(char ch) const noexcept { return find(ch) != npos; }
    constexpr bool contains(StringView sv) const noexcept { return find(sv) != npos; }

    constexpr int compare(StringView other) const noexcept {
        std::size_t len = m_size < other.m_size ? m_size : other.m_size;
        for (std::size_t i = 0; i < len; ++i) {
            if (m_data[i] < other.m_data[i]) return -1;
            if (m_data[i] > other.m_data[i]) return 1;
        }
        if (m_size < other.m_size) return -1;
        if (m_size > other.m_size) return 1;
        return 0;
    }

    constexpr StringView ltrimmed() const noexcept {
        std::size_t lo = 0;
        while (lo < m_size && sv_is_whitespace(m_data[lo])) ++lo;
        return StringView(m_data + lo, m_size - lo);
    }

    constexpr StringView rtrimmed() const noexcept {
        std::size_t hi = m_size;
        while (hi > 0 && sv_is_whitespace(m_data[hi - 1])) --hi;
        return StringView(m_data, hi);
    }

    constexpr StringView trimmed() const noexcept {
        std::size_t lo = 0, hi = m_size;
        while (lo < hi && sv_is_whitespace(m_data[lo])) ++lo;
        while (hi > lo && sv_is_whitespace(m_data[hi - 1])) --hi;
        return StringView(m_data + lo, hi - lo);
    }

    inline String to_string() const;
    inline String to_owned() const;

    constexpr ConstArrayIterator<char> begin() const noexcept { return ConstArrayIterator<char>(m_data); }
    constexpr ConstArrayIterator<char> end() const noexcept { return ConstArrayIterator<char>(m_data + m_size); }

    friend constexpr bool operator==(StringView a, StringView b) noexcept { return a.compare(b) == 0; }
    friend constexpr bool operator!=(StringView a, StringView b) noexcept { return a.compare(b) != 0; }
    friend constexpr bool operator<(StringView a, StringView b) noexcept { return a.compare(b) < 0; }
    friend constexpr bool operator<=(StringView a, StringView b) noexcept { return a.compare(b) <= 0; }
    friend constexpr bool operator>(StringView a, StringView b) noexcept { return a.compare(b) > 0; }
    friend constexpr bool operator>=(StringView a, StringView b) noexcept { return a.compare(b) >= 0; }

#ifdef CPP17_OR_GREATER
    constexpr StringView(std::string_view sv) noexcept : m_data(sv.data()), m_size(sv.size()) {}
    constexpr operator std::string_view() const noexcept { return std::string_view(m_data, m_size); }
#endif

private:
    static constexpr bool sv_is_whitespace(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    }

    static constexpr std::size_t const_strlen(const char* str) noexcept {
        if (!str) return 0;
        std::size_t len = 0;
        while (str[len]) ++len;
        return len;
    }
};

class String;
class StringWhereProxy;

class String {
private:
    DynamicArray<char> m_data;
    bool m_null_present;
    friend class StringWhereProxy;

    static char impl_to_upper(char c) noexcept { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }
    static char impl_to_lower(char c) noexcept { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

    static std::size_t c_strlen(const char* str) noexcept {
        if (!str) return 0;
        std::size_t len = 0;
        while (str[len]) ++len;
        return len;
    }

    void assign_raw(const char* str, std::size_t len) {
        m_data.clear();
        m_null_present = false;
        if (str && len > 0) {
            m_data.reserve(len + 1);
            for (std::size_t i = 0; i < len; ++i) { m_data.push_back(str[i]); }
            m_data.push_back('\0');
            m_null_present = true;
        }
    }

    void append_raw(const char* str, std::size_t len) {
        if (!str || len == 0) return;
        if (m_null_present) { m_data.remove_back(); m_null_present = false; }
        m_data.ensure_capacity(m_data.size() + len + 1);
        for (std::size_t i = 0; i < len; ++i) { m_data.push_back(str[i]); }
        m_data.push_back('\0');
        m_null_present = true;
    }

    static bool str_equal(const char* a, std::size_t a_len, const char* b, std::size_t b_len) noexcept {
        if (a_len != b_len) return false;
        for (std::size_t i = 0; i < a_len; ++i) { if (a[i] != b[i]) return false; }
        return true;
    }

    static int str_compare(const char* a, std::size_t a_len, const char* b, std::size_t b_len) noexcept {
        std::size_t len = a_len < b_len ? a_len : b_len;
        for (std::size_t i = 0; i < len; ++i) {
            if (a[i] < b[i]) return -1;
            if (a[i] > b[i]) return 1;
        }
        if (a_len < b_len) return -1;
        if (a_len > b_len) return 1;
        return 0;
    }

    void strip_null_terminator() noexcept {
        if (m_null_present) { m_data.remove_back(); m_null_present = false; }
    }

    void restore_null_terminator() {
        if (!m_null_present) { m_data.push_back('\0'); m_null_present = true; }
    }

    DynamicArray<std::size_t> collect_char_indices(char ch) const {
        DynamicArray<std::size_t> idx;
        for (std::size_t i = 0; i < size(); ++i) { if (m_data[i] == ch) idx.push_back(i); }
        return idx;
    }

    template <typename Pred>
    DynamicArray<std::size_t> collect_char_indices_if(Pred pred) const {
        DynamicArray<std::size_t> idx;
        for (std::size_t i = 0; i < size(); ++i) { if (pred(m_data[i])) idx.push_back(i); }
        return idx;
    }

    static bool glob_match(const char* s, std::size_t slen, const char* p, std::size_t plen) noexcept {
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

public:
    constexpr String() noexcept : m_data(), m_null_present(false) {}
    String(const String& other) : m_data(other.m_data), m_null_present(other.m_null_present) {}
    String(String&& other) noexcept : m_data(std::move(other.m_data)), m_null_present(other.m_null_present) { other.m_null_present = false; }
    String(const char* str) : m_data(), m_null_present(false) { assign_raw(str, c_strlen(str)); }
    String(const char* str, std::size_t len) : m_data(), m_null_present(false) { assign_raw(str, len); }
    String(char* str) : String(static_cast<const char*>(str)) {}
    String(StringView sv) : m_data(), m_null_present(false) { assign_raw(sv.data(), sv.size()); }
    String(const std::string& str) : m_data(), m_null_present(false) { assign_raw(str.data(), str.size()); }
    String(std::string&& str) : m_data(), m_null_present(false) { assign_raw(str.data(), str.size()); }

    explicit String(char ch) : m_data(), m_null_present(false) {
        m_data.reserve(2);
        m_data.push_back(ch);
        m_data.push_back('\0');
        m_null_present = true;
    }

    String(std::size_t count, char ch) : m_data(), m_null_present(false) {
        m_data.reserve(count + 1);
        for (std::size_t i = 0; i < count; ++i) { m_data.push_back(ch); }
        m_data.push_back('\0');
        m_null_present = true;
    }

#ifdef CPP17_OR_GREATER
    String(std::string_view sv) : m_data(), m_null_present(false) { assign_raw(sv.data(), sv.size()); }
#endif

public:
    String& operator=(const String& other) {
        if (this != &other) { m_data = other.m_data; m_null_present = other.m_null_present; }
        return *this;
    }

    String& operator=(String&& other) noexcept {
        if (this != &other) { m_data = std::move(other.m_data); m_null_present = other.m_null_present; other.m_null_present = false; }
        return *this;
    }

    String& operator=(const char* str) { assign_raw(str, c_strlen(str)); return *this; }
    String& operator=(char* str)       { return *this = static_cast<const char*>(str); }
    String& operator=(StringView sv)   { assign_raw(sv.data(), sv.size()); return *this; }
    String& operator=(const std::string& str) { assign_raw(str.data(), str.size()); return *this; }
    String& operator=(std::string&& str)      { assign_raw(str.data(), str.size()); return *this; }

    String& operator=(char ch) {
        m_data.clear();
        m_null_present = false;
        m_data.reserve(2);
        m_data.push_back(ch);
        m_data.push_back('\0');
        m_null_present = true;
        return *this;
    }

#ifdef CPP17_OR_GREATER
    String& operator=(std::string_view sv) { assign_raw(sv.data(), sv.size()); return *this; }
#endif

public:
    std::size_t size() const noexcept { return m_null_present ? m_data.size() - 1 : m_data.size(); }
    std::size_t length() const noexcept { return size(); }
    std::size_t capacity() const noexcept { return m_data.capacity(); }
    bool empty() const noexcept { return size() == 0; }
    DynamicArray<char>& data() noexcept { return m_data; }
    const DynamicArray<char>& data() const noexcept { return m_data; }
    char* c_str() noexcept { return m_data.data(); }
    const char* c_str() const noexcept { return m_data.data(); }
    char& operator[](std::size_t index) noexcept { return m_data[index]; }
    const char& operator[](std::size_t index) const noexcept { return m_data[index]; }
    char* at(std::size_t index) noexcept { return index < size() ? &m_data[index] : nullptr; }
    const char* at(std::size_t index) const noexcept { return index < size() ? &m_data[index] : nullptr; }
    char& front() noexcept { return m_data[0]; }
    const char& front() const noexcept { return m_data[0]; }
    char& back() noexcept { return m_data[size() - 1]; }
    const char& back() const noexcept { return m_data[size() - 1]; }

public:
    void reserve(std::size_t new_capacity) { m_data.reserve(new_capacity); }
    void shrink() { m_data.shrink(); }
    void clear() noexcept { m_data.clear(); m_null_present = false; }

    void resize(std::size_t new_size) {
        if (new_size == 0) { m_data.clear(); m_null_present = false; return; }
        std::size_t old_size = size();
        m_data.resize(new_size + 1);
        for (std::size_t i = old_size; i < new_size; ++i) { m_data[i] = '\0'; }
        m_data[new_size] = '\0';
        m_null_present = true;
    }

    void resize(std::size_t new_size, char ch) {
        if (new_size == 0) { m_data.clear(); m_null_present = false; return; }
        std::size_t old_size = size();
        m_data.resize(new_size + 1);
        for (std::size_t i = old_size; i < new_size; ++i) { m_data[i] = ch; }
        m_data[new_size] = '\0';
        m_null_present = true;
    }

    void truncate(std::size_t n) {
        if (n >= size()) return;
        m_data.resize(n + 1);
        m_data[n] = '\0';
        m_null_present = true;
    }

    void swap(String& other) noexcept {
        std::swap(m_data, other.m_data);
        std::swap(m_null_present, other.m_null_present);
    }

public:
    bool has_null_terminator() const noexcept { return m_null_present; }
    void trim_terminator() noexcept { strip_null_terminator(); }
    void ensure_terminator() { restore_null_terminator(); }

public:
    ArrayIterator<char> begin() noexcept { return m_data.begin(); }
    ArrayIterator<char> end() noexcept { return ArrayIterator<char>(m_data.data() + size()); }
    ConstArrayIterator<char> begin() const noexcept { return m_data.begin(); }
    ConstArrayIterator<char> end() const noexcept { return ConstArrayIterator<char>(m_data.data() + size()); }
    ConstArrayIterator<char> cbegin() const noexcept { return m_data.cbegin(); }
    ConstArrayIterator<char> cend() const noexcept { return ConstArrayIterator<char>(m_data.data() + size()); }
    ReverseArrayIterator<char> rbegin() noexcept { return ReverseArrayIterator<char>(m_data.data() + size()); }
    ReverseArrayIterator<char> rend() noexcept { return m_data.rend(); }
    ConstReverseArrayIterator<char> rbegin() const noexcept { return ConstReverseArrayIterator<char>(m_data.data() + size()); }
    ConstReverseArrayIterator<char> rend() const noexcept { return m_data.rend(); }
    ConstReverseArrayIterator<char> crbegin() const noexcept { return ConstReverseArrayIterator<char>(m_data.data() + size()); }
    ConstReverseArrayIterator<char> crend() const noexcept { return m_data.crend(); }

public:
    StringView view() const noexcept { return StringView(c_str(), size()); }

    StringView view(std::size_t start, std::size_t count = npos) const noexcept {
        if (start >= size()) return StringView(nullptr, 0);
        std::size_t remaining = size() - start;
        std::size_t len = count < remaining ? count : remaining;
        return StringView(c_str() + start, len);
    }

    operator StringView() const noexcept { return view(); }

public:
    int compare(const String& other) const noexcept { return str_compare(c_str(), size(), other.c_str(), other.size()); }
    int compare(const char* str) const noexcept { return str_compare(c_str(), size(), str, c_strlen(str)); }
    int compare(StringView sv) const noexcept { return str_compare(c_str(), size(), sv.data(), sv.size()); }
    int compare(const std::string& str) const noexcept { return str_compare(c_str(), size(), str.c_str(), str.size()); }

    int compare_ignore_case(StringView sv) const noexcept {
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

    bool equals_ignore_case(StringView sv) const noexcept { return compare_ignore_case(sv) == 0; }

    friend bool operator==(const String& lhs, const String& rhs) noexcept { return str_equal(lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size()); }
    friend bool operator==(const String& lhs, const char* rhs) noexcept { return str_equal(lhs.c_str(), lhs.size(), rhs, c_strlen(rhs)); }
    friend bool operator==(const char* lhs, const String& rhs) noexcept { return str_equal(lhs, c_strlen(lhs), rhs.c_str(), rhs.size()); }
    friend bool operator==(const String& lhs, StringView rhs) noexcept { return str_equal(lhs.c_str(), lhs.size(), rhs.data(), rhs.size()); }
    friend bool operator==(StringView lhs, const String& rhs) noexcept { return str_equal(lhs.data(), lhs.size(), rhs.c_str(), rhs.size()); }
    friend bool operator==(const String& lhs, const std::string& rhs) noexcept { return str_equal(lhs.c_str(), lhs.size(), rhs.data(), rhs.size()); }
    friend bool operator==(const std::string& lhs, const String& rhs) noexcept { return str_equal(lhs.data(), lhs.size(), rhs.c_str(), rhs.size()); }

    friend bool operator!=(const String& lhs, const String& rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(const String& lhs, const char* rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(const char* lhs, const String& rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(const String& lhs, StringView rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(StringView lhs, const String& rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(const String& lhs, const std::string& rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(const std::string& lhs, const String& rhs) noexcept { return !(lhs == rhs); }

    friend bool operator<(const String& lhs, const String& rhs) noexcept { return lhs.compare(rhs) < 0; }
    friend bool operator<=(const String& lhs, const String& rhs) noexcept { return lhs.compare(rhs) <= 0; }
    friend bool operator>(const String& lhs, const String& rhs) noexcept { return lhs.compare(rhs) > 0; }
    friend bool operator>=(const String& lhs, const String& rhs) noexcept { return lhs.compare(rhs) >= 0; }

    friend std::ostream& operator<<(std::ostream& os, const String& str) {
        if (!str.empty()) os.write(str.c_str(), static_cast<std::streamsize>(str.size()));
        return os;
    }

public:
    String& operator+=(const String& other) { append_raw(other.c_str(), other.size()); return *this; }
    String& operator+=(const char* str) { append_raw(str, c_strlen(str)); return *this; }
    String& operator+=(StringView sv) { append_raw(sv.data(), sv.size()); return *this; }
    String& operator+=(const std::string& str) { append_raw(str.data(), str.size()); return *this; }

    String& operator+=(char ch) {
        if (m_null_present) { m_data.remove_back(); m_null_present = false; }
        m_data.push_back(ch);
        m_data.push_back('\0');
        m_null_present = true;
        return *this;
    }

    friend String operator+(const String& lhs, const String& rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(const String& lhs, const char* rhs)   { String r(lhs); r += rhs; return r; }
    friend String operator+(const char* lhs, const String& rhs)   { String r(lhs); r += rhs; return r; }
    friend String operator+(const String& lhs, StringView rhs)    { String r(lhs); r += rhs; return r; }
    friend String operator+(StringView lhs, const String& rhs)    { String r(lhs); r += rhs; return r; }
    friend String operator+(const String& lhs, const std::string& rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(const std::string& lhs, const String& rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(const String& lhs, char rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(char lhs, const String& rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(String&& lhs, const String& rhs)      { lhs += rhs; return std::move(lhs); }
    friend String operator+(String&& lhs, const char* rhs)        { lhs += rhs; return std::move(lhs); }
    friend String operator+(String&& lhs, StringView rhs)         { lhs += rhs; return std::move(lhs); }
    friend String operator+(String&& lhs, const std::string& rhs) { lhs += rhs; return std::move(lhs); }
    friend String operator+(String&& lhs, char rhs)               { lhs += rhs; return std::move(lhs); }

    friend String operator*(const String& s, std::size_t n) { return s.repeated(n); }
    friend String operator*(std::size_t n, const String& s) { return s.repeated(n); }

#ifdef CPP17_OR_GREATER
    int compare(std::string_view sv) const noexcept { return str_compare(c_str(), size(), sv.data(), sv.size()); }
    friend bool operator==(const String& lhs, std::string_view rhs) noexcept { return str_equal(lhs.c_str(), lhs.size(), rhs.data(), rhs.size()); }
    friend bool operator==(std::string_view lhs, const String& rhs) noexcept { return str_equal(lhs.data(), lhs.size(), rhs.c_str(), rhs.size()); }
    friend bool operator!=(const String& lhs, std::string_view rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(std::string_view lhs, const String& rhs) noexcept { return !(lhs == rhs); }
    String& operator+=(std::string_view sv) { append_raw(sv.data(), sv.size()); return *this; }
    friend String operator+(const String& lhs, std::string_view rhs)  { String r(lhs); r += rhs; return r; }
    friend String operator+(std::string_view lhs, const String& rhs)  { String r(lhs); r += rhs; return r; }
    friend String operator+(String&& lhs, std::string_view rhs)       { lhs += rhs; return std::move(lhs); }
#endif

public:
    explicit operator std::string() const { return std::string(c_str(), size()); }
    explicit operator bool()        const noexcept { return !empty(); }
    explicit operator const char*() const noexcept { return c_str(); }
    explicit operator int()                const noexcept { return as_int<int>(); }
    explicit operator long()               const noexcept { return as_int<long>(); }
    explicit operator long long()          const noexcept { return as_int<long long>(); }
    explicit operator unsigned int()       const noexcept { return as_int<unsigned int>(); }
    explicit operator unsigned long()      const noexcept { return as_int<unsigned long>(); }
    explicit operator unsigned long long() const noexcept { return as_int<unsigned long long>(); }

#ifdef CPP17_OR_GREATER
    explicit operator std::string_view() const noexcept { return std::string_view(c_str(), size()); }
#endif

public:
    static constexpr bool is_whitespace(char c) noexcept { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type> static constexpr T get_int(char ch) noexcept { return (ch >= '0' && ch <= '9') ? static_cast<T>(ch - '0') : T{}; }

    template <typename T>
    static T get_int(StringView sv) noexcept {
        const char* p = sv.data();
        std::size_t len = sv.size();
        if (!p || len == 0) return T{};
        std::size_t i = 0;
        while (i < len && is_whitespace(p[i])) ++i;
        bool negative = false;

        if (i < len && (p[i] == '-' || p[i] == '+')) {
            negative = (p[i] == '-');
            ++i;
        }

        T result = T{};

        while (i < len && p[i] >= '0' && p[i] <= '9') {
            result = result * static_cast<T>(10) + static_cast<T>(p[i] - '0');
            ++i;
        }

        return negative ? -result : result;
    }

    template <typename T = int>
    T as_int() const noexcept { return get_int<T>(view()); }

    float  to_float()  const noexcept { return static_cast<float>(std::atof(c_str())); }
    double to_double() const noexcept { return std::atof(c_str()); }

    bool to_bool() const noexcept {
        StringView v = view().trimmed();
        if (v == StringView("true")  || v == StringView("1") || v == StringView("yes") || v == StringView("on"))  return true;
        if (v == StringView("false") || v == StringView("0") || v == StringView("no")  || v == StringView("off")) return false;
        return !empty();
    }

    bool is_integer() const noexcept {
        if (empty()) return false;
        std::size_t i = 0;
        if (m_data[i] == '-' || m_data[i] == '+') ++i;
        if (i == size()) return false;
        for (; i < size(); ++i) { if (!std::isdigit(static_cast<unsigned char>(m_data[i]))) return false; }
        return true;
    }

    bool is_float() const noexcept {
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

    static String from_int(long long v) { return String(std::to_string(v)); }

    static String from_float(float v, int prec = 6) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*f", prec, static_cast<double>(v));
        return String(buf);
    }

    static String from_double(double v, int prec = 6) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*f", prec, v);
        return String(buf);
    }

    static String from_bool(bool v) { return String(v ? "true" : "false"); }

    static String format(const char* fmt, ...) {
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

    static String from_stream(std::istream& is) {
        String result;
        char buf[4096];
        while (is.read(buf, sizeof(buf))) {
            result.append_raw(buf, static_cast<std::size_t>(is.gcount()));
        }
        if (is.gcount() > 0) result.append_raw(buf, static_cast<std::size_t>(is.gcount()));
        return result;
    }

    void write_to(std::ostream& os) const {
        if (!empty()) os.write(c_str(), static_cast<std::streamsize>(size()));
    }

    String to_hex() const {
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

    static String from_hex(StringView sv) {
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

    String to_base64() const {
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

    static String from_base64(StringView sv) {
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

    String escape(StringView chars) const {
        String result;
        result.reserve(size() + 1);
        for (std::size_t i = 0; i < size(); ++i) {
            if (m_data[i] == '\\' || chars.contains(m_data[i])) result += '\\';
            result += m_data[i];
        }
        return result;
    }

    String unescape() const {
        String result;
        result.reserve(size() + 1);
        for (std::size_t i = 0; i < size(); ++i) {
            if (m_data[i] == '\\' && i + 1 < size()) { ++i; }
            result += m_data[i];
        }
        return result;
    }

    String url_encode() const {
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

    String url_decode() const {
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

    std::size_t hash() const noexcept {
        std::size_t h = 14695981039346656037ULL;
        for (std::size_t i = 0; i < size(); ++i) {
            h ^= static_cast<unsigned char>(m_data[i]);
            h *= 1099511628211ULL;
        }
        return h;
    }

public:
    String substring(std::size_t pos, std::size_t count = npos) const {
        if (pos >= size()) return String();
        std::size_t len = (count > size() - pos) ? (size() - pos) : count;
        return String(c_str() + pos, len);
    }

    String substr(std::size_t pos, std::size_t count = npos) const { return substring(pos, count); }

public:
    String& append(const String& s)   { return *this += s; }
    String& append(const char* s)     { return *this += s; }
    String& append(StringView sv)     { return *this += sv; }
    String& append(char ch)           { return *this += ch; }

    String& append(std::size_t n, char ch) {
        if (m_null_present) { m_data.remove_back(); m_null_present = false; }
        m_data.reserve(m_data.size() + n + 1);
        for (std::size_t i = 0; i < n; ++i) m_data.push_back(ch);
        m_data.push_back('\0');
        m_null_present = true;
        return *this;
    }

    String& prepend(const String& s)  { return insert(0, s); }
    String& prepend(const char* s)    { return insert(0, s); }
    String& prepend(StringView sv)    { return insert(0, sv); }
    String& prepend(char ch)          { return insert(0, ch); }

public:
    String& insert(std::size_t idx, char ch) {
        if (idx > size()) idx = size();
        strip_null_terminator();
        m_data.insert(idx, ch);
        restore_null_terminator();
        return *this;
    }

    String& insert(std::size_t idx, const char* str, std::size_t len) {
        if (len == 0) return *this;
        if (idx > size()) idx = size();
        strip_null_terminator();
        m_data.insert(idx, str, len);
        restore_null_terminator();
        return *this;
    }

    String& insert(std::size_t idx, const char* str) { return insert(idx, str, c_strlen(str)); }
    String& insert(std::size_t idx, StringView sv)   { return insert(idx, sv.data(), sv.size()); }
    String& insert(std::size_t idx, const String& s) { return insert(idx, s.c_str(), s.size()); }

    String& erase(std::size_t pos, std::size_t count = npos) {
        if (pos >= size()) return *this;
        std::size_t n = (count > size() - pos) ? (size() - pos) : count;
        strip_null_terminator();
        for (std::size_t i = pos; i + n < m_data.size(); ++i) m_data[i] = m_data[i + n];
        m_data.resize(m_data.size() - n);
        restore_null_terminator();
        return *this;
    }

public:
    std::size_t find(char ch, std::size_t pos = 0) const noexcept {
        for (std::size_t i = pos; i < size(); ++i) { if (m_data[i] == ch) return i; }
        return npos;
    }

    std::size_t find(StringView sv, std::size_t pos = 0) const noexcept {
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

    std::size_t find(const char* s, std::size_t pos = 0) const noexcept { return find(StringView(s), pos); }

    template <typename Pred>
    std::size_t find_if(Pred pred, std::size_t pos = 0) const noexcept {
        for (std::size_t i = pos; i < size(); ++i) { if (pred(m_data[i])) return i; }
        return npos;
    }

    std::size_t find_last(char ch, std::size_t before = npos) const noexcept {
        std::size_t lim = (before < size()) ? before : size();
        for (std::size_t i = lim; i > 0; --i) { if (m_data[i - 1] == ch) return i - 1; }
        return npos;
    }

    template <typename Pred>
    std::size_t find_last_if(Pred pred, std::size_t before = npos) const noexcept {
        std::size_t lim = (before < size()) ? before : size();
        for (std::size_t i = lim; i > 0; --i) { if (pred(m_data[i - 1])) return i - 1; }
        return npos;
    }

    std::size_t rfind(char ch, std::size_t before = npos) const noexcept { return find_last(ch, before); }

    std::size_t rfind(StringView sv, std::size_t before = npos) const noexcept {
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

    std::size_t rfind(const char* s, std::size_t before = npos) const noexcept { return rfind(StringView(s), before); }

    DynamicArray<std::size_t> find_all(StringView sv) const {
        DynamicArray<std::size_t> result;
        if (sv.empty() || sv.size() > size()) return result;
        std::size_t pos = 0;
        while ((pos = find(sv, pos)) != npos) { result.push_back(pos); pos += sv.size(); }
        return result;
    }

    DynamicArray<std::size_t> find_all(char ch) const { return collect_char_indices(ch); }

    std::size_t find_nth(char ch, std::size_t n) const noexcept {
        if (n == 0) return npos;
        std::size_t count = 0;
        for (std::size_t i = 0; i < size(); ++i) {
            if (m_data[i] == ch && ++count == n) return i;
        }
        return npos;
    }

    std::size_t find_nth(StringView sv, std::size_t n) const noexcept {
        if (sv.empty() || n == 0) return npos;
        std::size_t count = 0, pos = 0;
        while ((pos = find(sv, pos)) != npos) {
            if (++count == n) return pos;
            pos += sv.size();
        }
        return npos;
    }

    String find_between(StringView open, StringView close) const {
        std::size_t s = find(open);
        if (s == npos) return String();
        s += open.size();
        std::size_t e = find(close, s);
        if (e == npos) return String();
        return substring(s, e - s);
    }

    bool matches_glob(StringView pattern) const noexcept {
        return glob_match(c_str(), size(), pattern.data(), pattern.size());
    }

    std::size_t find_first_of(StringView charset, std::size_t pos = 0) const noexcept {
        for (std::size_t i = pos; i < size(); ++i) {
            for (std::size_t j = 0; j < charset.size(); ++j) {
                if (m_data[i] == charset[j]) return i;
            }
        }
        return npos;
    }

    std::size_t find_last_of(StringView charset, std::size_t before = npos) const noexcept {
        std::size_t lim = (before < size()) ? before : size();
        for (std::size_t i = lim; i > 0; --i) {
            for (std::size_t j = 0; j < charset.size(); ++j) {
                if (m_data[i - 1] == charset[j]) return i - 1;
            }
        }
        return npos;
    }

    std::size_t find_first_not_of(StringView charset, std::size_t pos = 0) const noexcept {
        for (std::size_t i = pos; i < size(); ++i) {
            bool found = false;
            for (std::size_t j = 0; j < charset.size(); ++j) {
                if (m_data[i] == charset[j]) { found = true; break; }
            }
            if (!found) return i;
        }
        return npos;
    }

    std::size_t find_last_not_of(StringView charset, std::size_t before = npos) const noexcept {
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

    DynamicArray<std::size_t> find_indices(char ch) const { return collect_char_indices(ch); }
    template <typename Pred> DynamicArray<std::size_t> find_indices_if(Pred pred) const { return collect_char_indices_if(pred); }

public:
    bool starts_with(char ch)        const noexcept { return !empty() && m_data[0] == ch; }
    bool starts_with(StringView sv)  const noexcept { return view().starts_with(sv); }
    bool starts_with(const char* s)  const noexcept { return starts_with(StringView(s)); }

    bool ends_with(char ch)          const noexcept { return !empty() && m_data[size() - 1] == ch; }
    bool ends_with(StringView sv)    const noexcept { return view().ends_with(sv); }
    bool ends_with(const char* s)    const noexcept { return ends_with(StringView(s)); }

public:
    std::size_t count_of(char ch) const noexcept {
        std::size_t n = 0;
        for (std::size_t i = 0; i < size(); ++i) { if (m_data[i] == ch) ++n; }
        return n;
    }

    std::size_t count_occurrences(StringView sv) const noexcept {
        if (sv.empty() || sv.size() > size()) return 0;
        std::size_t n = 0, pos = 0;
        while ((pos = find(sv, pos)) != npos) { ++n; pos += sv.size(); }
        return n;
    }

    template <typename Pred>
    std::size_t count_if(Pred pred) const noexcept {
        std::size_t n = 0;
        for (std::size_t i = 0; i < size(); ++i) { if (pred(m_data[i])) ++n; }
        return n;
    }

    bool contains(char ch)          const noexcept { return find(ch) != npos; }
    bool contains(StringView sv)    const noexcept { return find(sv) != npos; }
    bool contains(const char* s)    const noexcept { return find(StringView(s)) != npos; }

    template <typename Pred>
    bool contains_if(Pred pred) const noexcept {
        for (std::size_t i = 0; i < size(); ++i) { if (pred(m_data[i])) return true; }
        return false;
    }

    template <typename Pred> bool all_of(Pred pred) const noexcept  { for (std::size_t i = 0; i < size(); ++i) { if (!pred(m_data[i])) return false; } return true; }
    template <typename Pred> bool any_of(Pred pred) const noexcept  { for (std::size_t i = 0; i < size(); ++i) { if ( pred(m_data[i])) return true;  } return false; }
    template <typename Pred> bool none_of(Pred pred) const noexcept { for (std::size_t i = 0; i < size(); ++i) { if ( pred(m_data[i])) return false; } return true; }

    bool is_alpha() const noexcept {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) { if (!std::isalpha(static_cast<unsigned char>(m_data[i]))) return false; }
        return true;
    }

    bool is_digit() const noexcept {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) { if (!std::isdigit(static_cast<unsigned char>(m_data[i]))) return false; }
        return true;
    }

    bool is_alnum() const noexcept {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) { if (!std::isalnum(static_cast<unsigned char>(m_data[i]))) return false; }
        return true;
    }

    bool is_whitespace_only() const noexcept {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) { if (!is_whitespace(m_data[i])) return false; }
        return true;
    }

    bool is_hex() const noexcept {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) {
            char c = m_data[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
        }
        return true;
    }

    bool is_upper() const noexcept {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) {
            if (std::isalpha(static_cast<unsigned char>(m_data[i])) &&
                !std::isupper(static_cast<unsigned char>(m_data[i]))) return false;
        }
        return true;
    }

    bool is_lower() const noexcept {
        if (empty()) return false;
        for (std::size_t i = 0; i < size(); ++i) {
            if (std::isalpha(static_cast<unsigned char>(m_data[i])) &&
                !std::islower(static_cast<unsigned char>(m_data[i]))) return false;
        }
        return true;
    }

    bool is_palindrome() const noexcept {
        if (size() <= 1) return true;
        std::size_t lo = 0, hi = size() - 1;
        while (lo < hi) { if (m_data[lo++] != m_data[hi--]) return false; }
        return true;
    }

    bool is_sorted() const noexcept {
        for (std::size_t i = 1; i < size(); ++i) { if (m_data[i] < m_data[i - 1]) return false; }
        return true;
    }

    template <typename Compare>
    bool is_sorted(Compare comp) const noexcept {
        for (std::size_t i = 1; i < size(); ++i) { if (comp(m_data[i], m_data[i - 1])) return false; }
        return true;
    }

    std::size_t levenshtein_distance(StringView other) const {
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

    StringView common_prefix(StringView other) const noexcept {
        std::size_t n = size() < other.size() ? size() : other.size();
        std::size_t i = 0;
        while (i < n && m_data[i] == other[i]) ++i;
        return StringView(c_str(), i);
    }

    StringView common_suffix(StringView other) const noexcept {
        std::size_t n = size() < other.size() ? size() : other.size();
        std::size_t i = 0;
        while (i < n && m_data[size() - 1 - i] == other[other.size() - 1 - i]) ++i;
        return StringView(c_str() + size() - i, i);
    }

    std::size_t word_count() const noexcept {
        std::size_t n = 0;
        bool in_word = false;
        for (std::size_t i = 0; i < size(); ++i) {
            if (!is_whitespace(m_data[i])) { if (!in_word) { ++n; in_word = true; } }
            else                           { in_word = false; }
        }
        return n;
    }

    char min() const noexcept {
        if (empty()) return '\0';
        char m = m_data[0];
        for (std::size_t i = 1; i < size(); ++i) { if (m_data[i] < m) m = m_data[i]; }
        return m;
    }

    template <typename Compare>
    char min(Compare comp) const noexcept {
        if (empty()) return '\0';
        char m = m_data[0];
        for (std::size_t i = 1; i < size(); ++i) { if (comp(m_data[i], m)) m = m_data[i]; }
        return m;
    }

    char max() const noexcept {
        if (empty()) return '\0';
        char m = m_data[0];
        for (std::size_t i = 1; i < size(); ++i) { if (m_data[i] > m) m = m_data[i]; }
        return m;
    }

    template <typename Compare>
    char max(Compare comp) const noexcept {
        if (empty()) return '\0';
        char m = m_data[0];
        for (std::size_t i = 1; i < size(); ++i) { if (comp(m, m_data[i])) m = m_data[i]; }
        return m;
    }

    std::pair<char, char> min_max() const noexcept {
        if (empty()) return { '\0', '\0' };
        char lo = m_data[0], hi = m_data[0];
        for (std::size_t i = 1; i < size(); ++i) {
            if (m_data[i] < lo) lo = m_data[i];
            if (m_data[i] > hi) hi = m_data[i];
        }
        return { lo, hi };
    }

public:
    void to_upper() { for (std::size_t i = 0; i < size(); ++i) { m_data[i] = impl_to_upper(m_data[i]); } }
    void to_lower() { for (std::size_t i = 0; i < size(); ++i) { m_data[i] = impl_to_lower(m_data[i]); } }
    void to_upper(std::size_t index) { if (index < size()) m_data[index] = impl_to_upper(m_data[index]); }
    void to_lower(std::size_t index) { if (index < size()) m_data[index] = impl_to_lower(m_data[index]); }

    void to_upper(std::size_t start, std::size_t end_idx) {
        if (end_idx > size()) end_idx = size();
        for (std::size_t i = start; i < end_idx; ++i) { m_data[i] = impl_to_upper(m_data[i]); }
    }

    void to_lower(std::size_t start, std::size_t end_idx) {
        if (end_idx > size()) end_idx = size();
        for (std::size_t i = start; i < end_idx; ++i) { m_data[i] = impl_to_lower(m_data[i]); }
    }

    String uppercased() const { String r(*this); r.to_upper(); return r; }
    String lowercased() const { String r(*this); r.to_lower(); return r; }
    String uppercased(std::size_t start, std::size_t end_idx) const { String r(*this); r.to_upper(start, end_idx); return r; }
    String lowercased(std::size_t start, std::size_t end_idx) const { String r(*this); r.to_lower(start, end_idx); return r; }

    void capitalize() {
        if (empty()) return;
        to_lower();
        m_data[0] = impl_to_upper(m_data[0]);
    }

    String capitalized() const { String r(*this); r.capitalize(); return r; }

    void title_case() {
        bool new_word = true;
        for (std::size_t i = 0; i < size(); ++i) {
            if (is_whitespace(m_data[i])) { new_word = true; }
            else if (new_word) { m_data[i] = impl_to_upper(m_data[i]); new_word = false; }
            else { m_data[i] = impl_to_lower(m_data[i]); }
        }
    }

    String title_cased() const { String r(*this); r.title_case(); return r; }

    String snake_to_camel() const {
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

    String camel_to_snake() const {
        String result;
        result.reserve(size() + size() / 4 + 1);
        for (std::size_t i = 0; i < size(); ++i) {
            if (std::isupper(static_cast<unsigned char>(m_data[i])) && i > 0) result += '_';
            result += impl_to_lower(m_data[i]);
        }
        return result;
    }

public:
    void ltrim() {
        std::size_t n = 0;
        while (n < size() && is_whitespace(m_data[n])) ++n;
        if (n) m_data.pop_front(n);
    }

    void rtrim() {
        std::size_t s = size();
        while (s > 0 && is_whitespace(m_data[s - 1])) --s;
        m_data.resize(s + 1);
        m_data[s] = '\0';
    }

    void trim() { ltrim(); rtrim(); }

    String ltrimmed() const { String r(*this); r.ltrim(); return r; }
    String rtrimmed() const { String r(*this); r.rtrim(); return r; }
    String trimmed()  const { String r(*this); r.trim();  return r; }

    void normalize_whitespace() {
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

    String normalized_whitespace() const { String r(*this); r.normalize_whitespace(); return r; }

    String indent(std::size_t n, char pad = ' ') const {
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

    String wrap(std::size_t width) const {
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

    String truncate_to(std::size_t max_len, StringView ellipsis = StringView("...")) const {
        if (size() <= max_len) return *this;
        if (ellipsis.size() >= max_len) return String(ellipsis.substring(0, max_len));
        return substring(0, max_len - ellipsis.size()) + ellipsis;
    }

public:
    void remove(std::size_t index) { if (index < size()) m_data.remove(index); }
    void remove_front() { if (!empty()) m_data.remove_front(); }
    void remove_back()  { if (!empty()) m_data.remove(size() - 1); }

    std::size_t remove_by_value(char ch, std::size_t count = 0, Direction dir = Direction::All) {
        strip_null_terminator();
        std::size_t n = m_data.remove_by_value(ch, count, dir);
        restore_null_terminator();
        return n;
    }

    template <typename Pred>
    std::size_t remove_if(Pred pred, std::size_t count = 0, Direction dir = Direction::All) {
        strip_null_terminator();
        std::size_t n = m_data.remove_if(pred, count, dir);
        restore_null_terminator();
        return n;
    }

    char pop_front_char() {
        if (empty()) return '\0';
        char ch = m_data[0];
        m_data.remove_front();
        return ch;
    }

    char pop_back_char() {
        if (empty()) return '\0';
        char ch = m_data[size() - 1];
        remove_back();
        return ch;
    }

public:
    std::size_t replace_by_value(char old_val, char new_val, std::size_t count = 0, Direction dir = Direction::All) {
        strip_null_terminator();
        std::size_t n = m_data.replace_by_value(old_val, new_val, count, dir);
        restore_null_terminator();
        return n;
    }

    template <typename Pred>
    std::size_t replace_if(Pred pred, char new_val, std::size_t count = 0, Direction dir = Direction::All) {
        strip_null_terminator();
        std::size_t n = m_data.replace_if(pred, new_val, count, dir);
        restore_null_terminator();
        return n;
    }

    std::size_t replace_str(StringView old_sv, StringView new_sv,
                             std::size_t max_count = 0, Direction dir = Direction::All) {
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

    std::size_t replace_str(const char* old_s, const char* new_s,
                             std::size_t max_count = 0, Direction dir = Direction::All) {
        return replace_str(StringView(old_s), StringView(new_s), max_count, dir);
    }

public:
    void swap(std::size_t i, std::size_t j) noexcept { if (i < size() && j < size() && i != j) m_data.swap(i, j); }
    void reverse() noexcept { m_data.reverse(0, size()); }

    void reverse(std::size_t start, std::size_t end_idx) noexcept {
        if (end_idx > size()) end_idx = size();
        m_data.reverse(start, end_idx);
    }

    void sort() { std::sort(m_data.data(), m_data.data() + size()); }
    template <typename Compare> void sort(Compare comp) { std::sort(m_data.data(), m_data.data() + size(), comp); }

    void sort(std::size_t start, std::size_t end_idx) {
        if (end_idx > size()) end_idx = size();
        std::sort(m_data.data() + start, m_data.data() + end_idx);
    }

    template <typename Compare>
    void sort(std::size_t start, std::size_t end_idx, Compare comp) {
        if (end_idx > size()) end_idx = size();
        std::sort(m_data.data() + start, m_data.data() + end_idx, comp);
    }

    void stable_sort() { std::stable_sort(m_data.data(), m_data.data() + size()); }
    template <typename Compare> void stable_sort(Compare comp) { std::stable_sort(m_data.data(), m_data.data() + size(), comp); }

public:
    void unique() { strip_null_terminator(); m_data.unique(); restore_null_terminator(); }
    template <typename Pred> void unique(Pred pred) { strip_null_terminator(); m_data.unique(pred); restore_null_terminator(); }
    void deduplicate() { strip_null_terminator(); m_data.deduplicate(); restore_null_terminator(); }

    String distinct() const {
        String result;
        for (std::size_t i = 0; i < size(); ++i) { if (result.find(m_data[i]) == npos) result += m_data[i]; }
        return result;
    }

public:
    void rotate_left(std::size_t n = 1) {
        if (size() <= 1) return;
        strip_null_terminator(); m_data.rotate_left(n); restore_null_terminator();
    }

    void rotate_right(std::size_t n = 1) {
        if (size() <= 1) return;
        strip_null_terminator(); m_data.rotate_right(n); restore_null_terminator();
    }

    void shuffle() {
        if (size() <= 1) return;
        strip_null_terminator(); m_data.shuffle(); restore_null_terminator();
    }

    void shuffle(std::size_t start, std::size_t end_idx) {
        if (end_idx > size()) end_idx = size();
        m_data.shuffle(start, end_idx);
    }

    void unsecure_shuffle() {
        if (size() <= 1) return;
        strip_null_terminator(); m_data.unsecure_shuffle(); restore_null_terminator();
    }

    void unsecure_shuffle(std::size_t start, std::size_t end_idx) {
        if (end_idx > size()) end_idx = size();
        m_data.unsecure_shuffle(start, end_idx);
    }

public:
    DynamicArray<String> split(char delim, bool keep_empty = true) const {
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

    DynamicArray<String> split(StringView delim, bool keep_empty = true) const {
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

    DynamicArray<String> split(const char* delim, bool keep_empty = true) const {
        return split(StringView(delim), keep_empty);
    }

    DynamicArray<String> split_whitespace() const {
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

    std::pair<String, String> split_at(std::size_t pos) const {
        if (pos >= size()) return { *this, String() };
        return { substring(0, pos), substring(pos) };
    }

    DynamicArray<String> split_n(StringView delim, std::size_t max_parts) const {
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

    DynamicArray<String> rsplit(StringView delim, std::size_t n) const {
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

    DynamicArray<String> lines(bool keep_empty = true) const {
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

    DynamicArray<String> chunks(std::size_t n) const {
        DynamicArray<String> result;
        if (n == 0) return result;
        for (std::size_t i = 0; i < size(); i += n) result.push_back(substring(i, n));
        return result;
    }

    static String join(const DynamicArray<String>& parts, StringView sep) {
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

    static String join(const DynamicArray<String>& parts, char sep)       { return join(parts, StringView(&sep, 1)); }
    static String join(const DynamicArray<String>& parts, const char* sep) { return join(parts, StringView(sep)); }

    static String join(const DynamicArray<StringView>& parts, StringView sep) {
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

    static String join(const DynamicArray<StringView>& parts, char sep)        { return join(parts, StringView(&sep, 1)); }
    static String join(const DynamicArray<StringView>& parts, const char* sep) { return join(parts, StringView(sep)); }

public:
    String repeated(std::size_t n) const {
        if (n == 0 || empty()) return String();
        String result;
        result.reserve(size() * n + 1);
        for (std::size_t i = 0; i < n; ++i) result.append(*this);
        return result;
    }

    String padded_left(std::size_t total_width, char pad = ' ') const {
        if (size() >= total_width) return *this;
        String r(total_width - size(), pad);
        r.append(*this);
        return r;
    }

    String padded_right(std::size_t total_width, char pad = ' ') const {
        if (size() >= total_width) return *this;
        String r(*this);
        r.append(total_width - size(), pad);
        return r;
    }

    String padded_center(std::size_t total_width, char pad = ' ') const {
        if (size() >= total_width) return *this;
        std::size_t left = (total_width - size()) / 2;
        std::size_t right = total_width - size() - left;
        String r(left, pad);
        r.append(*this);
        r.append(right, pad);
        return r;
    }

    String zfill(std::size_t total_width) const { return padded_left(total_width, '0'); }

public:
    template <typename Func> String& for_each(Func func) { for (std::size_t i = 0; i < size(); ++i) { func(m_data[i]); } return *this; }
    template <typename Func> const String& for_each(Func func) const { for (std::size_t i = 0; i < size(); ++i) { func(m_data[i]); } return *this; }
    template <typename Func> String& for_each_indexed(Func func) { for (std::size_t i = 0; i < size(); ++i) { func(i, m_data[i]); } return *this; }
    template <typename Func> const String& for_each_indexed(Func func) const { for (std::size_t i = 0; i < size(); ++i) { func(i, m_data[i]); } return *this; }

public:
    template <typename Pred>
    String filter(Pred pred) const {
        String result;
        for (std::size_t i = 0; i < size(); ++i) { if (pred(m_data[i])) result += m_data[i]; }
        return result;
    }

    template <typename Pred>
    std::pair<String, String> partition(Pred pred) const {
        String matching, non_matching;
        for (std::size_t i = 0; i < size(); ++i) {
            if (pred(m_data[i])) matching += m_data[i];
            else                 non_matching += m_data[i];
        }
        return { std::move(matching), std::move(non_matching) };
    }

    template <typename U, typename Func>
    U reduce(U init, Func func) const {
        for (std::size_t i = 0; i < size(); ++i) { init = func(std::move(init), m_data[i]); }
        return init;
    }

public:
    inline StringWhereProxy where(char ch);
    template <typename Pred> inline StringWhereProxy where(Pred pred);
};

inline String StringView::to_string() const { return String(m_data, m_size); }
inline String StringView::to_owned()   const { return String(m_data, m_size); }

class StringWhereProxy {
    friend class String;
    String&                   m_str;
    DynamicArray<std::size_t> m_indices;

    StringWhereProxy(String& str, DynamicArray<std::size_t>&& indices)
        : m_str(str), m_indices(std::move(indices)) {}

public:
    StringWhereProxy& first(std::size_t n = 1) {
        if (n < m_indices.size()) m_indices.resize(n);
        return *this;
    }

    StringWhereProxy& last(std::size_t n = 1) {
        if (n < m_indices.size()) {
            std::size_t start = m_indices.size() - n;
            for (std::size_t i = 0; i < n; ++i) m_indices[i] = m_indices[start + i];
            m_indices.resize(n);
        }
        return *this;
    }

    StringWhereProxy& range(std::size_t start, std::size_t end_idx) {
        std::size_t write = 0;
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            if (m_indices[i] >= start && m_indices[i] < end_idx) m_indices[write++] = m_indices[i];
        }
        m_indices.resize(write);
        return *this;
    }

    StringWhereProxy& skip(std::size_t n = 1) {
        if (n >= m_indices.size()) { m_indices.clear(); }
        else {
            std::size_t new_count = m_indices.size() - n;
            for (std::size_t i = 0; i < new_count; ++i) m_indices[i] = m_indices[n + i];
            m_indices.resize(new_count);
        }
        return *this;
    }

    StringWhereProxy& at(std::size_t n) {
        if (n < m_indices.size()) { m_indices[0] = m_indices[n]; m_indices.resize(1); }
        else m_indices.clear();
        return *this;
    }

    StringWhereProxy& slice(std::size_t start, std::size_t end_idx) {
        if (start >= m_indices.size()) { m_indices.clear(); return *this; }
        if (end_idx > m_indices.size()) end_idx = m_indices.size();
        if (start >= end_idx) { m_indices.clear(); return *this; }
        std::size_t count = end_idx - start;
        for (std::size_t i = 0; i < count; ++i) m_indices[i] = m_indices[start + i];
        m_indices.resize(count);
        return *this;
    }

    StringWhereProxy& even() {
        std::size_t write = 0;
        for (std::size_t i = 0; i < m_indices.size(); i += 2) m_indices[write++] = m_indices[i];
        m_indices.resize(write);
        return *this;
    }

    StringWhereProxy& odd() {
        std::size_t write = 0;
        for (std::size_t i = 1; i < m_indices.size(); i += 2) m_indices[write++] = m_indices[i];
        m_indices.resize(write);
        return *this;
    }

    StringWhereProxy& step(std::size_t n) {
        if (n <= 1) return *this;
        std::size_t write = 0;
        for (std::size_t i = 0; i < m_indices.size(); i += n) m_indices[write++] = m_indices[i];
        m_indices.resize(write);
        return *this;
    }

    StringWhereProxy& except(const DynamicArray<std::size_t>& positions) {
        std::size_t write = 0;
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            if (!positions.contains(i)) m_indices[write++] = m_indices[i];
        }
        m_indices.resize(write);
        return *this;
    }

    template <typename Pred>
    StringWhereProxy& where(Pred pred) {
        std::size_t write = 0;
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            if (pred(m_str.m_data[m_indices[i]])) m_indices[write++] = m_indices[i];
        }
        m_indices.resize(write);
        return *this;
    }

    StringWhereProxy& invert() {
        std::size_t sz = m_str.size();
        DynamicArray<std::size_t> inv;
        inv.reserve(sz > m_indices.size() ? sz - m_indices.size() : 0);
        for (std::size_t i = 0; i < sz; ++i) {
            if (!m_indices.contains(i)) inv.push_back(i);
        }
        m_indices = std::move(inv);
        return *this;
    }

    StringWhereProxy& unique() {
        DynamicArray<char> seen;
        std::size_t write = 0;
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            char c = m_str.m_data[m_indices[i]];
            if (!seen.contains(c)) { seen.push_back(c); m_indices[write++] = m_indices[i]; }
        }
        m_indices.resize(write);
        return *this;
    }

    std::size_t count() const noexcept { return m_indices.size(); }
    bool empty()        const noexcept { return m_indices.empty(); }
    bool any()          const noexcept { return !m_indices.empty(); }
    ArrayView<std::size_t> indices() const noexcept { return ArrayView<std::size_t>(m_indices.data(), m_indices.size()); }
    DynamicArray<std::size_t> indices_copy() const { return DynamicArray<std::size_t>(m_indices); }

    DynamicArray<std::size_t> release_indices() noexcept {
        DynamicArray<std::size_t> out = std::move(m_indices);
        m_indices.deallocate();   
        return out;
    }

    DynamicArray<std::size_t> extract_indices() noexcept {
        DynamicArray<std::size_t> out = std::move(m_indices);
        m_indices.clear();        
        return out;
    }

    bool contains(char ch) const noexcept {
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            if (m_str.m_data[m_indices[i]] == ch) return true;
        }
        return false;
    }

    template <typename Pred>
    std::size_t count_if(Pred pred) const noexcept {
        std::size_t n = 0;
        for (std::size_t i = 0; i < m_indices.size(); ++i) { if (pred(m_str.m_data[m_indices[i]])) ++n; }
        return n;
    }

    String values() const {
        String result;
        result.reserve(m_indices.size() + 1);
        for (std::size_t i = 0; i < m_indices.size(); ++i) result += m_str.m_data[m_indices[i]];
        return result;
    }

    String as_string() const { return values(); }

    char* first_value() noexcept {
        return m_indices.empty() ? nullptr : &m_str.m_data[m_indices.front()];
    }

    const char* first_value() const noexcept {
        return m_indices.empty() ? nullptr : &m_str.m_data[m_indices.front()];
    }

    char* last_value() noexcept {
        return m_indices.empty() ? nullptr : &m_str.m_data[m_indices.back()];
    }

    const char* last_value() const noexcept {
        return m_indices.empty() ? nullptr : &m_str.m_data[m_indices.back()];
    }

    std::size_t remove() {
        std::size_t n = m_indices.size();
        if (n == 0) return 0;
        m_str.strip_null_terminator();
        for (std::size_t i = n; i > 0; --i) m_str.m_data.remove(m_indices[i - 1]);
        m_str.restore_null_terminator();
        return n;
    }

    std::size_t remove_unordered() {
        std::size_t n = m_indices.size();
        if (n == 0) return 0;
        m_str.strip_null_terminator();
        for (std::size_t i = n; i > 0; --i) m_str.m_data.remove_unordered(m_indices[i - 1]);
        m_str.restore_null_terminator();
        return n;
    }

    String pop() {
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

    String pop_unordered() {
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

    void replace(char value) {
        for (std::size_t i = 0; i < m_indices.size(); ++i) m_str.m_data[m_indices[i]] = value;
    }

    template <typename Func>
    StringWhereProxy& replace_with(Func func) {
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            m_str.m_data[m_indices[i]] = func(m_str.m_data[m_indices[i]]);
        }
        return *this;
    }

    template <typename Func>
    StringWhereProxy& for_each(Func func) {
        for (std::size_t i = 0; i < m_indices.size(); ++i) func(m_str.m_data[m_indices[i]]);
        return *this;
    }

    template <typename Func>
    StringWhereProxy& transform(Func func) {
        for (std::size_t i = 0; i < m_indices.size(); ++i) func(m_str.m_data[m_indices[i]]);
        return *this;
    }

    StringWhereProxy& sort() {
        std::size_t n = m_indices.size();
        if (n <= 1) return *this;
        DynamicArray<char> tmp;
        tmp.reserve(n);
        for (std::size_t i = 0; i < n; ++i) tmp.push_back(m_str.m_data[m_indices[i]]);
        std::sort(tmp.data(), tmp.data() + n);
        for (std::size_t i = 0; i < n; ++i) m_str.m_data[m_indices[i]] = tmp[i];
        return *this;
    }

    template <typename Compare>
    StringWhereProxy& sort(Compare comp) {
        std::size_t n = m_indices.size();
        if (n <= 1) return *this;
        DynamicArray<char> tmp;
        tmp.reserve(n);
        for (std::size_t i = 0; i < n; ++i) tmp.push_back(m_str.m_data[m_indices[i]]);
        std::sort(tmp.data(), tmp.data() + n, comp);
        for (std::size_t i = 0; i < n; ++i) m_str.m_data[m_indices[i]] = tmp[i];
        return *this;
    }

    StringWhereProxy& reverse() {
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

    StringWhereProxy& shuffle() {
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

    StringWhereProxy& to_upper() {
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            char& c = m_str.m_data[m_indices[i]];
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return *this;
    }

    StringWhereProxy& to_lower() {
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            char& c = m_str.m_data[m_indices[i]];
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return *this;
    }

    String uppercased() const {
        String r(m_str);
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            r.m_data[m_indices[i]] = static_cast<char>(std::toupper(static_cast<unsigned char>(r.m_data[m_indices[i]])));
        }
        return r;
    }

    String lowercased() const {
        String r(m_str);
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            r.m_data[m_indices[i]] = static_cast<char>(std::tolower(static_cast<unsigned char>(r.m_data[m_indices[i]])));
        }
        return r;
    }

    void copy_to(String& dest) const {
        dest.strip_null_terminator();
        for (std::size_t i = 0; i < m_indices.size(); ++i) dest.m_data.push_back(m_str.m_data[m_indices[i]]);
        dest.restore_null_terminator();
    }

    void copy_to(DynamicArray<char>& dest) const {
        for (std::size_t i = 0; i < m_indices.size(); ++i) dest.push_back(m_str.m_data[m_indices[i]]);
    }

    void move_to(String& dest) {
        dest.strip_null_terminator();
        for (std::size_t i = 0; i < m_indices.size(); ++i) dest.m_data.push_back(m_str.m_data[m_indices[i]]);
        dest.restore_null_terminator();
        m_str.strip_null_terminator();
        for (std::size_t i = m_indices.size(); i > 0; --i) m_str.m_data.remove(m_indices[i - 1]);
        m_str.restore_null_terminator();
    }

    void insert_before(char value) {
        if (m_indices.empty()) return;
        m_str.strip_null_terminator();
        for (std::size_t i = m_indices.size(); i > 0; --i) {
            std::size_t idx = m_indices[i - 1] + (i - 1);
            m_str.m_data.insert(idx, value);
        }
        m_str.restore_null_terminator();
    }

    void insert_after(char value) {
        if (m_indices.empty()) return;
        m_str.strip_null_terminator();
        for (std::size_t i = m_indices.size(); i > 0; --i) {
            std::size_t idx = m_indices[i - 1] + i;
            m_str.m_data.insert(idx, value);
        }
        m_str.restore_null_terminator();
    }

    String swap_with(char value) {
        String result;
        result.reserve(m_indices.size() + 1);
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            result += m_str.m_data[m_indices[i]];
            m_str.m_data[m_indices[i]] = value;
        }
        return result;
    }
};

inline StringWhereProxy String::where(char ch) {
    return StringWhereProxy(*this, m_data.where(ch).release_indices());
}

template <typename Pred>
inline StringWhereProxy String::where(Pred pred) {
    return StringWhereProxy(*this, m_data.where(pred).release_indices());
}

} // namespace fizmo

namespace std {
template <>
struct hash<fizmo::String> {
    std::size_t operator()(const fizmo::String& s) const noexcept { return s.hash(); }
};
}

#endif // FIZMO_CUSTOM_STRING_CLASS_HPP