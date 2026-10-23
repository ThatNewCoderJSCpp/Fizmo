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

bool strequal(std::string_view a, std::string_view b) noexcept {
    static const unsigned char empty_byte = 0;
    const std::size_t a_len = a.size();
    const std::size_t b_len = b.size();
    const std::size_t longest = a_len > b_len ? a_len : b_len;
    const volatile unsigned char* x = a_len ? reinterpret_cast<const volatile unsigned char*>(a.data()) : &empty_byte;
    const volatile unsigned char* y = b_len ? reinterpret_cast<const volatile unsigned char*>(b.data()) : &empty_byte;
    volatile std::size_t flag = a_len ^ b_len;

    for (std::size_t i = 0; i < longest; ++i) {
        const bool inside_a = i < a_len;
        const bool inside_b = i < b_len;
        const unsigned char cx = static_cast<unsigned char>(x[inside_a ? i : 0] & static_cast<unsigned char>(0u - static_cast<unsigned>(inside_a)));
        const unsigned char cy = static_cast<unsigned char>(y[inside_b ? i : 0] & static_cast<unsigned char>(0u - static_cast<unsigned>(inside_b)));
        flag = flag | static_cast<std::size_t>(cx ^ cy);
    }

    return flag == 0;
}

bool strequal(const char* a, const char* b) noexcept {
    return strequal(std::string_view(a ? a : ""), std::string_view(b ? b : ""));
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
