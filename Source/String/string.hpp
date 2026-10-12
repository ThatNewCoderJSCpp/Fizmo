#ifndef FIZMO_CUSTOM_STRING_CLASS_HPP
#define FIZMO_CUSTOM_STRING_CLASS_HPP

#include "string_view.hpp"
#include "unicode.hpp"
#include "../Arrays/array.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <istream>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#if defined(__GNUC__) || defined(__clang__)
    #define FIZMO_STRING_PRINTF_FORMAT(F, A) __attribute__((format(printf, F, A)))
#else
    #define FIZMO_STRING_PRINTF_FORMAT(F, A)
#endif

namespace fizmo {

class StringWhereProxy;

class String {
public:
    using value_type             = char;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using reference              = char&;
    using const_reference        = const char&;
    using pointer                = char*;
    using const_pointer          = const char*;
    using iterator               = char*;
    using const_iterator         = const char*;
    using reverse_iterator       = std::reverse_iterator<char*>;
    using const_reverse_iterator = std::reverse_iterator<const char*>;
    using NormalizationForm      = unicode::NormalizationForm;

    static constexpr std::size_t npos           = static_cast<std::size_t>(-1);
    static constexpr std::size_t small_capacity = 15;

private:
    char*       m_ptr;
    std::size_t m_size;
    union {
        std::size_t m_capacity;
        char        m_local[small_capacity + 1];
    };

    friend class StringWhereProxy;

    bool is_small() const noexcept { return m_ptr == m_local; }

    void set_size(std::size_t n) noexcept {
        m_size     = n;
        m_ptr[n]   = '\0';
    }

    void release() noexcept {
        if (!is_small()) delete[] m_ptr;
        m_ptr      = m_local;
        m_local[0] = '\0';
    }

    void reallocate(std::size_t new_capacity);
    void grow_to(std::size_t needed);
    void assign_raw(const char* str, std::size_t len);
    void append_raw(const char* str, std::size_t len);
    void splice(std::size_t pos, std::size_t count, const char* str, std::size_t len);
    bool aliases(const char* p) const noexcept { return p >= m_ptr && p <= m_ptr + m_size; }

    static std::size_t c_strlen(const char* str) noexcept { return str ? std::strlen(str) : 0; }

    static bool str_equal(const char* a, std::size_t a_len, const char* b, std::size_t b_len) noexcept {
        return a_len == b_len && (a_len == 0 || std::memcmp(a, b, a_len) == 0);
    }

    static int str_compare(const char* a, std::size_t a_len, const char* b, std::size_t b_len) noexcept {
        const std::size_t n = a_len < b_len ? a_len : b_len;
        const int         c = n ? std::memcmp(a, b, n) : 0;
        if (c != 0) return c < 0 ? -1 : 1;
        return a_len < b_len ? -1 : (a_len > b_len ? 1 : 0);
    }

    static bool glob_match(const char* s, std::size_t slen, const char* p, std::size_t plen) noexcept;

    collections::DynamicArray<StringView> grapheme_list() const;
    void rebuild_from(const collections::DynamicArray<StringView>& parts);
    std::size_t floor_grapheme(std::size_t pos) const noexcept;
    std::size_t ceil_grapheme(std::size_t pos) const noexcept;

    template <typename Pred>
    collections::DynamicArray<std::size_t> collect_indices_if(Pred pred) const {
        collections::DynamicArray<std::size_t> idx;
        for (std::size_t i = 0; i < m_size; ++i) {
            if (pred(m_ptr[i])) idx.push_back(i);
        }
        return idx;
    }

    template <typename Pred>
    std::size_t erase_if_impl(Pred pred, std::size_t count, Direction dir);

public:
    constexpr String() noexcept : m_ptr(m_local), m_size(0), m_local{} {}
    String(const String& other) : String() { assign_raw(other.m_ptr, other.m_size); }
    String(String&& other) noexcept;
    String(const char* str) : String() { assign_raw(str, c_strlen(str)); }
    String(const char* str, std::size_t len) : String() { assign_raw(str, len); }
    String(char* str) : String(static_cast<const char*>(str)) {}
    String(StringView sv) : String() { assign_raw(sv.data(), sv.size()); }
    String(const std::string& str) : String() { assign_raw(str.data(), str.size()); }
    String(std::string_view sv) : String() { assign_raw(sv.data(), sv.size()); }
    explicit String(char ch) : String() { assign_raw(&ch, 1); }
    String(std::size_t count, char ch);

    template <typename It, typename = typename std::enable_if<!std::is_integral<It>::value && std::is_convertible<typename std::iterator_traits<It>::value_type, char>::value>::type>
    String(It first, It last) : String() {
        for (; first != last; ++first) push_back(static_cast<char>(*first));
    }

#if defined(__cpp_char8_t)
    String(const char8_t* str) : String() {
        if (str) assign_raw(reinterpret_cast<const char*>(str), std::char_traits<char8_t>::length(str));
    }
    String(std::u8string_view sv) : String() { assign_raw(reinterpret_cast<const char*>(sv.data()), sv.size()); }
#endif

    ~String() { release(); }

    String& operator=(const String& other) {
        if (this != &other) assign_raw(other.m_ptr, other.m_size);
        return *this;
    }

    String& operator=(String&& other) noexcept;
    String& operator=(const char* str) { assign_raw(str, c_strlen(str)); return *this; }
    String& operator=(char* str) { return *this = static_cast<const char*>(str); }
    String& operator=(StringView sv) { assign_raw(sv.data(), sv.size()); return *this; }
    String& operator=(const std::string& str) { assign_raw(str.data(), str.size()); return *this; }
    String& operator=(std::string_view sv) { assign_raw(sv.data(), sv.size()); return *this; }
    String& operator=(char ch) { assign_raw(&ch, 1); return *this; }

