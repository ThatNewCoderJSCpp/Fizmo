#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "random_std_int.hpp"

namespace fizmo {
namespace detail {

void os_random_bytes(void* buffer, std::size_t length) {
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

auto RNG::rdtsc() noexcept -> std::uint64_t {
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

auto RNG::make_seed() noexcept -> std::uint64_t {
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

} // namespace detail
} // namespace fizmo
