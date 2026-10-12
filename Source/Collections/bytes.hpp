#ifndef FIZMO_COLLECTIONS_BYTES_HPP
#define FIZMO_COLLECTIONS_BYTES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "hash.hpp"
#include "iterator.hpp"

namespace fizmo {
namespace collections {

class Bytes;
class ByteArray;

namespace bytes_detail {

constexpr std::size_t npos = static_cast<std::size_t>(-1);

inline bool is_space(std::uint8_t c) noexcept { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }

inline std::size_t find(const std::uint8_t* h, std::size_t hn, const std::uint8_t* n, std::size_t nn, std::size_t start) noexcept {
    if (start > hn) return npos;
    if (nn == 0) return start;
    if (nn > hn - start) return npos;
    const std::uint8_t* p = std::search(h + start, h + hn, n, n + nn);
    return p == h + hn ? npos : static_cast<std::size_t>(p - h);
}

inline std::size_t rfind(const std::uint8_t* h, std::size_t hn, const std::uint8_t* n, std::size_t nn) noexcept {
    if (nn > hn) return npos;
    if (nn == 0) return hn;
    const std::uint8_t* p = std::find_end(h, h + hn, n, n + nn);
    return p == h + hn ? npos : static_cast<std::size_t>(p - h);
}

inline int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline std::vector<std::uint8_t> parse_hex(std::string_view s) {
    std::vector<std::uint8_t> out;
    int hi = -1;

    for (char c : s) {
        if (c == ' ' || c == ':' || c == '-' || c == '\n' || c == '\t') {
            if (hi >= 0) throw std::invalid_argument("from_hex: odd digit count before separator");
            continue;
        }

        const int v = hex_value(c);
        if (v < 0) throw std::invalid_argument("from_hex: invalid hexadecimal digit");

        if (hi < 0) {
            hi = v;
        } else {
            out.push_back(static_cast<std::uint8_t>(hi * 16 + v));
            hi = -1;
        }
    }

    if (hi >= 0) throw std::invalid_argument("from_hex: odd number of digits");
    return out;
}

inline std::string to_hex(const std::uint8_t* p, std::size_t n, std::string_view sep, bool upper) {
    static const char lo[] = "0123456789abcdef";
    static const char up[] = "0123456789ABCDEF";
    const char* d = upper ? up : lo;
    std::string out;
    out.reserve(n * (2 + sep.size()));

    for (std::size_t i = 0; i < n; ++i) {
        if (i && !sep.empty()) out.append(sep.data(), sep.size());
        out.push_back(d[p[i] >> 4]);
        out.push_back(d[p[i] & 15]);
    }

    return out;
}

inline std::string to_base64(const std::uint8_t* p, std::size_t n, bool url_safe) {
    const char* a = url_safe ? "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_" : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((n + 2) / 3 * 4);
    std::size_t i = 0;

    for (; i + 2 < n; i += 3) {
        const std::uint32_t v = (std::uint32_t(p[i]) << 16) | (std::uint32_t(p[i + 1]) << 8) | p[i + 2];
        out.push_back(a[(v >> 18) & 63]);
        out.push_back(a[(v >> 12) & 63]);
        out.push_back(a[(v >> 6) & 63]);
        out.push_back(a[v & 63]);
    }

    if (i < n) {
        std::uint32_t v = std::uint32_t(p[i]) << 16;
        if (i + 1 < n) v |= std::uint32_t(p[i + 1]) << 8;
        out.push_back(a[(v >> 18) & 63]);
        out.push_back(a[(v >> 12) & 63]);
        out.push_back(i + 1 < n ? a[(v >> 6) & 63] : '=');
        out.push_back('=');
    }

    return out;
}

inline std::vector<std::uint8_t> parse_base64(std::string_view s) {
    std::vector<std::uint8_t> out;
    std::uint32_t acc = 0;
    int bits = 0;

    for (char c : s) {
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+' || c == '-') v = 62;
        else if (c == '/' || c == '_') v = 63;
        else if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        else throw std::invalid_argument("from_base64: invalid character");

        acc = (acc << 6) | static_cast<std::uint32_t>(v);
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((acc >> bits) & 0xFF));
        }
    }