    static String from_code_point(char32_t cp);
    static String from_code_points(const char32_t* cps, std::size_t n);
    static String from_utf16(const char16_t* str, std::size_t len);
    static String from_utf16(std::u16string_view sv) { return from_utf16(sv.data(), sv.size()); }
    static String from_utf32(const char32_t* str, std::size_t len) { return from_code_points(str, len); }
    static String from_utf32(std::u32string_view sv) { return from_code_points(sv.data(), sv.size()); }
    static String from_wide(const wchar_t* str, std::size_t len);
    static String from_wide(std::wstring_view sv) { return from_wide(sv.data(), sv.size()); }

    std::u16string to_utf16() const;
    std::u32string to_utf32() const;
    std::wstring   to_wide() const;
    std::string    to_std_string() const { return std::string(m_ptr, m_size); }

#if defined(__cpp_char8_t)
    std::u8string to_u8string() const { return std::u8string(reinterpret_cast<const char8_t*>(m_ptr), m_size); }
#endif

public:
    std::size_t size() const noexcept { return m_size; }
    std::size_t length() const noexcept { return m_size; }
    std::size_t capacity() const noexcept { return is_small() ? small_capacity : m_capacity; }
    std::size_t max_size() const noexcept { return static_cast<std::size_t>(-1) / 2 - 1; }
    bool empty() const noexcept { return m_size == 0; }
    char* data() noexcept { return m_ptr; }
    const char* data() const noexcept { return m_ptr; }
    char* c_str() noexcept { return m_ptr; }
    const char* c_str() const noexcept { return m_ptr; }
    char& operator[](std::size_t index) noexcept { return m_ptr[index]; }
    const char& operator[](std::size_t index) const noexcept { return m_ptr[index]; }
    char* at(std::size_t index) noexcept { return index < m_size ? m_ptr + index : nullptr; }
    const char* at(std::size_t index) const noexcept { return index < m_size ? m_ptr + index : nullptr; }
    char& front() noexcept { return m_ptr[0]; }
    const char& front() const noexcept { return m_ptr[0]; }
    char& back() noexcept { return m_ptr[m_size - 1]; }
    const char& back() const noexcept { return m_ptr[m_size - 1]; }

public:
    void reserve(std::size_t new_capacity) {
        if (new_capacity > capacity()) reallocate(new_capacity);
    }

    void shrink();
    void shrink_to_fit() { shrink(); }
    void clear() noexcept { set_size(0); }
    void resize(std::size_t new_size) { resize(new_size, '\0'); }
    void resize(std::size_t new_size, char ch);

    void truncate(std::size_t n) noexcept {
        if (n < m_size) set_size(n);
    }

    void swap(String& other) noexcept {
        String tmp(std::move(other));
        other = std::move(*this);
        *this = std::move(tmp);
    }

    void swap(std::size_t i, std::size_t j) noexcept {
        if (i < m_size && j < m_size && i != j) std::swap(m_ptr[i], m_ptr[j]);
    }

public:
    char* begin() noexcept { return m_ptr; }
    char* end() noexcept { return m_ptr + m_size; }
    const char* begin() const noexcept { return m_ptr; }
    const char* end() const noexcept { return m_ptr + m_size; }
    const char* cbegin() const noexcept { return m_ptr; }
    const char* cend() const noexcept { return m_ptr + m_size; }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

public:
    StringView view() const noexcept { return StringView(m_ptr, m_size); }
    StringView view(std::size_t start, std::size_t count = npos) const noexcept { return view().substring(start, count); }
    operator StringView() const noexcept { return view(); }

    unicode::CodePointView code_points() const& noexcept { return unicode::CodePointView(view()); }
    unicode::GraphemeView graphemes() const& noexcept { return unicode::GraphemeView(view()); }
    unicode::WordView words(bool words_only = true) const& noexcept { return unicode::WordView(view(), words_only); }
    unicode::CodePointView code_points() const&& = delete;
    unicode::GraphemeView graphemes() const&& = delete;
    unicode::WordView words(bool words_only = true) const&& = delete;

    std::size_t code_point_count() const noexcept { return unicode::count_code_points(view()); }
    std::size_t grapheme_count() const noexcept { return unicode::count_graphemes(view()); }
    std::size_t display_width() const noexcept { return unicode::display_width(view()); }
    char32_t code_point_at(std::size_t byte_index) const noexcept { return unicode::code_point_at(view(), byte_index); }
    StringView grapheme_at(std::size_t index) const noexcept;
    String grapheme_substring(std::size_t first, std::size_t count = npos) const;
    bool is_ascii() const noexcept;
    bool is_valid_utf8() const noexcept { return unicode::is_valid_utf8(view()); }
    void sanitize_utf8();
    String sanitized_utf8() const { String r(*this); r.sanitize_utf8(); return r; }

public:
    int compare(const String& other) const noexcept { return str_compare(m_ptr, m_size, other.m_ptr, other.m_size); }
    int compare(const char* str) const noexcept { return str_compare(m_ptr, m_size, str, c_strlen(str)); }
    int compare(StringView sv) const noexcept { return str_compare(m_ptr, m_size, sv.data(), sv.size()); }
    int compare(const std::string& str) const noexcept { return str_compare(m_ptr, m_size, str.data(), str.size()); }
    int compare(std::string_view sv) const noexcept { return str_compare(m_ptr, m_size, sv.data(), sv.size()); }

