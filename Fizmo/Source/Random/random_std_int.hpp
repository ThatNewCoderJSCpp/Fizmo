#ifndef RANDOM_STD_INT_HPP
#define RANDOM_STD_INT_HPP

#include <random>
#include <chrono>
#include <thread>
#include <cstdint>
#include <array>
#include <cstring>
#include <stdexcept>
#include <limits>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace fizmo {
namespace detail {

class RNG {
private:
    std::mt19937_64 engine;

    static std::uint64_t rdtsc() {
    #if defined(_MSC_VER)
        return __rdtsc();
    #elif defined(__i386__)
        std::uint64_t x;
        __asm__ volatile ("rdtsc" : "=A" (x));
        return x;
    #elif defined(__x86_64__)
        std::uint32_t hi, lo;
        __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
        return ((std::uint64_t)hi << 32) | lo;
    #else
        return std::chrono::high_resolution_clock::now().time_since_epoch().count();
    #endif
    }

    static std::uint64_t mix_entropy() {
        std::uint64_t seed = 0;
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        std::uint64_t tsc = rdtsc();
        std::uint64_t pid = static_cast<std::uint64_t>(::getpid());
        std::uint64_t tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        std::random_device rd;
        std::uint64_t rd_val = (static_cast<std::uint64_t>(rd()) << 32) ^ rd();
        seed ^= now + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= tsc + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= pid + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= tid + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= rd_val + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }

public:
    RNG() {
        std::uint64_t seed = mix_entropy();
        engine.seed(seed);
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    inline T random_int(T a, T b) {
        std::uniform_int_distribution<T> dist(a, b);
        return dist(engine);
    }
};

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #include <bcrypt.h>
#elif defined(__linux__)
    #include <sys/random.h>
#elif defined(__APPLE__)
    #include <Security/Security.h>
#elif defined(__ANDROID__)
    #include <sys/random.h>
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #include <unistd.h>
#elif defined(__sun)
    #include <fcntl.h>
    #include <unistd.h>
#else 
    #include <fcntl.h>
    #include <unistd.h>
#endif

inline void os_random_bytes(void* buffer, std::size_t length) {
#if defined(_WIN32) || defined(_WIN64)
    if (BCryptGenRandom(nullptr, static_cast<PUCHAR>(buffer), (ULONG)length, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        throw std::runtime_error("BCryptGenRandom failed");
    }
#elif defined(__linux__)
    ssize_t ret = getrandom(buffer, length, 0);
    if (ret < 0 || static_cast<std::size_t>(ret) != length) { throw std::runtime_error("genrandom failed"); }
#elif defined(__APPLE__)
    if (SecRandomCopyBytes(kSecRandomDefault, length, buffer) != errSecSuccess) { throw std::runtime_error("SecRandomCopyBytes failed"); }
#elif defined(__ANDROID__)
    ssize_t ret = getrandom(buffer, length, 0);
    if (ret < 0 || static_cast<std::size_t>(ret) != length) { throw std::runtime_error("genrandom failed"); }
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    arc4random_buf(buffer, length);
#elif defined(__sun)
    int fd = open("/dev/random", O_RDONLY);
    if (fd < 0) { throw std::runtime_error("open(/dev/random) failed"); }
    ssize_t ret = read(fd, buffer, length);
    close(fd);
    if (ret < 0 || static_cast<std::size_t>(ret) != length) { throw std::runtime_error("read(/dev/random) failed"); }
#else 
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) { throw std::runtime_error("open(/dev/urandom) failed"); }
    ssize_t ret = read(fd, buffer, length);
    close(fd);
    if (ret < 0 || static_cast<std::size_t>(ret) != length) { throw std::runtime_error("read(/dev/urandom) failed"); }
#endif
}

} // namespace detail

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T random_int(T a = T(0), T b = std::numeric_limits<T>::max()) {
    using U = typename std::make_unsigned<T>::type;
    U value;
    detail::os_random_bytes(&value, sizeof(U));
    const U range = static_cast<U>(b) - static_cast<U>(a);
    const U mapped = (range == std::numeric_limits<U>::max()) ? value : value % (range + U(1));
    return a + static_cast<T>(mapped);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T random_int_nothrow(T a = T(0), T b = std::numeric_limits<T>::max()) noexcept {
    try {
        return random_int(a, b);
    } catch (...) {
        return std::numeric_limits<T>::max();
    }
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T unsecure_random_int(T min = T(0), T max = std::numeric_limits<T>::max()) {
    detail::RNG rng;
    return rng.random_int(min, max);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T unsecure_random_int_nothrow(T min = T(0), T max = std::numeric_limits<T>::max()) noexcept {
    try {
        return unsecure_random_int(min, max);
    } catch (...) {
        return std::numeric_limits<T>::max();
    }
}

} // namespace fizmo

#endif // RANDOM_STD_INT_HPP