    return out;
}

template <class Derived>
class ByteOps {
public:
    using value_type = std::uint8_t;
    using size_type  = std::size_t;
    using const_iterator         = ConstArrayIterator<std::uint8_t>;
    using const_reverse_iterator = ConstReverseArrayIterator<std::uint8_t>;
    static constexpr size_type npos = bytes_detail::npos;

    const_iterator begin() const noexcept { return const_iterator(self().data()); }
    const_iterator end() const noexcept { return const_iterator(self().data() + self().size()); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(self().data() + self().size()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(self().data()); }
    bool empty() const noexcept { return self().size() == 0; }
    explicit operator bool() const noexcept { return !empty(); }

    std::uint8_t at(std::ptrdiff_t i) const { return self().data()[resolve(i)]; }

    std::string_view view() const noexcept { return std::string_view(reinterpret_cast<const char*>(self().data()), self().size()); }
    std::string to_string() const { return std::string(view()); }
    std::string decode() const { return to_string(); }
    std::vector<std::uint8_t> to_vector() const { return std::vector<std::uint8_t>(begin(), end()); }

    std::string hex(std::string_view sep = {}, bool upper = false) const { return to_hex(self().data(), self().size(), sep, upper); }
    std::string base64(bool url_safe = false) const { return to_base64(self().data(), self().size(), url_safe); }

    template <class B, typename std::enable_if<!std::is_arithmetic<B>::value, int>::type = 0>
    size_type find(const B& sub, size_type start = 0) const {
        const auto v = as_view(sub);
        return bytes_detail::find(self().data(), self().size(), v.data(), v.size(), start);
    }

    size_type find(std::uint8_t b, size_type start = 0) const { return bytes_detail::find(self().data(), self().size(), &b, 1, start); }

    template <class B, typename std::enable_if<!std::is_arithmetic<B>::value, int>::type = 0>
    size_type rfind(const B& sub) const {
        const auto v = as_view(sub);
        return bytes_detail::rfind(self().data(), self().size(), v.data(), v.size());
    }

    template <class B>
    size_type index(const B& sub, size_type start = 0) const {
        const size_type i = find(sub, start);
        if (i == npos) throw std::out_of_range("subsequence not found");
        return i;
    }

    template <class B>
    bool contains(const B& sub) const { return find(sub) != npos; }

    template <class B>
    size_type count(const B& sub) const {
        const auto s = as_view(sub);
        if (s.size() == 0) return self().size() + 1;
        size_type n = 0;

        for (size_type i = find(s); i != npos; i = find(s, i + s.size())) ++n;
        return n;
    }

    size_type count(std::uint8_t b) const { return static_cast<size_type>(std::count(begin(), end(), b)); }

    template <class B>
    bool starts_with(const B& p) const {
        const auto s = as_view(p);
        return s.size() <= self().size() && std::equal(s.begin(), s.end(), begin());
    }

    template <class B>
    bool ends_with(const B& p) const {
        const auto s = as_view(p);
        return s.size() <= self().size() && std::equal(s.begin(), s.end(), end() - s.size());
    }

    bool is_ascii() const noexcept { return std::all_of(begin(), end(), [](std::uint8_t c) { return c < 128; }); }
    bool is_digit() const noexcept { return !empty() && std::all_of(begin(), end(), [](std::uint8_t c) { return c >= '0' && c <= '9'; }); }
    bool is_alpha() const noexcept { return !empty() && std::all_of(begin(), end(), [](std::uint8_t c) { return (c | 32) >= 'a' && (c | 32) <= 'z'; }); }
    bool is_space() const noexcept { return !empty() && std::all_of(begin(), end(), bytes_detail::is_space); }

    Derived slice(std::ptrdiff_t start, std::ptrdiff_t stop) const {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(self().size());
        if (start < 0) start = std::max<std::ptrdiff_t>(0, start + n);
        if (stop < 0) stop = std::max<std::ptrdiff_t>(0, stop + n);
        start = std::min(start, n);
        stop = std::min(stop, n);
        if (stop < start) stop = start;
        return self().sub(static_cast<size_type>(start), static_cast<size_type>(stop - start));
    }

    Derived slice(std::ptrdiff_t start) const { return slice(start, static_cast<std::ptrdiff_t>(self().size())); }

    Derived upper() const { return mapped([](std::uint8_t c) { return (c >= 'a' && c <= 'z') ? static_cast<std::uint8_t>(c - 32) : c; }); }
    Derived lower() const { return mapped([](std::uint8_t c) { return (c >= 'A' && c <= 'Z') ? static_cast<std::uint8_t>(c + 32) : c; }); }

    Derived strip() const { return lstrip().rstrip(); }

    Derived lstrip() const {
        size_type i = 0;
        while (i < self().size() && bytes_detail::is_space(self().data()[i])) ++i;
        return self().sub(i, self().size() - i);
    }

    Derived rstrip() const {
        size_type n = self().size();
        while (n > 0 && bytes_detail::is_space(self().data()[n - 1])) --n;
        return self().sub(0, n);
    }

    template <class B>
    Derived strip(const B& chars) const {
        const auto c = as_view(chars);
        size_type a = 0;
        size_type b = self().size();
        while (a < b && std::find(c.begin(), c.end(), self().data()[a]) != c.end()) ++a;
        while (b > a && std::find(c.begin(), c.end(), self().data()[b - 1]) != c.end()) --b;
        return self().sub(a, b - a);
    }

    template <class B>
    std::vector<Derived> split(const B& sep, std::ptrdiff_t maxsplit = -1) const {
        const auto s = as_view(sep);
        if (s.size() == 0) throw std::invalid_argument("split: empty separator");
        std::vector<Derived> out;
        size_type pos = 0;

        for (;;) {
            const size_type i = (maxsplit >= 0 && static_cast<std::ptrdiff_t>(out.size()) >= maxsplit) ? npos : find(s, pos);

            if (i == npos) {
                out.push_back(self().sub(pos, self().size() - pos));
                return out;
            }

            out.push_back(self().sub(pos, i - pos));
            pos = i + s.size();
        }
    }

    std::vector<Derived> split() const {
        std::vector<Derived> out;
        size_type i = 0;
        const size_type n = self().size();

        while (i < n) {
            while (i < n && bytes_detail::is_space(self().data()[i])) ++i;
            if (i >= n) break;
            const size_type j0 = i;
            while (i < n && !bytes_detail::is_space(self().data()[i])) ++i;
            out.push_back(self().sub(j0, i - j0));
        }

        return out;
    }

    std::vector<Derived> splitlines() const {
        std::vector<Derived> out;
        size_type a = 0;
        const size_type n = self().size();

        for (size_type i = 0; i < n; ++i) {
            const std::uint8_t c = self().data()[i];

            if (c == '\n' || c == '\r') {
                out.push_back(self().sub(a, i - a));
                if (c == '\r' && i + 1 < n && self().data()[i + 1] == '\n') ++i;
                a = i + 1;
            }
        }

        if (a < n) out.push_back(self().sub(a, n - a));
        return out;
    }

    template <class Range>
    Derived join(const Range& parts) const {
        std::vector<std::uint8_t> out;
        bool first = true;

        for (const auto& p : parts) {
            if (!first) out.insert(out.end(), begin(), end());
            const auto v = as_view(p);
            out.insert(out.end(), v.begin(), v.end());
            first = false;
        }

        return Derived(std::move(out));
    }

    template <class A, class B>
    Derived replace(const A& old_bytes, const B& new_bytes, std::ptrdiff_t max_count = -1) const {
        const auto o = as_view(old_bytes);
        const auto n = as_view(new_bytes);
        std::vector<std::uint8_t> out;
        size_type pos = 0;
        std::ptrdiff_t done = 0;

        if (o.size() == 0) {
            for (size_type i = 0; i <= self().size(); ++i) {
                if (max_count < 0 || done < max_count) {
                    out.insert(out.end(), n.begin(), n.end());
                    ++done;
                }

                if (i < self().size()) out.push_back(self().data()[i]);
            }

            return Derived(std::move(out));
        }

        for (;;) {
            const size_type i = (max_count >= 0 && done >= max_count) ? npos : find(o, pos);

            if (i == npos) {
                out.insert(out.end(), begin() + pos, end());
                return Derived(std::move(out));
            }

            out.insert(out.end(), begin() + pos, begin() + i);
            out.insert(out.end(), n.begin(), n.end());
            pos = i + o.size();
            ++done;
        }
    }

    template <class B>
    std::vector<Derived> partition(const B& sep) const {
        const auto s = as_view(sep);
        const size_type i = find(s);
        if (i == npos) return { self().sub(0, self().size()), Derived(), Derived() };
        return { self().sub(0, i), self().sub(i, s.size()), self().sub(i + s.size(), self().size() - i - s.size()) };
    }

    friend bool operator==(const Derived& a, const Derived& b) noexcept { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()); }
    friend bool operator!=(const Derived& a, const Derived& b) noexcept { return !(a == b); }
    friend bool operator<(const Derived& a, const Derived& b) noexcept { return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()); }
    friend bool operator>(const Derived& a, const Derived& b) noexcept { return b < a; }
    friend bool operator<=(const Derived& a, const Derived& b) noexcept { return !(b < a); }
    friend bool operator>=(const Derived& a, const Derived& b) noexcept { return !(a < b); }

protected:
    struct View {
        const std::uint8_t* p;
        std::size_t         n;
        const std::uint8_t* begin() const noexcept { return p; }
        const std::uint8_t* end() const noexcept { return p + n; }
        const std::uint8_t* data() const noexcept { return p; }
        std::size_t size() const noexcept { return n; }
    };