    int compare_ignore_case(StringView sv) const { return unicode::compare_ignore_case(view(), sv); }
    bool equals_ignore_case(StringView sv) const { return unicode::equals_ignore_case(view(), sv); }

    friend bool operator==(const String& lhs, const String& rhs) noexcept { return str_equal(lhs.m_ptr, lhs.m_size, rhs.m_ptr, rhs.m_size); }
    friend bool operator==(const String& lhs, const char* rhs) noexcept { return str_equal(lhs.m_ptr, lhs.m_size, rhs, c_strlen(rhs)); }
    friend bool operator==(const char* lhs, const String& rhs) noexcept { return str_equal(lhs, c_strlen(lhs), rhs.m_ptr, rhs.m_size); }
    friend bool operator==(const String& lhs, StringView rhs) noexcept { return str_equal(lhs.m_ptr, lhs.m_size, rhs.data(), rhs.size()); }
    friend bool operator==(StringView lhs, const String& rhs) noexcept { return str_equal(lhs.data(), lhs.size(), rhs.m_ptr, rhs.m_size); }
    friend bool operator==(const String& lhs, const std::string& rhs) noexcept { return str_equal(lhs.m_ptr, lhs.m_size, rhs.data(), rhs.size()); }
    friend bool operator==(const std::string& lhs, const String& rhs) noexcept { return str_equal(lhs.data(), lhs.size(), rhs.m_ptr, rhs.m_size); }
    friend bool operator==(const String& lhs, std::string_view rhs) noexcept { return str_equal(lhs.m_ptr, lhs.m_size, rhs.data(), rhs.size()); }
    friend bool operator==(std::string_view lhs, const String& rhs) noexcept { return str_equal(lhs.data(), lhs.size(), rhs.m_ptr, rhs.m_size); }

