#ifndef FIZMO_COLLECTIONS_RANGE_HPP
#define FIZMO_COLLECTIONS_RANGE_HPP

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <vector>

#include "hash.hpp"
#include "iterator.hpp"

namespace fizmo {
namespace collections {

class Range {
public:
    using value_type = std::int64_t;
    using size_type  = std::size_t;

    using iterator               = SequenceIterator<std::int64_t, std::int64_t>;
    using const_iterator         = iterator;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = reverse_iterator;

    Range() = default;
    explicit Range(std::int64_t stop) : Range(0, stop, 1) {}

    Range(std::int64_t start, std::int64_t stop, std::int64_t step = 1) : m_start(start), m_stop(stop), m_step(step) {
        if (step == 0) throw std::invalid_argument("Range step cannot be zero");
        m_size = compute_size(start, stop, step);
    }

    std::int64_t start() const noexcept { return m_start; }
    std::int64_t stop() const noexcept { return m_stop; }
    std::int64_t step() const noexcept { return m_step; }
    size_type size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }
    explicit operator bool() const noexcept { return m_size != 0; }

    std::int64_t operator[](size_type i) const noexcept { return m_start + m_step * static_cast<std::int64_t>(i); }

    std::int64_t at(std::ptrdiff_t i) const {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(m_size);
        if (i < 0) i += n;
        if (i < 0 || i >= n) throw std::out_of_range("Range index out of range");
        return (*this)[static_cast<size_type>(i)];
    }

    std::int64_t front() const { return at(0); }
    std::int64_t back() const { return at(-1); }

    iterator begin() const noexcept { return iterator(m_start, m_step); }
    iterator end() const noexcept { return iterator(m_start + m_step * static_cast<std::int64_t>(m_size), m_step); }
    reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }

    bool contains(std::int64_t v) const noexcept {
        if (m_size == 0) return false;
        if (m_step > 0 ? (v < m_start || v >= m_stop) : (v > m_start || v <= m_stop)) return false;
        return (v - m_start) % m_step == 0;
    }

    size_type count(std::int64_t v) const noexcept { return contains(v) ? 1 : 0; }

    size_type index(std::int64_t v) const {
        if (!contains(v)) throw std::out_of_range("Range::index: value not in range");
        return static_cast<size_type>((v - m_start) / m_step);
    }

    Range reversed() const noexcept {
        if (m_size == 0) return Range(0, 0, 1);
        const std::int64_t last = (*this)[m_size - 1];
        return Range(last, m_start - m_step, -m_step);
    }

    Range slice(std::ptrdiff_t start, std::ptrdiff_t stop, std::ptrdiff_t step = 1) const {
        if (step == 0) throw std::invalid_argument("Range::slice: step cannot be zero");
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(m_size);
        const auto adjust = [n, step](std::ptrdiff_t v) {
            if (v < 0) {
                v += n;
                if (v < 0) v = step < 0 ? -1 : 0;
            } else if (v >= n) {
                v = step < 0 ? n - 1 : n;
            }

            return v;
        };

        const std::ptrdiff_t a = adjust(start);
        const std::ptrdiff_t b = adjust(stop);
        return Range(m_start + m_step * a, m_start + m_step * b, m_step * step);
    }

    std::vector<std::int64_t> to_vector() const { return std::vector<std::int64_t>(begin(), end()); }

    friend bool operator==(const Range& a, const Range& b) noexcept {
        if (a.m_size != b.m_size) return false;
        if (a.m_size == 0) return true;
        if (a.m_start != b.m_start) return false;
        return a.m_size == 1 || a.m_step == b.m_step;
    }

    friend bool operator!=(const Range& a, const Range& b) noexcept { return !(a == b); }

private:
    std::int64_t m_start = 0;
    std::int64_t m_stop = 0;
    std::int64_t m_step = 1;
    size_type    m_size = 0;

    static size_type compute_size(std::int64_t start, std::int64_t stop, std::int64_t step) noexcept {
        if (step > 0 && start < stop) return static_cast<size_type>((static_cast<std::uint64_t>(stop) - static_cast<std::uint64_t>(start) - 1) / static_cast<std::uint64_t>(step) + 1);
        if (step < 0 && start > stop) return static_cast<size_type>((static_cast<std::uint64_t>(start) - static_cast<std::uint64_t>(stop) - 1) / (0 - static_cast<std::uint64_t>(step)) + 1);
        return 0;
    }
};

inline Range range(std::int64_t stop) { return Range(stop); }
inline Range range(std::int64_t start, std::int64_t stop, std::int64_t step = 1) { return Range(start, stop, step); }

template <>
struct Hash<Range> {
    std::size_t operator()(const Range& r) const noexcept {
        if (r.empty()) return 0;
        return hash_combine(hash_combine(r.size(), static_cast<std::size_t>(r.start())), r.size() == 1 ? 0 : static_cast<std::size_t>(r.step()));
    }
};

} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_RANGE_HPP