    static View as_view(const View& v) noexcept { return v; }
    static View as_view(std::string_view s) noexcept { return View{ reinterpret_cast<const std::uint8_t*>(s.data()), s.size() }; }
    static View as_view(const char* s) noexcept { return as_view(std::string_view(s)); }
    static View as_view(const std::string& s) noexcept { return as_view(std::string_view(s)); }
    static View as_view(const std::vector<std::uint8_t>& v) noexcept { return View{ v.data(), v.size() }; }

    template <class B, typename std::enable_if<std::is_base_of<ByteOps<B>, B>::value, int>::type = 0>
    static View as_view(const B& b) noexcept { return View{ b.data(), b.size() }; }

private:
    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }

    std::size_t resolve(std::ptrdiff_t i) const {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(self().size());
        if (i < 0) i += n;
        if (i < 0 || i >= n) throw std::out_of_range("byte index out of range");
        return static_cast<std::size_t>(i);
    }

    template <class F>
    Derived mapped(F f) const {
        std::vector<std::uint8_t> out(begin(), end());
        for (auto& c : out) c = f(c);
        return Derived(std::move(out));
    }
};

} // namespace bytes_detail

class Bytes : public bytes_detail::ByteOps<Bytes> {
public:
    Bytes() = default;
    Bytes(std::initializer_list<std::uint8_t> init) : Bytes(std::vector<std::uint8_t>(init)) {}
    explicit Bytes(std::vector<std::uint8_t> v) : m_data(std::make_shared<const std::vector<std::uint8_t>>(std::move(v))), m_len(m_data->size()) {}
    Bytes(const void* p, std::size_t n) : Bytes(std::vector<std::uint8_t>(static_cast<const std::uint8_t*>(p), static_cast<const std::uint8_t*>(p) + n)) {}
    explicit Bytes(std::string_view s) : Bytes(s.data(), s.size()) {}
    explicit Bytes(const char* s) : Bytes(std::string_view(s)) {}
    explicit Bytes(const std::string& s) : Bytes(std::string_view(s)) {}
    explicit Bytes(std::size_t n, std::uint8_t fill = 0) : Bytes(std::vector<std::uint8_t>(n, fill)) {}
    explicit Bytes(const ByteArray& a);

