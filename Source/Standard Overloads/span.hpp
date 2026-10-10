#ifndef SPAN_OVERLOAD_HPP
#define SPAN_OVERLOAD_HPP

#include <cstdint>

namespace fizmo {

template<typename T>
class Span {
    T* data_;
    std::size_t size_;
public:
    constexpr Span(T* d, std::size_t s) noexcept : data_(d), size_(s) {}
    
    template<std::size_t N>
    constexpr Span(T (&arr)[N]) noexcept : data_(arr), size_(N) {}
    
    constexpr const T* data() const noexcept { return data_; }
    constexpr T* data() noexcept { return data_; }

    constexpr const std::size_t size() const noexcept { return size_; }
    constexpr std::size_t size() noexcept { return size_; }

    constexpr const T& operator[](std::size_t i) const noexcept { return i >= size_ ? data_[i % size_] : data_[i]; }
    constexpr T& operator[](std::size_t i) noexcept { return i >= size_ ? data_[i % size_] : data_[i]; }

    constexpr const T* begin() const noexcept { return data_; }
    constexpr T* begin() noexcept { return data_; }
    
    constexpr const T* end() const noexcept { return data_ + size_; }
    constexpr T* end() noexcept { return data_ + size_; }
};

} // namespce fizmo

#endif // SPAN_OVERLOAD_HPP