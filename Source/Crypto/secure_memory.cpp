#include "fizmo_library.hpp"
#include "secure_memory.hpp"

#if defined(OS_WINDOWS)
    #include <windows.h>
#else
    #include <sys/mman.h>
#endif

namespace fizmo {
namespace crypto {

void secure_zero(void* data, std::size_t len) noexcept {
    if (!data || len == 0) return;
#if defined(OS_WINDOWS)
    SecureZeroMemory(data, len);
#else
    volatile unsigned char* p = static_cast<volatile unsigned char*>(data);
    while (len--) *p++ = 0;
    __asm__ __volatile__("" : : "r"(data) : "memory");
#endif
}

bool secure_compare(const void* a, const void* b, std::size_t len) noexcept {
    const volatile unsigned char* x = static_cast<const volatile unsigned char*>(a);
    const volatile unsigned char* y = static_cast<const volatile unsigned char*>(b);
    unsigned char diff = 0;
    for (std::size_t i = 0; i < len; ++i) diff = static_cast<unsigned char>(diff | (x[i] ^ y[i]));
    return diff == 0;
}

bool lock_memory(void* data, std::size_t len) noexcept {
    if (!data || len == 0) return false;
#if defined(OS_WINDOWS)
    return VirtualLock(data, len) != 0;
#else
    return ::mlock(data, len) == 0;
#endif
}

void unlock_memory(void* data, std::size_t len) noexcept {
    if (!data || len == 0) return;
#if defined(OS_WINDOWS)
    VirtualUnlock(data, len);
#else
    ::munlock(data, len);
#endif
}

} // namespace crypto
} // namespace fizmo