    template <class It, typename std::enable_if<!std::is_integral<It>::value && !std::is_pointer<It>::value, int>::type = 0>
    Bytes(It first, It last) : Bytes(std::vector<std::uint8_t>(first, last)) {}

    static Bytes from_hex(std::string_view s) { return Bytes(bytes_detail::parse_hex(s)); }
    static Bytes from_base64(std::string_view s) { return Bytes(bytes_detail::parse_base64(s)); }

    const std::uint8_t* data() const noexcept { return m_data ? m_data->data() + m_off : nullptr; }
    std::size_t size() const noexcept { return m_len; }
    std::uint8_t operator[](std::size_t i) const noexcept { return data()[i]; }

    Bytes sub(std::size_t off, std::size_t len) const {
        Bytes r = *this;
        r.m_off = m_off + off;
        r.m_len = len;
        return r;
    }

    Bytes compact() const { return Bytes(to_vector()); }

    friend Bytes operator+(const Bytes& a, const Bytes& b) {
        std::vector<std::uint8_t> v(a.begin(), a.end());
        v.insert(v.end(), b.begin(), b.end());
        return Bytes(std::move(v));
    }

    friend Bytes operator*(const Bytes& a, std::size_t n) {
        std::vector<std::uint8_t> v;
        v.reserve(a.size() * n);
        for (std::size_t i = 0; i < n; ++i) v.insert(v.end(), a.begin(), a.end());
        return Bytes(std::move(v));
    }

private:
    std::shared_ptr<const std::vector<std::uint8_t>> m_data;
    std::size_t                                      m_off = 0;
    std::size_t                                      m_len = 0;
};

