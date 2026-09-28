#ifndef FIZMO_ARRAY_ITERATORS_HPP
#define FIZMO_ARRAY_ITERATORS_HPP

#include "../Basic/fizmo_defines.hpp"   

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace fizmo {
namespace detail {

template <typename T, bool Reverse>
class BasicArrayIterator {
    template <typename, bool> friend class BasicArrayIterator;

    using mutable_type = typename std::remove_const<T>::type;

    template <typename U>
    using enable_const_conversion = typename std::enable_if<std::is_const<T>::value && std::is_same<U, mutable_type>::value>::type;

    static constexpr std::ptrdiff_t step(std::ptrdiff_t n) noexcept { return Reverse ? -n : n; }

public:
    using iterator_category = std::random_access_iterator_tag;

#if defined(__cpp_lib_ranges)
    using iterator_concept  = typename std::conditional<Reverse, std::random_access_iterator_tag, std::contiguous_iterator_tag>::type;
#endif

    using value_type        = mutable_type;
    using difference_type   = std::ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;

    constexpr BasicArrayIterator() noexcept : m_ptr(nullptr) {}
    constexpr explicit BasicArrayIterator(T* ptr) noexcept : m_ptr(ptr) {}

    template <typename U, typename = enable_const_conversion<U>>
    constexpr BasicArrayIterator(const BasicArrayIterator<U, Reverse>& other) noexcept : m_ptr(other.m_ptr) {}

    constexpr T* get() const noexcept { return m_ptr; }

    constexpr reference operator*()  const noexcept { return Reverse ? *(m_ptr - 1) : *m_ptr; }
    constexpr pointer   operator->() const noexcept { return Reverse ? m_ptr - 1 : m_ptr; }
    constexpr reference operator[](difference_type n) const noexcept { return m_ptr[Reverse ? -1 - n : n]; }

    OPTIONAL_CPP14_CONSTEXPR BasicArrayIterator& operator++() noexcept { m_ptr += step(1); return *this; }
    OPTIONAL_CPP14_CONSTEXPR BasicArrayIterator& operator--() noexcept { m_ptr -= step(1); return *this; }
    OPTIONAL_CPP14_CONSTEXPR BasicArrayIterator operator++(int) noexcept { BasicArrayIterator t(*this); m_ptr += step(1); return t; }
    OPTIONAL_CPP14_CONSTEXPR BasicArrayIterator operator--(int) noexcept { BasicArrayIterator t(*this); m_ptr -= step(1); return t; }

    OPTIONAL_CPP14_CONSTEXPR BasicArrayIterator& operator+=(difference_type n) noexcept { m_ptr += step(n); return *this; }
    OPTIONAL_CPP14_CONSTEXPR BasicArrayIterator& operator-=(difference_type n) noexcept { m_ptr -= step(n); return *this; }

    friend constexpr BasicArrayIterator operator+(BasicArrayIterator it, difference_type n) noexcept { return BasicArrayIterator(it.m_ptr + step(n)); }
    friend constexpr BasicArrayIterator operator+(difference_type n, BasicArrayIterator it) noexcept { return BasicArrayIterator(it.m_ptr + step(n)); }
    friend constexpr BasicArrayIterator operator-(BasicArrayIterator it, difference_type n) noexcept { return BasicArrayIterator(it.m_ptr - step(n)); }
    friend constexpr difference_type operator-(const BasicArrayIterator& a, const BasicArrayIterator& b) noexcept { return step(a.m_ptr - b.m_ptr); }

    friend constexpr bool operator==(const BasicArrayIterator& a, const BasicArrayIterator& b) noexcept { return a.m_ptr == b.m_ptr; }
    friend constexpr bool operator!=(const BasicArrayIterator& a, const BasicArrayIterator& b) noexcept { return a.m_ptr != b.m_ptr; }
    friend constexpr bool operator< (const BasicArrayIterator& a, const BasicArrayIterator& b) noexcept { return Reverse ? b.m_ptr < a.m_ptr : a.m_ptr < b.m_ptr; }
    friend constexpr bool operator> (const BasicArrayIterator& a, const BasicArrayIterator& b) noexcept { return b < a; }
    friend constexpr bool operator<=(const BasicArrayIterator& a, const BasicArrayIterator& b) noexcept { return !(b < a); }
    friend constexpr bool operator>=(const BasicArrayIterator& a, const BasicArrayIterator& b) noexcept { return !(a < b); }

private:
    T* m_ptr;
};

} // namespace detail

template <typename T> using ArrayIterator             = detail::BasicArrayIterator<T,       false>;
template <typename T> using ConstArrayIterator        = detail::BasicArrayIterator<const T, false>;
template <typename T> using ReverseArrayIterator      = detail::BasicArrayIterator<T,       true>;
template <typename T> using ConstReverseArrayIterator = detail::BasicArrayIterator<const T, true>;

template <typename Derived, typename T>
class ContiguousIterable {
    Derived&       self() noexcept       { return static_cast<Derived&>(*this); }
    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }

public:
    using iterator               = ArrayIterator<T>;
    using const_iterator         = ConstArrayIterator<T>;
    using reverse_iterator       = ReverseArrayIterator<T>;
    using const_reverse_iterator = ConstReverseArrayIterator<T>;

          iterator begin ()       noexcept { return iterator(self().data());                       }
          iterator end   ()       noexcept { return iterator(self().data() + self().size());       }
    const_iterator begin () const noexcept { return const_iterator(self().data());                 }
    const_iterator end   () const noexcept { return const_iterator(self().data() + self().size()); }
    const_iterator cbegin() const noexcept { return begin();                                       }
    const_iterator cend  () const noexcept { return end();                                         }

          reverse_iterator rbegin ()       noexcept { return reverse_iterator(self().data() + self().size());       }
          reverse_iterator rend   ()       noexcept { return reverse_iterator(self().data());                       }
    const_reverse_iterator rbegin () const noexcept { return const_reverse_iterator(self().data() + self().size()); }
    const_reverse_iterator rend   () const noexcept { return const_reverse_iterator(self().data());                 }
    const_reverse_iterator crbegin() const noexcept { return rbegin();                                              }
    const_reverse_iterator crend  () const noexcept { return rend();                                                }
};

} // namespace fizmo

#endif // FIZMO_ARRAY_ITERATORS_HPP