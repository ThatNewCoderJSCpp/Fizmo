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

#if defined(OS_WINDOWS)
    #include <bcrypt.h>         
#elif defined(OS_LINUX)
    #include <errno.h>
    #include <fcntl.h>
    #include <sys/random.h>
    #include <unistd.h>
#endif

#if defined(_MSC_VER)
    #include <intrin.h>
#endif

namespace fizmo {
namespace detail {

inline void os_random_bytes(void* buffer, std::size_t length) {
    if (length == 0) return;

#if defined(OS_WINDOWS)
    const NTSTATUS status = BCryptGenRandom(
        nullptr, static_cast<PUCHAR>(buffer), 
        static_cast<ULONG>(length),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );

    if (status != 0) throw std::runtime_error("BCryptGenRandom failed");
#elif defined(OS_LINUX)
    auto* out = static_cast<unsigned char*>(buffer);
    std::size_t remaining = length;

    while (remaining > 0) {
        const ssize_t got = ::getrandom(out, remaining, 0);

        if (got < 0) {
            if (errno == EINTR) continue;          
            if (errno == ENOSYS) break;            
            throw std::runtime_error("getrandom failed");
        }

        out       += got;
        remaining -= static_cast<std::size_t>(got);
    }

    if (remaining == 0) return;
    const int fd = ::open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) throw std::runtime_error("open(/dev/urandom) failed");

    while (remaining > 0) {
        const ssize_t got = ::read(fd, out, remaining);

        if (got < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            throw std::runtime_error("read(/dev/urandom) failed");
        }

        if (got == 0) {                            
            ::close(fd);
            throw std::runtime_error("read(/dev/urandom) returned EOF");
        }

        out       += got;
        remaining -= static_cast<std::size_t>(got);
    }

    ::close(fd);
#endif
}

class RNG {
private:
    std::mt19937_64 m_engine;

    static std::uint64_t rdtsc() noexcept {
    #if defined(_MSC_VER)
        return __rdtsc();
    #elif defined(__x86_64__)
        std::uint32_t hi, lo;
        __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
        return (static_cast<std::uint64_t>(hi) << 32) | lo;
    #elif defined(__i386__)
        std::uint64_t x;
        __asm__ volatile ("rdtsc" : "=A"(x));
        return x;
    #else
        return static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    #endif
    }

    static std::uint64_t mix(std::uint64_t seed, std::uint64_t value) noexcept {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }

    static std::uint64_t make_seed() noexcept {
        std::uint64_t seed = 0;
        std::uint64_t os_value = 0;

        try {
            os_random_bytes(&os_value, sizeof(os_value));
        } catch (...) {
            os_value = 0;
        }

        seed = mix(seed, os_value);
        seed = mix(seed, static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        seed = mix(seed, rdtsc());
        seed = mix(seed, std::hash<std::thread::id>{}(std::this_thread::get_id()));
        seed = mix(seed, reinterpret_cast<std::uintptr_t>(&seed));  
        return seed;
    }

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