class ByteArray : public bytes_detail::ByteOps<ByteArray> {
public:
    using iterator         = ArrayIterator<std::uint8_t>;
    using reverse_iterator = ReverseArrayIterator<std::uint8_t>;

    ByteArray() = default;
    ByteArray(std::initializer_list<std::uint8_t> init) : m_data(init) {}
    explicit ByteArray(std::vector<std::uint8_t> v) : m_data(std::move(v)) {}
    ByteArray(const void* p, std::size_t n) : m_data(static_cast<const std::uint8_t*>(p), static_cast<const std::uint8_t*>(p) + n) {}
    explicit ByteArray(std::string_view s) : ByteArray(s.data(), s.size()) {}
    explicit ByteArray(const char* s) : ByteArray(std::string_view(s)) {}
    explicit ByteArray(const std::string& s) : ByteArray(std::string_view(s)) {}
    explicit ByteArray(std::size_t n, std::uint8_t fill = 0) : m_data(n, fill) {}
    explicit ByteArray(const Bytes& b) : m_data(b.begin(), b.end()) {}

    template <class It, typename std::enable_if<!std::is_integral<It>::value && !std::is_pointer<It>::value, int>::type = 0>
    ByteArray(It first, It last) : m_data(first, last) {}

    static ByteArray from_hex(std::string_view s) { return ByteArray(bytes_detail::parse_hex(s)); }
    static ByteArray from_base64(std::string_view s) { return ByteArray(bytes_detail::parse_base64(s)); }

    const std::uint8_t* data() const noexcept { return m_data.data(); }
    std::uint8_t* data() noexcept { return m_data.data(); }
    std::size_t size() const noexcept { return m_data.size(); }
    std::size_t capacity() const noexcept { return m_data.capacity(); }

    using bytes_detail::ByteOps<ByteArray>::begin;
    using bytes_detail::ByteOps<ByteArray>::end;
    using bytes_detail::ByteOps<ByteArray>::rbegin;
    using bytes_detail::ByteOps<ByteArray>::rend;
    iterator begin() noexcept { return iterator(m_data.data()); }
    iterator end() noexcept { return iterator(m_data.data() + m_data.size()); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(m_data.data() + m_data.size()); }
    reverse_iterator rend() noexcept { return reverse_iterator(m_data.data()); }

