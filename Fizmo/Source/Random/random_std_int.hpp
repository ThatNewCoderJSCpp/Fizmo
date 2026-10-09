#ifndef FIZMO_RANDOM_HPP
#define FIZMO_RANDOM_HPP

#include "../Basic/fizmo_defines.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace fizmo {
namespace detail {

void os_random_bytes(void* buffer, std::size_t length);

class RNG {
private:
    std::mt19937_64 m_engine;

    static std::uint64_t rdtsc() noexcept;

    static std::uint64_t mix(std::uint64_t seed, std::uint64_t value) noexcept {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }

    static std::uint64_t make_seed() noexcept;

public:
    RNG() noexcept : m_engine(make_seed()) {}

    std::mt19937_64& engine() noexcept { return m_engine; }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && !std::is_same<typename std::remove_cv<T>::type, bool>::value>::type>
    T random_int(T a, T b) noexcept {
        if (a > b) { const T t = a; a = b; b = t; }
        if (a == b) return a;
        using Wide = typename std::conditional<std::is_signed<T>::value, long long, unsigned long long>::type;
        std::uniform_int_distribution<Wide> dist(static_cast<Wide>(a), static_cast<Wide>(b));
        return static_cast<T>(dist(m_engine));
    }
};

inline RNG& thread_rng() noexcept {
    static thread_local RNG rng;
    return rng;
}

} // namespace detail

inline void random_bytes(void* buffer, std::size_t length) {
    detail::os_random_bytes(buffer, length);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && !std::is_same<typename std::remove_cv<T>::type, bool>::value>::type>
T random_int(T a = T(0), T b = std::numeric_limits<T>::max()) {
    if (a > b) { const T t = a; a = b; b = t; }
    if (a == b) return a;
    using U = typename std::make_unsigned<T>::type;
    const U span = static_cast<U>(static_cast<U>(b) - static_cast<U>(a));  
    U value;

    if (span == std::numeric_limits<U>::max()) {
        random_bytes(&value, sizeof(U));          
    } else {
        const U bound = static_cast<U>(span + U(1));
        const U threshold = static_cast<U>(static_cast<U>(U(0) - bound) % bound);

        do {
            random_bytes(&value, sizeof(U));
        } while (value < threshold);

        value = static_cast<U>(value % bound);
    }

    return static_cast<T>(static_cast<U>(static_cast<U>(a) + value));
}

template <typename T>
T random_int_nothrow(T a = T(0), T b = std::numeric_limits<T>::max(), T fallback = std::numeric_limits<T>::max()) noexcept {
    try {
        return random_int<T>(a, b);
    } catch (...) {
        return fallback;
    }
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && !std::is_same<typename std::remove_cv<T>::type, bool>::value>::type>
T unsecure_random_int(T a = T(0), T b = std::numeric_limits<T>::max()) noexcept {
    return detail::thread_rng().template random_int<T>(a, b);
}

} // namespace fizmo

#endif // FIZMO_RANDOM_HPP