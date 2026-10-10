#ifndef FIZMO_GPU_COMMON_HPP
#define FIZMO_GPU_COMMON_HPP

#include "gpu_geometry.hpp"
#include "../../Graphics/paint.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fizmo {
namespace windows {
namespace detail {

namespace gfx {

struct Vertex {
    float         x, y;      
    float         u, v;      
    std::uint32_t color;     
    std::uint32_t mode;      
    float         grad[4];   
};
static_assert(sizeof(Vertex) == 40, "vertex layout must match the pipeline");

inline std::uint32_t pack_rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept {
    return static_cast<std::uint32_t>(r) | (static_cast<std::uint32_t>(g) << 8)
         | (static_cast<std::uint32_t>(b) << 16) | (static_cast<std::uint32_t>(a) << 24);
}

inline std::uint8_t mul8(unsigned int c, unsigned int a) noexcept {
    return static_cast<std::uint8_t>((c * a + 127u) / 255u);
}

inline std::uint32_t pack_premul(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept {
    return pack_rgba(mul8(r, a), mul8(g, a), mul8(b, a), a);
}

inline std::uint32_t pack_opacity(std::uint8_t a) noexcept {
    return pack_rgba(a, a, a, a);
}

inline std::uint8_t scale_alpha(std::uint8_t a, float opacity) noexcept {
    if (opacity >= 1.0f) return a;
    if (opacity <= 0.0f) return 0;
    return static_cast<std::uint8_t>(a * opacity + 0.5f);
}

inline std::uint64_t hash_bytes(const void* data, std::size_t n, std::uint64_t seed = 0x9E3779B97F4A7C15ull) noexcept {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    std::uint64_t h = seed ^ (n * 0xff51afd7ed558ccdull);

    while (n >= 8) {
        std::uint64_t k;
        std::memcpy(&k, p, 8);
        k *= 0x87c37b91114253d5ull;
        k ^= k >> 31;
        h ^= k;
        h = ((h << 27) | (h >> 37)) * 5 + 0x52dce729;
        p += 8; n -= 8;
    }

    std::uint64_t tail = 0;
    for (std::size_t i = 0; i < n; ++i) tail |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    h ^= tail * 0x4cf5ad432745937full;
    h ^= h >> 33; h *= 0xff51afd7ed558ccdull; h ^= h >> 33;
    return h;
}

struct LineVertex3D {
    float         x, y, z;        
    float         ox, oy, oz;     
    std::uint32_t color;          
    float         side;           
    float         width;          
};
static_assert(sizeof(LineVertex3D) == 36, "line vertex layout must match the pipeline");

inline Cap  to_cap(graphics::LineCap c) noexcept {
    return c == graphics::LineCap::Round ? Cap::Round : c == graphics::LineCap::Square ? Cap::Square : Cap::Flat;
}

inline Join to_join(graphics::LineJoin j) noexcept {
    return j == graphics::LineJoin::Round ? Join::Round : j == graphics::LineJoin::Bevel ? Join::Bevel : Join::Miter;
}

} // namespace gfx

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_GPU_COMMON_HPP
