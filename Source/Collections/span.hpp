#ifndef FIZMO_COLLECTIONS_SPAN_HPP
#define FIZMO_COLLECTIONS_SPAN_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "iterator.hpp"

namespace fizmo {
namespace collections {

template <class T>
class Span {
public:
    using element_type    = T;
    using value_type      = typename std::remove_cv<T>::type;
    using size_type       = std::size_t;
    using pointer         = T*;
    using reference       = T&;
    using iterator         = ArrayIterator<T>;
    using reverse_iterator = ReverseArrayIterator<T>;

    constexpr Span() noexcept = default;
    constexpr Span(T* data, size_type size) noexcept : m_data(data), m_size(size) {}
    constexpr Span(T* first, T* last) noexcept : m_data(first), m_size(static_cast<size_type>(last - first)) {}

    template <std::size_t N>
    constexpr Span(T (&arr)[N]) noexcept : m_data(arr), m_size(N) {}

    template <class U, std::size_t N, typename std::enable_if<std::is_convertible<U (*)[], T (*)[]>::value, int>::type = 0>
    constexpr Span(std::array<U, N>& arr) noexcept : m_data(arr.data()), m_size(N) {}

    template <class U, std::size_t N, typename std::enable_if<std::is_convertible<const U (*)[], T (*)[]>::value, int>::type = 0>
    constexpr Span(const std::array<U, N>& arr) noexcept : m_data(arr.data()), m_size(N) {}

    template <class C, typename std::enable_if<
        !std::is_array<typename std::remove_reference<C>::type>::value &&
        !std::is_same<typename std::decay<C>::type, Span>::value &&
        std::is_convertible<typename std::remove_pointer<decltype(std::declval<C&>().data())>::type (*)[], T (*)[]>::value, int>::type = 0>
    constexpr Span(C& c) noexcept(noexcept(c.data())) : m_data(c.data()), m_size(static_cast<size_type>(c.size())) {}

    template <class U, typename std::enable_if<!std::is_same<U, T>::value && std::is_convertible<U (*)[], T (*)[]>::value, int>::type = 0>
    constexpr Span(const Span<U>& o) noexcept : m_data(o.data()), m_size(o.size()) {}

    constexpr T* data() const noexcept { return m_data; }
    constexpr size_type size() const noexcept { return m_size; }
    constexpr size_type size_bytes() const noexcept { return m_size * sizeof(T); }
    constexpr bool empty() const noexcept { return m_size == 0; }

    constexpr T& operator[](size_type i) const noexcept { return m_data[i]; }

    T& at(size_type i) const {
        if (i >= m_size) throw std::out_of_range("Span::at");
        return m_data[i];
    }

    T& front() const { return at(0); }
    T& back() const {
        if (m_size == 0) throw std::out_of_range("Span::back");
        return m_data[m_size - 1];
    }

    constexpr iterator begin() const noexcept { return iterator(m_data); }
    constexpr iterator end() const noexcept { return iterator(m_data + m_size); }
    constexpr reverse_iterator rbegin() const noexcept { return reverse_iterator(m_data + m_size); }
    constexpr reverse_iterator rend() const noexcept { return reverse_iterator(m_data); }

    Span first(size_type n) const {
        if (n > m_size) throw std::out_of_range("Span::first");
        return Span(m_data, n);
    }

    Span last(size_type n) const {
        if (n > m_size) throw std::out_of_range("Span::last");
        return Span(m_data + (m_size - n), n);
    }

    Span subspan(size_type offset, size_type count = static_cast<size_type>(-1)) const {
        if (offset > m_size) throw std::out_of_range("Span::subspan");
        if (count == static_cast<size_type>(-1) || count > m_size - offset) count = m_size - offset;
        return Span(m_data + offset, count);
    }

    Span<const std::uint8_t> as_bytes() const noexcept { return Span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(m_data), size_bytes()); }

private:
    T*        m_data = nullptr;
    size_type m_size = 0;
};

template <class T>
Span<T> make_span(T* p, std::size_t n) noexcept { return Span<T>(p, n); }

template <class C>
auto make_span(C& c) noexcept -> Span<typename std::remove_pointer<decltype(c.data())>::type> { return Span<typename std::remove_pointer<decltype(c.data())>::type>(c.data(), c.size()); }

} // namespace collections

template <class T>
using Span = collections::Span<T>;

} // namespace fizmo

#endif // FIZMO_COLLECTIONS_SPAN_HPP