    friend bool operator!=(const String& lhs, const String& rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(const String& lhs, const char* rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(const char* lhs, const String& rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(const String& lhs, StringView rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(StringView lhs, const String& rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(const String& lhs, const std::string& rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(const std::string& lhs, const String& rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(const String& lhs, std::string_view rhs) noexcept { return !(lhs == rhs); }
    friend bool operator!=(std::string_view lhs, const String& rhs) noexcept { return !(lhs == rhs); }

    friend bool operator<(const String& lhs, const String& rhs) noexcept { return lhs.compare(rhs) < 0; }
    friend bool operator<=(const String& lhs, const String& rhs) noexcept { return lhs.compare(rhs) <= 0; }
    friend bool operator>(const String& lhs, const String& rhs) noexcept { return lhs.compare(rhs) > 0; }
    friend bool operator>=(const String& lhs, const String& rhs) noexcept { return lhs.compare(rhs) >= 0; }

    friend std::ostream& operator<<(std::ostream& os, const String& str) {
        return os.write(str.m_ptr, static_cast<std::streamsize>(str.m_size));
    }

    friend std::istream& operator>>(std::istream& is, String& str) {
        std::string tmp;
        is >> tmp;
        str = tmp;
        return is;
    }

public:
    String& operator+=(const String& other) { append_raw(other.m_ptr, other.m_size); return *this; }
    String& operator+=(const char* str) { append_raw(str, c_strlen(str)); return *this; }
    String& operator+=(StringView sv) { append_raw(sv.data(), sv.size()); return *this; }
    String& operator+=(const std::string& str) { append_raw(str.data(), str.size()); return *this; }
    String& operator+=(std::string_view sv) { append_raw(sv.data(), sv.size()); return *this; }
    String& operator+=(char ch) { push_back(ch); return *this; }

    friend String operator+(const String& lhs, const String& rhs) { String r; r.reserve(lhs.size() + rhs.size()); r += lhs; r += rhs; return r; }
    friend String operator+(const String& lhs, const char* rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(const char* lhs, const String& rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(const String& lhs, StringView rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(StringView lhs, const String& rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(const String& lhs, const std::string& rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(const std::string& lhs, const String& rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(const String& lhs, std::string_view rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(std::string_view lhs, const String& rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(const String& lhs, char rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(char lhs, const String& rhs) { String r(lhs); r += rhs; return r; }
    friend String operator+(String&& lhs, const String& rhs) { lhs += rhs; return std::move(lhs); }
    friend String operator+(String&& lhs, const char* rhs) { lhs += rhs; return std::move(lhs); }
    friend String operator+(String&& lhs, StringView rhs) { lhs += rhs; return std::move(lhs); }
    friend String operator+(String&& lhs, const std::string& rhs) { lhs += rhs; return std::move(lhs); }
    friend String operator+(String&& lhs, std::string_view rhs) { lhs += rhs; return std::move(lhs); }
    friend String operator+(String&& lhs, char rhs) { lhs += rhs; return std::move(lhs); }

    friend String operator*(const String& s, std::size_t n) { return s.repeated(n); }
    friend String operator*(std::size_t n, const String& s) { return s.repeated(n); }

public:
    explicit operator std::string() const { return std::string(m_ptr, m_size); }
    explicit operator std::string_view() const noexcept { return std::string_view(m_ptr, m_size); }
    explicit operator bool() const noexcept { return m_size != 0; }
    explicit operator const char*() const noexcept { return m_ptr; }
    explicit operator int() const noexcept { return as_int<int>(); }
    explicit operator long() const noexcept { return as_int<long>(); }
    explicit operator long long() const noexcept { return as_int<long long>(); }
    explicit operator unsigned int() const noexcept { return as_int<unsigned int>(); }
    explicit operator unsigned long() const noexcept { return as_int<unsigned long>(); }
    explicit operator unsigned long long() const noexcept { return as_int<unsigned long long>(); }

public:
    static constexpr bool is_whitespace(char c) noexcept { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    static constexpr T get_int(char ch) noexcept { return (ch >= '0' && ch <= '9') ? static_cast<T>(ch - '0') : T{}; }

    template <typename T>
    static T get_int(StringView sv) noexcept {
        std::size_t i = 0;
        const std::size_t n = sv.size();
        while (i < n && is_whitespace(sv[i])) ++i;
        bool negative = false;
        if (i < n && (sv[i] == '-' || sv[i] == '+')) {
            negative = sv[i] == '-';
            ++i;
        }
        T result = T{};
        while (i < n && sv[i] >= '0' && sv[i] <= '9') {
            result = static_cast<T>(result * static_cast<T>(10) + static_cast<T>(sv[i] - '0'));
            ++i;
        }
        return negative ? static_cast<T>(T{} - result) : result;
    }

    template <typename T = int>
    T as_int() const noexcept { return get_int<T>(view()); }

    float       to_float() const noexcept { return std::strtof(m_ptr, nullptr); }
    double      to_double() const noexcept { return std::strtod(m_ptr, nullptr); }
    long double to_long_double() const noexcept { return std::strtold(m_ptr, nullptr); }

    bool to_bool() const noexcept;
    bool is_integer() const noexcept;
    bool is_float() const noexcept;

    static String from_int(long long v) { return format("%lld", v); }
    static String from_uint(unsigned long long v) { return format("%llu", v); }
    static String from_float(float v, int prec = 6) { return format("%.*f", prec, static_cast<double>(v)); }
    static String from_double(double v, int prec = 6) { return format("%.*f", prec, v); }
    static String from_long_double(long double v, int prec = 6);
    static String from_bool(bool v) { return String(v ? "true" : "false"); }

    static String format(const char* fmt, ...) FIZMO_STRING_PRINTF_FORMAT(1, 2);
    static String vformat(const char* fmt, va_list args);

    static String from_stream(std::istream& is);

    void write_to(std::ostream& os) const { os.write(m_ptr, static_cast<std::streamsize>(m_size)); }

    String to_hex() const;
    static String from_hex(StringView sv);
    String to_base64() const;
    static String from_base64(StringView sv);
    String escape(StringView chars) const;
    String unescape() const;
    String url_encode() const;
    String url_decode() const;

    std::size_t hash() const noexcept { return view().hash(); }

public:
    String substring(std::size_t pos, std::size_t count = npos) const { return String(view().substring(pos, count)); }
    String substr(std::size_t pos, std::size_t count = npos) const { return substring(pos, count); }

public:
    void push_back(char ch) {
        if (m_size == capacity()) grow_to(m_size + 1);
        m_ptr[m_size] = ch;
        set_size(m_size + 1);
    }

    void pop_back() noexcept {
        if (m_size) set_size(m_size - 1);
    }

    String& append(const String& s) { return *this += s; }
    String& append(const char* s) { return *this += s; }
    String& append(const char* s, std::size_t len) { append_raw(s, len); return *this; }
    String& append(StringView sv) { return *this += sv; }
    String& append(const std::string& s) { return *this += s; }
    String& append(std::string_view sv) { return *this += sv; }
    String& append(char ch) { push_back(ch); return *this; }
    String& append(std::size_t n, char ch);
    String& append_code_point(char32_t cp);
    String& append_code_point(std::size_t n, char32_t cp);

    String& prepend(const String& s) { return insert(0, s); }
    String& prepend(const char* s) { return insert(0, s); }
    String& prepend(StringView sv) { return insert(0, sv); }
    String& prepend(const std::string& s) { return insert(0, s.data(), s.size()); }
    String& prepend(std::string_view sv) { return insert(0, sv.data(), sv.size()); }
    String& prepend(char ch) { return insert(0, ch); }

public:
    String& insert(std::size_t idx, char ch) { return insert(idx, &ch, 1); }
    String& insert(std::size_t idx, const char* str, std::size_t len);
    String& insert(std::size_t idx, const char* str) { return insert(idx, str, c_strlen(str)); }
    String& insert(std::size_t idx, StringView sv) { return insert(idx, sv.data(), sv.size()); }
    String& insert(std::size_t idx, const String& s) { return insert(idx, s.m_ptr, s.m_size); }
    String& insert(std::size_t idx, const std::string& s) { return insert(idx, s.data(), s.size()); }
    String& insert(std::size_t idx, std::string_view sv) { return insert(idx, sv.data(), sv.size()); }

    String& erase(std::size_t pos, std::size_t count = npos);
    String& replace(std::size_t pos, std::size_t count, StringView with);

public:
    std::size_t find(char ch, std::size_t pos = 0) const noexcept { return view().find(ch, pos); }
    std::size_t find(StringView sv, std::size_t pos = 0) const noexcept { return view().find(sv, pos); }
    std::size_t find(const char* s, std::size_t pos = 0) const noexcept { return view().find(StringView(s), pos); }

    template <typename Pred>
    std::size_t find_if(Pred pred, std::size_t pos = 0) const noexcept {
        for (std::size_t i = pos; i < m_size; ++i) {
            if (pred(m_ptr[i])) return i;
        }
        return npos;
    }

    std::size_t find_last(char ch, std::size_t before = npos) const noexcept { return view().rfind(ch, before); }

    template <typename Pred>
    std::size_t find_last_if(Pred pred, std::size_t before = npos) const noexcept {
        const std::size_t lim = before < m_size ? before : m_size;
        for (std::size_t i = lim; i > 0; --i) {
            if (pred(m_ptr[i - 1])) return i - 1;
        }
        return npos;
    }

    std::size_t rfind(char ch, std::size_t before = npos) const noexcept { return view().rfind(ch, before); }
    std::size_t rfind(StringView sv, std::size_t before = npos) const noexcept { return view().rfind(sv, before); }
    std::size_t rfind(const char* s, std::size_t before = npos) const noexcept { return rfind(StringView(s), before); }

    collections::DynamicArray<std::size_t> find_all(StringView sv) const;
    collections::DynamicArray<std::size_t> find_all(char ch) const { return collect_indices_if([ch](char c) { return c == ch; }); }

    std::size_t find_nth(char ch, std::size_t n) const noexcept;
    std::size_t find_nth(StringView sv, std::size_t n) const noexcept;

    String find_between(StringView open, StringView close) const;

    bool matches_glob(StringView pattern) const noexcept { return glob_match(m_ptr, m_size, pattern.data(), pattern.size()); }

    std::size_t find_first_of(StringView charset, std::size_t pos = 0) const noexcept { return view().find_first_of(charset, pos); }
    std::size_t find_last_of(StringView charset, std::size_t before = npos) const noexcept { return view().find_last_of(charset, before); }
    std::size_t find_first_not_of(StringView charset, std::size_t pos = 0) const noexcept { return view().find_first_not_of(charset, pos); }
    std::size_t find_last_not_of(StringView charset, std::size_t before = npos) const noexcept { return view().find_last_not_of(charset, before); }

    collections::DynamicArray<std::size_t> find_indices(char ch) const { return find_all(ch); }
    template <typename Pred> collections::DynamicArray<std::size_t> find_indices_if(Pred pred) const { return collect_indices_if(pred); }

public:
    bool starts_with(char ch) const noexcept { return view().starts_with(ch); }
    bool starts_with(StringView sv) const noexcept { return view().starts_with(sv); }
    bool starts_with(const char* s) const noexcept { return view().starts_with(StringView(s)); }
    bool ends_with(char ch) const noexcept { return view().ends_with(ch); }
    bool ends_with(StringView sv) const noexcept { return view().ends_with(sv); }
    bool ends_with(const char* s) const noexcept { return view().ends_with(StringView(s)); }

public:
    std::size_t count_of(char ch) const noexcept { return view().count(ch); }
    std::size_t count_occurrences(StringView sv) const noexcept;

    template <typename Pred>
    std::size_t count_if(Pred pred) const noexcept {
        std::size_t n = 0;
        for (std::size_t i = 0; i < m_size; ++i) {
            if (pred(m_ptr[i])) ++n;
        }
        return n;
    }

    bool contains(char ch) const noexcept { return find(ch) != npos; }
    bool contains(StringView sv) const noexcept { return find(sv) != npos; }
    bool contains(const char* s) const noexcept { return find(StringView(s)) != npos; }

    template <typename Pred>
    bool contains_if(Pred pred) const noexcept { return find_if(pred) != npos; }

    template <typename Pred> bool all_of(Pred pred) const noexcept { return find_if([&](char c) { return !pred(c); }) == npos; }
    template <typename Pred> bool any_of(Pred pred) const noexcept { return find_if(pred) != npos; }
    template <typename Pred> bool none_of(Pred pred) const noexcept { return find_if(pred) == npos; }

    bool is_alpha() const noexcept;
    bool is_digit() const noexcept;
    bool is_alnum() const noexcept;
    bool is_whitespace_only() const noexcept;
    bool is_hex() const noexcept;
    bool is_upper() const noexcept;
    bool is_lower() const noexcept;
    bool is_palindrome() const;
    bool is_sorted() const noexcept;

    template <typename Compare>
    bool is_sorted(Compare comp) const noexcept {
        std::size_t pos = 0;
        char32_t    prev = 0;
        bool        have = false;
        while (pos < m_size) {
            const char32_t cp = unicode::decode_utf8(m_ptr, m_size, pos);
            if (have && comp(cp, prev)) return false;
            prev = cp;
            have = true;
        }
        return true;
    }

    std::size_t levenshtein_distance(StringView other) const;
    StringView common_prefix(StringView other) const noexcept;
    StringView common_suffix(StringView other) const noexcept;
    std::size_t word_count() const noexcept;

    char min() const noexcept;
    char max() const noexcept;
    std::pair<char, char> min_max() const noexcept;

    template <typename Compare>
    char min(Compare comp) const noexcept {
        if (empty()) return '\0';
        char m = m_ptr[0];
        for (std::size_t i = 1; i < m_size; ++i) {
            if (comp(m_ptr[i], m)) m = m_ptr[i];
        }
        return m;
    }

    template <typename Compare>
    char max(Compare comp) const noexcept {
        if (empty()) return '\0';
        char m = m_ptr[0];
        for (std::size_t i = 1; i < m_size; ++i) {
            if (comp(m, m_ptr[i])) m = m_ptr[i];
        }
        return m;
    }

public:
    void to_upper() { *this = unicode::to_upper(view()); }
    void to_lower() { *this = unicode::to_lower(view()); }
    void to_upper(std::size_t index) { to_upper(index, index + 1); }
    void to_lower(std::size_t index) { to_lower(index, index + 1); }
    void to_upper(std::size_t start, std::size_t end_idx);
    void to_lower(std::size_t start, std::size_t end_idx);

    String uppercased() const { return unicode::to_upper(view()); }
    String lowercased() const { return unicode::to_lower(view()); }
    String uppercased(std::size_t start, std::size_t end_idx) const { String r(*this); r.to_upper(start, end_idx); return r; }
    String lowercased(std::size_t start, std::size_t end_idx) const { String r(*this); r.to_lower(start, end_idx); return r; }

    void case_fold() { *this = unicode::fold_case(view()); }
    String case_folded() const { return unicode::fold_case(view()); }

    void capitalize();
    String capitalized() const { String r(*this); r.capitalize(); return r; }
    void title_case() { *this = unicode::to_title(view()); }
    String title_cased() const { return unicode::to_title(view()); }
    String snake_to_camel() const;
    String camel_to_snake() const;

    void normalize(NormalizationForm form = NormalizationForm::nfc) { *this = unicode::normalize(view(), form); }
    String normalized(NormalizationForm form = NormalizationForm::nfc) const { return unicode::normalize(view(), form); }
    bool is_normalized(NormalizationForm form = NormalizationForm::nfc) const { return unicode::is_normalized(view(), form); }

public:
    void ltrim();
    void rtrim();
    void trim() { rtrim(); ltrim(); }
    String ltrimmed() const { String r(*this); r.ltrim(); return r; }
    String rtrimmed() const { String r(*this); r.rtrim(); return r; }
    String trimmed() const { String r(*this); r.trim(); return r; }

    void trim(StringView chars);
    String trimmed(StringView chars) const { String r(*this); r.trim(chars); return r; }

    void normalize_whitespace();
    String normalized_whitespace() const { String r(*this); r.normalize_whitespace(); return r; }
    String indent(std::size_t n, char pad = ' ') const;
    String wrap(std::size_t width) const;
    String truncate_to(std::size_t max_width, StringView ellipsis = StringView("...")) const;

public:
    void remove(std::size_t index);
    void remove_front();
    void remove_back();

    std::size_t remove_by_value(char ch, std::size_t count = 0, Direction dir = Direction::All) {
        return erase_if_impl([ch](char c) { return c == ch; }, count, dir);
    }

    template <typename Pred>
    std::size_t remove_if(Pred pred, std::size_t count = 0, Direction dir = Direction::All) { return erase_if_impl(pred, count, dir); }

    char32_t pop_front_char();
    char32_t pop_back_char();
    String pop_front_grapheme();
    String pop_back_grapheme();

public:
    std::size_t replace_by_value(char old_val, char new_val, std::size_t count = 0, Direction dir = Direction::All) {
        return replace_if([old_val](char c) { return c == old_val; }, new_val, count, dir);
    }

    template <typename Pred>
    std::size_t replace_if(Pred pred, char new_val, std::size_t count = 0, Direction dir = Direction::All) {
        collections::DynamicArray<std::size_t> idx = collect_indices_if(pred);
        std::size_t first = 0;
        std::size_t last  = idx.size();
        if (count != 0 && count < idx.size()) {
            if (dir == Direction::Front) last = count;
            else if (dir == Direction::Back) first = idx.size() - count;
        }
        for (std::size_t i = first; i < last; ++i) m_ptr[idx[i]] = new_val;
        return last - first;
    }

    std::size_t replace_str(StringView old_sv, StringView new_sv, std::size_t max_count = 0, Direction dir = Direction::All);

    std::size_t replace_str(const char* old_s, const char* new_s, std::size_t max_count = 0, Direction dir = Direction::All) {
        return replace_str(StringView(old_s), StringView(new_s), max_count, dir);
    }

public:
    void reverse();
    void reverse(std::size_t start, std::size_t end_idx);
    String reversed() const { String r(*this); r.reverse(); return r; }

    void sort();
    void sort(std::size_t start, std::size_t end_idx);
    void stable_sort() { sort(); }

    template <typename Compare>
    void sort(Compare comp) { sort_code_points(0, m_size, comp, false); }

    template <typename Compare>
    void sort(std::size_t start, std::size_t end_idx, Compare comp) { sort_code_points(start, end_idx, comp, false); }

    template <typename Compare>
    void stable_sort(Compare comp) { sort_code_points(0, m_size, comp, true); }

private:
    template <typename Compare>
    void sort_code_points(std::size_t start, std::size_t end_idx, Compare comp, bool stable) {
        if (end_idx > m_size) end_idx = m_size;
        start   = floor_grapheme(start);
        end_idx = ceil_grapheme(end_idx);
        if (start >= end_idx) return;
        std::u32string cps;
        std::size_t pos = start;
        while (pos < end_idx) cps.push_back(unicode::decode_utf8(m_ptr, m_size, pos));
        if (stable) std::stable_sort(cps.begin(), cps.end(), comp);
        else        std::sort(cps.begin(), cps.end(), comp);
        replace(start, end_idx - start, from_code_points(cps.data(), cps.size()));
    }

public:
    void unique();

    template <typename Pred>
    void unique(Pred pred) {
        if constexpr (!std::is_invocable<Pred&, StringView, StringView>::value) {
            std::size_t write = 0;
            for (std::size_t read = 0; read < m_size; ++read) {
                if (write == 0 || !pred(m_ptr[write - 1], m_ptr[read])) m_ptr[write++] = m_ptr[read];
            }
            set_size(write);
        } else {
            collections::DynamicArray<StringView> parts = grapheme_list();
            collections::DynamicArray<StringView> kept;
            for (std::size_t i = 0; i < parts.size(); ++i) {
                if (kept.empty() || !pred(kept.back(), parts[i])) kept.push_back(parts[i]);
            }
            rebuild_from(kept);
        }
    }

    void deduplicate();
    String distinct() const;

public:
    void rotate_left(std::size_t n = 1);
    void rotate_right(std::size_t n = 1);
    void shuffle();
    void shuffle(std::size_t start, std::size_t end_idx);
    void unsecure_shuffle();
    void unsecure_shuffle(std::size_t start, std::size_t end_idx);

public:
    collections::DynamicArray<String> split(char delim, bool keep_empty = true) const;
    collections::DynamicArray<String> split(StringView delim, bool keep_empty = true) const;
    collections::DynamicArray<String> split(const char* delim, bool keep_empty = true) const { return split(StringView(delim), keep_empty); }
    collections::DynamicArray<String> split_whitespace() const;

    std::pair<String, String> split_at(std::size_t pos) const {
        if (pos >= m_size) return { *this, String() };
        return { substring(0, pos), substring(pos) };
    }

    collections::DynamicArray<String> split_n(StringView delim, std::size_t max_parts) const;
    collections::DynamicArray<String> rsplit(StringView delim, std::size_t n) const;
    collections::DynamicArray<String> lines(bool keep_empty = true) const;
    collections::DynamicArray<String> chunks(std::size_t n) const;

    static String join(const collections::DynamicArray<String>& parts, StringView sep);
    static String join(const collections::DynamicArray<String>& parts, char sep) { return join(parts, StringView(&sep, 1)); }
    static String join(const collections::DynamicArray<String>& parts, const char* sep) { return join(parts, StringView(sep)); }
    static String join(const collections::DynamicArray<StringView>& parts, StringView sep);
    static String join(const collections::DynamicArray<StringView>& parts, char sep) { return join(parts, StringView(&sep, 1)); }
    static String join(const collections::DynamicArray<StringView>& parts, const char* sep) { return join(parts, StringView(sep)); }

public:
    String repeated(std::size_t n) const;
    String padded_left(std::size_t total_width, char pad = ' ') const { return padded_left(total_width, static_cast<char32_t>(static_cast<unsigned char>(pad))); }
    String padded_right(std::size_t total_width, char pad = ' ') const { return padded_right(total_width, static_cast<char32_t>(static_cast<unsigned char>(pad))); }
    String padded_center(std::size_t total_width, char pad = ' ') const { return padded_center(total_width, static_cast<char32_t>(static_cast<unsigned char>(pad))); }
    String padded_left(std::size_t total_width, char32_t pad) const;
    String padded_right(std::size_t total_width, char32_t pad) const;
    String padded_center(std::size_t total_width, char32_t pad) const;
    String zfill(std::size_t total_width) const;

public:
    template <typename Func> String& for_each(Func func) { for (std::size_t i = 0; i < m_size; ++i) func(m_ptr[i]); return *this; }
    template <typename Func> const String& for_each(Func func) const { for (std::size_t i = 0; i < m_size; ++i) func(m_ptr[i]); return *this; }
    template <typename Func> String& for_each_indexed(Func func) { for (std::size_t i = 0; i < m_size; ++i) func(i, m_ptr[i]); return *this; }
    template <typename Func> const String& for_each_indexed(Func func) const { for (std::size_t i = 0; i < m_size; ++i) func(i, m_ptr[i]); return *this; }

    template <typename Func>
    const String& for_each_code_point(Func func) const {
        std::size_t pos = 0;
        while (pos < m_size) func(unicode::decode_utf8(m_ptr, m_size, pos));
        return *this;
    }

    template <typename Func>
    const String& for_each_grapheme(Func func) const {
        for (StringView g : graphemes()) func(g);
        return *this;
    }

public:
    template <typename Pred>
    String filter(Pred pred) const {
        String result;
        for (std::size_t i = 0; i < m_size; ++i) {
            if (pred(m_ptr[i])) result.push_back(m_ptr[i]);
        }
        return result;
    }

    template <typename Pred>
    std::pair<String, String> partition(Pred pred) const {
        String matching;
        String non_matching;
        for (std::size_t i = 0; i < m_size; ++i) {
            if (pred(m_ptr[i])) matching.push_back(m_ptr[i]);
            else                non_matching.push_back(m_ptr[i]);
        }
        return { std::move(matching), std::move(non_matching) };
    }

    template <typename U, typename Func>
    U reduce(U init, Func func) const {
        for (std::size_t i = 0; i < m_size; ++i) init = func(std::move(init), m_ptr[i]);
        return init;
    }

public:
    inline StringWhereProxy where(char ch);
    template <typename Pred> inline StringWhereProxy where(Pred pred);
};

template <typename Pred>
std::size_t String::erase_if_impl(Pred pred, std::size_t count, Direction dir) {
    collections::DynamicArray<std::size_t> idx = collect_indices_if(pred);
    std::size_t first = 0;
    std::size_t last  = idx.size();
    if (count != 0 && count < idx.size()) {
        if (dir == Direction::Front) last = count;
        else if (dir == Direction::Back) first = idx.size() - count;
    }
    if (first == last) return 0;
    std::size_t write = idx[first];
    std::size_t k     = first;
    for (std::size_t read = idx[first]; read < m_size; ++read) {
        if (k < last && idx[k] == read) {
            ++k;
            continue;
        }
        m_ptr[write++] = m_ptr[read];
    }
    set_size(write);
    return last - first;
}

inline String StringView::to_string() const { return String(m_data, m_size); }
inline String StringView::to_owned() const { return String(m_data, m_size); }

inline namespace literals {
inline namespace string_literals {

inline String operator""_s(const char* s, std::size_t n) { return String(s, n); }

} // namespace string_literals
} // namespace literals

class StringWhereProxy {
    friend class String;
    String&                   m_str;
    collections::DynamicArray<std::size_t> m_indices;

    StringWhereProxy(String& str, collections::DynamicArray<std::size_t>&& indices) : m_str(str), m_indices(std::move(indices)) {}

    void erase_indices();

public:
    StringWhereProxy& first(std::size_t n = 1) {
        if (n < m_indices.size()) m_indices.resize(n);
        return *this;
    }

    StringWhereProxy& last(std::size_t n = 1);
    StringWhereProxy& range(std::size_t start, std::size_t end_idx);
    StringWhereProxy& skip(std::size_t n = 1);
    StringWhereProxy& at(std::size_t n);
    StringWhereProxy& slice(std::size_t start, std::size_t end_idx);
    StringWhereProxy& even();
    StringWhereProxy& odd();
    StringWhereProxy& step(std::size_t n);
    StringWhereProxy& except(const collections::DynamicArray<std::size_t>& positions);

    template <typename Pred>
    StringWhereProxy& where(Pred pred) {
        std::size_t write = 0;
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            if (pred(m_str.m_ptr[m_indices[i]])) m_indices[write++] = m_indices[i];
        }
        m_indices.resize(write);
        return *this;
    }

    StringWhereProxy& invert();
    StringWhereProxy& unique();

    std::size_t count() const noexcept { return m_indices.size(); }
    bool empty() const noexcept { return m_indices.empty(); }
    bool any() const noexcept { return !m_indices.empty(); }
    collections::ArrayView<std::size_t> indices() const noexcept { return collections::ArrayView<std::size_t>(m_indices.data(), m_indices.size()); }
    collections::DynamicArray<std::size_t> indices_copy() const { return collections::DynamicArray<std::size_t>(m_indices); }

    collections::DynamicArray<std::size_t> release_indices() noexcept {
        collections::DynamicArray<std::size_t> out = std::move(m_indices);
        m_indices.deallocate();
        return out;
    }

    collections::DynamicArray<std::size_t> extract_indices() noexcept {
        collections::DynamicArray<std::size_t> out = std::move(m_indices);
        m_indices.clear();
        return out;
    }

    bool contains(char ch) const noexcept;

    template <typename Pred>
    std::size_t count_if(Pred pred) const noexcept {
        std::size_t n = 0;
        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            if (pred(m_str.m_ptr[m_indices[i]])) ++n;
        }
        return n;
    }

    String values() const;
    String as_string() const { return values(); }

    char* first_value() noexcept { return m_indices.empty() ? nullptr : m_str.m_ptr + m_indices.front(); }
    const char* first_value() const noexcept { return m_indices.empty() ? nullptr : m_str.m_ptr + m_indices.front(); }
    char* last_value() noexcept { return m_indices.empty() ? nullptr : m_str.m_ptr + m_indices.back(); }
    const char* last_value() const noexcept { return m_indices.empty() ? nullptr : m_str.m_ptr + m_indices.back(); }

    std::size_t remove();
    std::size_t remove_unordered();
    String pop();
    String pop_unordered();

    void replace(char value) {
        for (std::size_t i = 0; i < m_indices.size(); ++i) m_str.m_ptr[m_indices[i]] = value;
    }

    template <typename Func>
    StringWhereProxy& replace_with(Func func) {
        for (std::size_t i = 0; i < m_indices.size(); ++i) m_str.m_ptr[m_indices[i]] = func(m_str.m_ptr[m_indices[i]]);
        return *this;
    }

    template <typename Func>
    StringWhereProxy& for_each(Func func) {
        for (std::size_t i = 0; i < m_indices.size(); ++i) func(m_str.m_ptr[m_indices[i]]);
        return *this;
    }

    template <typename Func>
    StringWhereProxy& transform(Func func) { return for_each(func); }

    StringWhereProxy& sort();

    template <typename Compare>
    StringWhereProxy& sort(Compare comp) {
        const std::size_t n = m_indices.size();
        if (n <= 1) return *this;
        collections::DynamicArray<char> tmp;
        tmp.reserve(n);
        for (std::size_t i = 0; i < n; ++i) tmp.push_back(m_str.m_ptr[m_indices[i]]);
        std::sort(tmp.data(), tmp.data() + n, comp);
        for (std::size_t i = 0; i < n; ++i) m_str.m_ptr[m_indices[i]] = tmp[i];
        return *this;
    }

    StringWhereProxy& reverse();
    StringWhereProxy& shuffle();
    StringWhereProxy& to_upper();
    StringWhereProxy& to_lower();
    String uppercased() const;
    String lowercased() const;
    void copy_to(String& dest) const;

    void copy_to(collections::DynamicArray<char>& dest) const {
        for (std::size_t i = 0; i < m_indices.size(); ++i) dest.push_back(m_str.m_ptr[m_indices[i]]);
    }

    void move_to(String& dest);
    void insert_before(char value);
    void insert_after(char value);
    String swap_with(char value);
};

inline StringWhereProxy String::where(char ch) {
    return StringWhereProxy(*this, find_all(ch));
}

template <typename Pred>
inline StringWhereProxy String::where(Pred pred) {
    return StringWhereProxy(*this, collect_indices_if(pred));
}

} // namespace fizmo

namespace std {
template <>
struct hash<fizmo::String> {
    std::size_t operator()(const fizmo::String& s) const noexcept { return s.hash(); }
};
}

#endif // FIZMO_CUSTOM_STRING_CLASS_HPP