    std::uint8_t& operator[](std::size_t i) noexcept { return m_data[i]; }
    std::uint8_t operator[](std::size_t i) const noexcept { return m_data[i]; }

    ByteArray sub(std::size_t off, std::size_t len) const { return ByteArray(data() + off, len); }

    void push_back(std::uint8_t b) { m_data.push_back(b); }
    void append(std::uint8_t b) { m_data.push_back(b); }

    template <class B>
    ByteArray& extend(const B& b) {
        const auto v = as_view(b);
        m_data.insert(m_data.end(), v.begin(), v.end());
        return *this;
    }

    void insert(std::size_t i, std::uint8_t b) { m_data.insert(m_data.begin() + static_cast<std::ptrdiff_t>(std::min(i, m_data.size())), b); }

    template <class B>
    void insert_bytes(std::size_t i, const B& b) {
        const auto v = as_view(b);
        m_data.insert(m_data.begin() + static_cast<std::ptrdiff_t>(std::min(i, m_data.size())), v.begin(), v.end());
    }

    void erase(std::size_t i, std::size_t n = 1) {
        if (i >= m_data.size()) return;
        n = std::min(n, m_data.size() - i);
        m_data.erase(m_data.begin() + static_cast<std::ptrdiff_t>(i), m_data.begin() + static_cast<std::ptrdiff_t>(i + n));
    }

    std::uint8_t pop() {
        if (m_data.empty()) throw std::out_of_range("ByteArray::pop: empty");
        const std::uint8_t b = m_data.back();
        m_data.pop_back();
        return b;
    }

    std::uint8_t pop(std::ptrdiff_t i) {
        const std::uint8_t b = at(i);
        erase(static_cast<std::size_t>(i < 0 ? i + static_cast<std::ptrdiff_t>(size()) : i));
        return b;
    }

    void remove(std::uint8_t b) {
        auto it = std::find(m_data.begin(), m_data.end(), b);
        if (it == m_data.end()) throw std::out_of_range("ByteArray::remove: value not present");
        m_data.erase(it);
    }

    void reverse() { std::reverse(m_data.begin(), m_data.end()); }
    void clear() noexcept { m_data.clear(); }
    void resize(std::size_t n, std::uint8_t fill = 0) { m_data.resize(n, fill); }
    void reserve(std::size_t n) { m_data.reserve(n); }
    void shrink_to_fit() { m_data.shrink_to_fit(); }

    std::vector<std::uint8_t>& vector() noexcept { return m_data; }
    const std::vector<std::uint8_t>& vector() const noexcept { return m_data; }
    std::vector<std::uint8_t> release() noexcept { return std::move(m_data); }

    template <class B>
    ByteArray& operator+=(const B& b) { return extend(b); }

    friend ByteArray operator+(ByteArray a, const ByteArray& b) {
        a.extend(b);
        return a;
    }

    friend ByteArray operator*(const ByteArray& a, std::size_t n) {
        ByteArray r;
        r.reserve(a.size() * n);
        for (std::size_t i = 0; i < n; ++i) r.extend(a);
        return r;
    }

private:
    std::vector<std::uint8_t> m_data;
};

inline Bytes::Bytes(const ByteArray& a) : Bytes(a.data(), a.size()) {}

template <>
struct Hash<Bytes> {
    std::size_t operator()(const Bytes& b) const noexcept { return hash_bytes(b.data(), b.size()); }
};

template <>
struct Hash<ByteArray> {
    std::size_t operator()(const ByteArray& b) const noexcept { return hash_bytes(b.data(), b.size()); }
};

} // namespace collections
} // namespace fizmo

namespace std {

template <>
struct hash<fizmo::collections::Bytes> {
    std::size_t operator()(const fizmo::collections::Bytes& b) const noexcept { return fizmo::collections::hash_bytes(b.data(), b.size()); }
};

} // namespace std

#endif // FIZMO_COLLECTIONS_BYTES_HPP
