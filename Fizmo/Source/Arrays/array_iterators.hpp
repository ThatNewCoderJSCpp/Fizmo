#ifndef DYNAMIC_ARRAY_ITERATORS_HPP
#define DYNAMIC_ARRAY_ITERATORS_HPP

#include "../Basic/type_traits.hpp"
#include "../Basic/basic_includes.hpp"
#include "../Basic/fizmo_defines.hpp"
#include "../Standard Overloads/optional.hpp"

namespace fizmo {

template <typename T>
class ArrayIterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const T*;
    using reference         = const T&;

    constexpr ArrayIterator(T* ptr) noexcept : m_pointer(ptr) {}

    constexpr T& operator*() const noexcept { return *m_pointer; }
    constexpr T* operator->() const noexcept { return m_pointer; }

    OPTIONAL_CPP14_CONSTEXPR ArrayIterator& operator++() noexcept { 
        ++m_pointer; 
        return *this; 
    }

    OPTIONAL_CPP14_CONSTEXPR ArrayIterator operator++(int) noexcept { 
        ArrayIterator temp = *this; 
        ++m_pointer; 
        return temp; 
    }

    OPTIONAL_CPP14_CONSTEXPR ArrayIterator& operator--() noexcept { 
        --m_pointer; 
        return *this; 
    }

    OPTIONAL_CPP14_CONSTEXPR ArrayIterator operator--(int) noexcept {
        ArrayIterator temp = *this;
        --m_pointer;
        return temp;
    }

    constexpr ArrayIterator operator+(std::ptrdiff_t offset) const noexcept { 
        return ArrayIterator(m_pointer + offset); 
    }
    
    constexpr ArrayIterator operator-(std::ptrdiff_t offset) const noexcept { 
        return ArrayIterator(m_pointer - offset); 
    }
    
    constexpr std::ptrdiff_t operator-(const ArrayIterator& other) const noexcept { 
        return m_pointer - other.m_pointer; 
    }

    OPTIONAL_CPP14_CONSTEXPR ArrayIterator& operator+=(std::ptrdiff_t offset) noexcept {
        m_pointer += offset;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR ArrayIterator& operator-=(std::ptrdiff_t offset) noexcept {
        m_pointer -= offset;
        return *this;
    }
    
    constexpr T& operator[](std::ptrdiff_t offset) const noexcept { 
        return *(m_pointer + offset); 
    }

    constexpr bool operator==(const ArrayIterator& other) const noexcept { 
        return m_pointer == other.m_pointer; 
    }
    
    constexpr bool operator!=(const ArrayIterator& other) const noexcept { 
        return m_pointer != other.m_pointer; 
    }
    
    constexpr bool operator<(const ArrayIterator& other) const noexcept { 
        return m_pointer < other.m_pointer; 
    }
    
    constexpr bool operator<=(const ArrayIterator& other) const noexcept { 
        return m_pointer <= other.m_pointer; 
    }
    
    constexpr bool operator>(const ArrayIterator& other) const noexcept { 
        return m_pointer > other.m_pointer; 
    }
    
    constexpr bool operator>=(const ArrayIterator& other) const noexcept { 
        return m_pointer >= other.m_pointer; 
    }

private:
    T* m_pointer;
};

template <typename T>
class ConstArrayIterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const T*;
    using reference         = const T&;

    constexpr ConstArrayIterator(const T* ptr) noexcept : m_pointer(ptr) {}

    constexpr const T& operator*() const noexcept { return *m_pointer; }
    constexpr const T* operator->() const noexcept { return m_pointer; }

    OPTIONAL_CPP14_CONSTEXPR ConstArrayIterator& operator++() noexcept { 
        ++m_pointer; 
        return *this; 
    }

    OPTIONAL_CPP14_CONSTEXPR ConstArrayIterator operator++(int) noexcept { 
        ConstArrayIterator temp = *this; 
        ++m_pointer; 
        return temp; 
    }

    OPTIONAL_CPP14_CONSTEXPR ConstArrayIterator& operator--() noexcept { 
        --m_pointer; 
        return *this; 
    }

    OPTIONAL_CPP14_CONSTEXPR ConstArrayIterator operator--(int) noexcept {
        ConstArrayIterator temp = *this;
        --m_pointer;
        return temp;
    }

    constexpr ConstArrayIterator operator+(std::ptrdiff_t offset) const noexcept { 
        return ConstArrayIterator(m_pointer + offset); 
    }
    
    constexpr ConstArrayIterator operator-(std::ptrdiff_t offset) const noexcept { 
        return ConstArrayIterator(m_pointer - offset); 
    }
    
    constexpr std::ptrdiff_t operator-(const ConstArrayIterator& other) const noexcept { 
        return m_pointer - other.m_pointer; 
    }

    OPTIONAL_CPP14_CONSTEXPR ConstArrayIterator& operator+=(std::ptrdiff_t offset) noexcept {
        m_pointer += offset;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR ConstArrayIterator& operator-=(std::ptrdiff_t offset) noexcept {
        m_pointer -= offset;
        return *this;
    }
    
    constexpr const T& operator[](std::ptrdiff_t offset) const noexcept { 
        return *(m_pointer + offset); 
    }

    constexpr bool operator==(const ConstArrayIterator& other) const noexcept { 
        return m_pointer == other.m_pointer; 
    }
    
    constexpr bool operator!=(const ConstArrayIterator& other) const noexcept { 
        return m_pointer != other.m_pointer; 
    }
    
    constexpr bool operator<(const ConstArrayIterator& other) const noexcept { 
        return m_pointer < other.m_pointer; 
    }
    
    constexpr bool operator<=(const ConstArrayIterator& other) const noexcept { 
        return m_pointer <= other.m_pointer; 
    }
    
    constexpr bool operator>(const ConstArrayIterator& other) const noexcept { 
        return m_pointer > other.m_pointer; 
    }
    
    constexpr bool operator>=(const ConstArrayIterator& other) const noexcept { 
        return m_pointer >= other.m_pointer; 
    }

private:
    const T* m_pointer;
};

template <typename T>
class ReverseArrayIterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const T*;
    using reference         = const T&;

