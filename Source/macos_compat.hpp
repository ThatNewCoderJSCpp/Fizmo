#ifndef FIZMO_MACOS_COMPAT_HPP
#define FIZMO_MACOS_COMPAT_HPP

#include "Basic/fizmo_defines.hpp"

#ifdef OS_MACOS

#include <cstdint>

namespace fizmo {
namespace macos {

struct Handle {
    void* window = nullptr;
    void* view   = nullptr;
    void* layer  = nullptr;
};

bool present_pixels(const Handle* handle, const std::uint32_t* bgra, unsigned int width, unsigned int height, unsigned int stride_bytes) noexcept;
float backing_scale(const Handle* handle) noexcept;

} // namespace macos
} // namespace fizmo

#endif // OS_MACOS
#endif // FIZMO_MACOS_COMPAT_HPP
