#ifndef FIZMO_RANDOM_SHUFFLE_BAG_HPP
#define FIZMO_RANDOM_SHUFFLE_BAG_HPP

#include "random_std_int.hpp"

#include <cstddef>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace fizmo {
namespace detail {

template <typename T, typename = void>
struct is_equality_comparable : std::false_type {};

template <typename T>
struct is_equality_comparable<T, std::void_t<decltype(std::declval<const T&>() == std::declval<const T&>())>> : std::true_type {};

} // namespace detail

template <typename T>
class ShuffleBag {
private:
    std::vector<T> m_items;
    std::size_t m_cursor = 0;
    bool m_avoid_boundary_repeat = false;
    bool m_has_last = false;

    void swap_at(std::size_t i, std::size_t j) {
        using std::swap;
        swap(m_items[i], m_items[j]);
    }

    void shuffle_range(std::size_t first) {
        const std::size_t n = m_items.size();
        if (n - first < 2) return;

        for (std::size_t i = n - 1; i > first; --i) {
            const std::size_t j = random_int<std::size_t>(first, i);
            if (i != j) swap_at(i, j);
        }
    }

    void fix_boundary_repeat(const T& last) {
        if constexpr (detail::is_equality_comparable<T>::value) {
            if (m_items.size() < 2 || !(m_items[0] == last)) return;

            for (std::size_t tries = 0; tries < m_items.size(); ++tries) {
                const std::size_t j = random_int<std::size_t>(1, m_items.size() - 1);

                if (!(m_items[j] == last)) {
                    swap_at(0, j);
                    return;
                }
            }

            for (std::size_t j = 1; j < m_items.size(); ++j) {
                if (!(m_items[j] == last)) {
                    swap_at(0, j);
                    return;
                }
            }
        }
    }

public:
    ShuffleBag() = default;

    ShuffleBag(std::initializer_list<T> items) : m_items(items) {
        refill();
    }

    explicit ShuffleBag(std::vector<T> items) : m_items(std::move(items)) {
        refill();
    }

    void add(const T& value, std::size_t copies = 1) {
        for (std::size_t c = 0; c < copies; ++c) {
            m_items.push_back(value);
            const std::size_t j = random_int<std::size_t>(m_cursor, m_items.size() - 1);
            if (j != m_items.size() - 1) swap_at(j, m_items.size() - 1);
        }
    }

    T next() {
        if (m_items.empty()) throw std::out_of_range("ShuffleBag::next on empty bag");

        if (m_cursor >= m_items.size()) {
            const T last = m_items[m_cursor - 1];
            refill();
            if (m_avoid_boundary_repeat && m_has_last) fix_boundary_repeat(last);
        }

        m_has_last = true;
        return m_items[m_cursor++];
    }

    T operator()() { return next(); }

    void refill() {
        m_cursor = 0;
        shuffle_range(0);
    }

    void clear() noexcept {
        m_items.clear();
        m_cursor = 0;
        m_has_last = false;
    }

    void avoid_boundary_repeat(bool enabled) noexcept { m_avoid_boundary_repeat = enabled; }
    bool avoids_boundary_repeat() const noexcept { return m_avoid_boundary_repeat; }

    std::size_t size() const noexcept { return m_items.size(); }
    std::size_t remaining() const noexcept { return m_items.size() - m_cursor; }
    bool empty() const noexcept { return m_items.empty(); }

    const std::vector<T>& contents() const noexcept { return m_items; }
};

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && !std::is_same<typename std::remove_cv<T>::type, bool>::value>::type>
ShuffleBag<T> random_int_bag(T a, T b, std::size_t copies = 1) {
    if (a > b) { const T t = a; a = b; b = t; }
    if (copies == 0) throw std::invalid_argument("random_int_bag: copies must be at least 1");

    using U = typename std::make_unsigned<T>::type;
    const U span = static_cast<U>(static_cast<U>(b) - static_cast<U>(a));
    constexpr std::size_t max_items = std::size_t(1) << 26;

    if (static_cast<unsigned long long>(span) >= max_items || (static_cast<std::size_t>(span) + 1) > max_items / copies) {
        throw std::length_error("random_int_bag: range too large for a bag");
    }

    std::vector<T> items;
    items.reserve((static_cast<std::size_t>(span) + 1) * copies);

    for (U offset = 0;; ++offset) {
        const T value = static_cast<T>(static_cast<U>(static_cast<U>(a) + offset));
        for (std::size_t c = 0; c < copies; ++c) items.push_back(value);
        if (offset == span) break;
    }

    return ShuffleBag<T>(std::move(items));
}

} // namespace fizmo

#endif // FIZMO_RANDOM_SHUFFLE_BAG_HPP