    constexpr ReverseArrayIterator(T* ptr) noexcept : m_pointer(ptr) {}

    constexpr T& operator*() const noexcept { return *(m_pointer - 1); }
    constexpr T* operator->() const noexcept { return (m_pointer - 1); }

    OPTIONAL_CPP14_CONSTEXPR ReverseArrayIterator& operator++() noexcept {
        --m_pointer; 
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR ReverseArrayIterator operator++(int) noexcept {
        ReverseArrayIterator temp = *this;
        --m_pointer;
        return temp;
    }

    OPTIONAL_CPP14_CONSTEXPR ReverseArrayIterator& operator--() noexcept {
        ++m_pointer;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR ReverseArrayIterator operator--(int) noexcept {
        ReverseArrayIterator temp = *this;
        ++m_pointer;
        return temp;
    }

    constexpr ReverseArrayIterator operator+(std::ptrdiff_t offset) const noexcept {
        return ReverseArrayIterator(m_pointer - offset);
    }

    constexpr ReverseArrayIterator operator-(std::ptrdiff_t offset) const noexcept {
        return ReverseArrayIterator(m_pointer + offset);
    }

    constexpr std::ptrdiff_t operator-(const ReverseArrayIterator& other) const noexcept {
        return other.m_pointer - m_pointer;
    }

    OPTIONAL_CPP14_CONSTEXPR ReverseArrayIterator& operator+=(std::ptrdiff_t offset) noexcept {
        m_pointer -= offset;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR ReverseArrayIterator& operator-=(std::ptrdiff_t offset) noexcept {
        m_pointer += offset;
        return *this;
    }

    constexpr T& operator[](std::ptrdiff_t offset) const noexcept {
        return *(*this + offset);
    }

    constexpr bool operator==(const ReverseArrayIterator& other) const noexcept {
        return m_pointer == other.m_pointer;
    }

    constexpr bool operator!=(const ReverseArrayIterator& other) const noexcept {
        return m_pointer != other.m_pointer;
    }

    constexpr bool operator<(const ReverseArrayIterator& other) const noexcept {
        return m_pointer > other.m_pointer;
    }

    constexpr bool operator<=(const ReverseArrayIterator& other) const noexcept {
        return m_pointer >= other.m_pointer;
    }

    constexpr bool operator>(const ReverseArrayIterator& other) const noexcept {
        return m_pointer < other.m_pointer;
    }

    constexpr bool operator>=(const ReverseArrayIterator& other) const noexcept {
        return m_pointer <= other.m_pointer;
    }

private:
    T* m_pointer;
};

template <typename T>
class ConstReverseArrayIterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const T*;
    using reference         = const T&;

    constexpr ConstReverseArrayIterator(const T* ptr) noexcept : m_pointer(ptr) {}

    constexpr const T& operator*() const noexcept { return *(m_pointer - 1); }
    constexpr const T* operator->() const noexcept { return (m_pointer - 1); }

    OPTIONAL_CPP14_CONSTEXPR ConstReverseArrayIterator& operator++() noexcept {
        --m_pointer;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR ConstReverseArrayIterator operator++(int) noexcept {
        ConstReverseArrayIterator temp = *this;
        --m_pointer;
        return temp;
    }

    OPTIONAL_CPP14_CONSTEXPR ConstReverseArrayIterator& operator--() noexcept {
        ++m_pointer;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR ConstReverseArrayIterator operator--(int) noexcept {
        ConstReverseArrayIterator temp = *this;
        ++m_pointer;
        return temp;
    }

    constexpr ConstReverseArrayIterator operator+(std::ptrdiff_t offset) const noexcept {
        return ConstReverseArrayIterator(m_pointer - offset);
    }

    constexpr ConstReverseArrayIterator operator-(std::ptrdiff_t offset) const noexcept {
        return ConstReverseArrayIterator(m_pointer + offset);
    }

    constexpr std::ptrdiff_t operator-(const ConstReverseArrayIterator& other) const noexcept {
        return other.m_pointer - m_pointer;
    }

    OPTIONAL_CPP14_CONSTEXPR ConstReverseArrayIterator& operator+=(std::ptrdiff_t offset) noexcept {
        m_pointer -= offset;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR ConstReverseArrayIterator& operator-=(std::ptrdiff_t offset) noexcept {
        m_pointer += offset;
        return *this;
    }

    constexpr const T& operator[](std::ptrdiff_t offset) const noexcept {
        return *(*this + offset);
    }

    constexpr bool operator==(const ConstReverseArrayIterator& other) const noexcept {
        return m_pointer == other.m_pointer;
    }

    constexpr bool operator!=(const ConstReverseArrayIterator& other) const noexcept {
        return m_pointer != other.m_pointer;
    }

    constexpr bool operator<(const ConstReverseArrayIterator& other) const noexcept {
        return m_pointer > other.m_pointer;
    }

    constexpr bool operator<=(const ConstReverseArrayIterator& other) const noexcept {
        return m_pointer >= other.m_pointer;
    }

    constexpr bool operator>(const ConstReverseArrayIterator& other) const noexcept {
        return m_pointer < other.m_pointer;
    }

    constexpr bool operator>=(const ConstReverseArrayIterator& other) const noexcept {
        return m_pointer <= other.m_pointer;
    }

private:
    const T* m_pointer;
};

} // namespace fizmo

#endif // DYNAMIC_ARRAY_ITERATORS_HPP