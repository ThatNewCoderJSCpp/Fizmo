#ifndef FIZMO_PACKED_COLOR_HPP
#define FIZMO_PACKED_COLOR_HPP

#include "color.hpp"
#include <cstddef>
#include <cstdint>

namespace fizmo {
namespace graphics {

class PremultipliedColor;

namespace detail {

inline constexpr std::uint32_t kLaneMask = 0x00FF00FFu;

constexpr std::uint32_t lanes_mul(std::uint32_t lanes, std::uint32_t f) noexcept {
    std::uint32_t p = lanes * f + 0x00800080u;
    p += (p >> 8) & kLaneMask;
    return (p >> 8) & kLaneMask;
}

constexpr std::uint32_t mul8x4(std::uint32_t v, std::uint32_t f) noexcept {
    return lanes_mul(v & kLaneMask, f) | (lanes_mul((v >> 8) & kLaneMask, f) << 8);
}

constexpr std::uint32_t lanes_mul_lanes(std::uint32_t a, std::uint32_t b) noexcept {
    const std::uint32_t lo = (a & 0xFFu) * (b & 0xFFu) + 0x80u;
    const std::uint32_t hi = ((a >> 16) & 0xFFu) * ((b >> 16) & 0xFFu) + 0x80u;
    return (((lo + (lo >> 8)) >> 8) & 0xFFu) | ((((hi + (hi >> 8)) >> 8) & 0xFFu) << 16);
}

constexpr std::uint32_t lanes_add_sat(std::uint32_t a, std::uint32_t b) noexcept {
    std::uint32_t s = a + b;
    s |= ((s >> 8) & 0x00010001u) * 0xFFu;
    return s & kLaneMask;
}

constexpr std::uint32_t add_sat8x4(std::uint32_t x, std::uint32_t y) noexcept {
    return lanes_add_sat(x & kLaneMask, y & kLaneMask) | (lanes_add_sat((x >> 8) & kLaneMask, (y >> 8) & kLaneMask) << 8);
}

constexpr std::uint32_t lerp8x4(std::uint32_t a, std::uint32_t b, std::uint32_t t) noexcept {
    return add_sat8x4(mul8x4(a, 255u - t), mul8x4(b, t));
}

constexpr std::uint32_t to_unit_byte(float t) noexcept {
    return t <= 0.0f ? 0u : (t >= 1.0f ? 255u : static_cast<std::uint32_t>(t * 255.0f + 0.5f));
}

} // namespace detail

class PackedColor {
private:
    std::uint32_t m_rgba = 0xFF000000u;

public:
    constexpr PackedColor() noexcept = default;
    explicit constexpr PackedColor(std::uint32_t rgba) noexcept : m_rgba(rgba) {}

    constexpr PackedColor(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) noexcept
        : m_rgba(static_cast<std::uint32_t>(r) | (static_cast<std::uint32_t>(g) << 8) | (static_cast<std::uint32_t>(b) << 16) | (static_cast<std::uint32_t>(a) << 24)) {}

    constexpr PackedColor(const Color& c) noexcept : PackedColor(c.red(), c.green(), c.blue(), c.alpha()) {}

    static constexpr PackedColor from_floats(float r, float g, float b, float a = 1.0f) noexcept {
        return PackedColor(
            static_cast<std::uint8_t>(detail::to_unit_byte(r)), static_cast<std::uint8_t>(detail::to_unit_byte(g)),
            static_cast<std::uint8_t>(detail::to_unit_byte(b)), static_cast<std::uint8_t>(detail::to_unit_byte(a))
        );
    }

    static constexpr PackedColor from_argb(std::uint32_t argb) noexcept {
        return PackedColor(
            static_cast<std::uint8_t>((argb >> 16) & 0xFFu), static_cast<std::uint8_t>((argb >> 8) & 0xFFu),
            static_cast<std::uint8_t>(argb & 0xFFu), static_cast<std::uint8_t>(argb >> 24)
        );
    }

    static constexpr PackedColor from_hex(std::uint32_t rrggbbaa) noexcept {
        return PackedColor(
            static_cast<std::uint8_t>(rrggbbaa >> 24), static_cast<std::uint8_t>((rrggbbaa >> 16) & 0xFFu),
            static_cast<std::uint8_t>((rrggbbaa >> 8) & 0xFFu), static_cast<std::uint8_t>(rrggbbaa & 0xFFu)
        );
    }

    constexpr std::uint32_t value() const noexcept { return m_rgba; }
    constexpr std::uint32_t argb() const noexcept { return (m_rgba & 0xFF00FF00u) | ((m_rgba & 0xFFu) << 16) | ((m_rgba >> 16) & 0xFFu); }
    constexpr std::uint32_t hex() const noexcept {
        return (static_cast<std::uint32_t>(red()) << 24) | (static_cast<std::uint32_t>(green()) << 16) | (static_cast<std::uint32_t>(blue()) << 8) | alpha();
    }

    constexpr std::uint8_t red()   const noexcept { return static_cast<std::uint8_t>(m_rgba & 0xFFu); }
    constexpr std::uint8_t green() const noexcept { return static_cast<std::uint8_t>((m_rgba >> 8) & 0xFFu); }
    constexpr std::uint8_t blue()  const noexcept { return static_cast<std::uint8_t>((m_rgba >> 16) & 0xFFu); }
    constexpr std::uint8_t alpha() const noexcept { return static_cast<std::uint8_t>(m_rgba >> 24); }

    constexpr PackedColor with_red(std::uint8_t v)   const noexcept { return PackedColor((m_rgba & 0xFFFFFF00u) | v); }
    constexpr PackedColor with_green(std::uint8_t v) const noexcept { return PackedColor((m_rgba & 0xFFFF00FFu) | (static_cast<std::uint32_t>(v) << 8)); }
    constexpr PackedColor with_blue(std::uint8_t v)  const noexcept { return PackedColor((m_rgba & 0xFF00FFFFu) | (static_cast<std::uint32_t>(v) << 16)); }
    constexpr PackedColor with_alpha(std::uint8_t v) const noexcept { return PackedColor((m_rgba & 0x00FFFFFFu) | (static_cast<std::uint32_t>(v) << 24)); }

    constexpr Color to_color() const noexcept { return Color(red(), green(), blue(), alpha()); }
    constexpr operator Color() const noexcept { return to_color(); }

    constexpr PackedColor lerp(PackedColor other, std::uint8_t t) const noexcept { return PackedColor(detail::lerp8x4(m_rgba, other.m_rgba, t)); }
    constexpr PackedColor lerp(PackedColor other, float t) const noexcept { return lerp(other, static_cast<std::uint8_t>(detail::to_unit_byte(t))); }
    constexpr PackedColor modulate(PackedColor other) const noexcept {
        return PackedColor(detail::lanes_mul_lanes(m_rgba & detail::kLaneMask, other.m_rgba & detail::kLaneMask)
                         | (detail::lanes_mul_lanes((m_rgba >> 8) & detail::kLaneMask, (other.m_rgba >> 8) & detail::kLaneMask) << 8));
    }

    constexpr PackedColor fade(std::uint8_t opacity) const noexcept {
        return with_alpha(static_cast<std::uint8_t>(detail::lanes_mul(alpha(), opacity)));
    }

    constexpr PremultipliedColor premultiplied() const noexcept;

    friend constexpr bool operator==(PackedColor a, PackedColor b) noexcept { return a.m_rgba == b.m_rgba; }
    friend constexpr bool operator!=(PackedColor a, PackedColor b) noexcept { return a.m_rgba != b.m_rgba; }
};

class PremultipliedColor {
private:
    std::uint32_t m_rgba = 0xFF000000u;

public:
    constexpr PremultipliedColor() noexcept = default;
    explicit constexpr PremultipliedColor(std::uint32_t premultiplied_rgba) noexcept : m_rgba(premultiplied_rgba) {}

    constexpr PremultipliedColor(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) noexcept
        : m_rgba(premultiply(static_cast<std::uint32_t>(r) | (static_cast<std::uint32_t>(g) << 8) | (static_cast<std::uint32_t>(b) << 16) | (static_cast<std::uint32_t>(a) << 24))) {}

    constexpr PremultipliedColor(const Color& c) noexcept : PremultipliedColor(c.red(), c.green(), c.blue(), c.alpha()) {}
    constexpr PremultipliedColor(PackedColor c) noexcept : m_rgba(premultiply(c.value())) {}

    static constexpr PremultipliedColor from_premultiplied(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept {
        return PremultipliedColor(static_cast<std::uint32_t>(r) | (static_cast<std::uint32_t>(g) << 8) | (static_cast<std::uint32_t>(b) << 16) | (static_cast<std::uint32_t>(a) << 24));
    }

    static constexpr PremultipliedColor transparent() noexcept { return PremultipliedColor(0u); }

    static constexpr std::uint32_t premultiply(std::uint32_t straight) noexcept {
        const std::uint32_t a = straight >> 24;
        return (detail::mul8x4(straight, a) & 0x00FFFFFFu) | (a << 24);
    }

    constexpr std::uint32_t value() const noexcept { return m_rgba; }
    constexpr std::uint8_t red()   const noexcept { return static_cast<std::uint8_t>(m_rgba & 0xFFu); }
    constexpr std::uint8_t green() const noexcept { return static_cast<std::uint8_t>((m_rgba >> 8) & 0xFFu); }
    constexpr std::uint8_t blue()  const noexcept { return static_cast<std::uint8_t>((m_rgba >> 16) & 0xFFu); }
    constexpr std::uint8_t alpha() const noexcept { return static_cast<std::uint8_t>(m_rgba >> 24); }
    constexpr bool opaque() const noexcept { return alpha() == 255; }
    constexpr bool invisible() const noexcept { return alpha() == 0; }

    constexpr PackedColor straight() const noexcept {
        const std::uint32_t a = alpha();
        if (a == 0) return PackedColor(0u);
        if (a == 255) return PackedColor(m_rgba);
        auto un = [a](std::uint32_t c) -> std::uint32_t { const std::uint32_t v = (c * 255u + a / 2) / a; return v > 255u ? 255u : v; };
        return PackedColor(static_cast<std::uint8_t>(un(red())), static_cast<std::uint8_t>(un(green())), static_cast<std::uint8_t>(un(blue())), static_cast<std::uint8_t>(a));
    }

    constexpr Color to_color() const noexcept { return straight().to_color(); }

    constexpr PremultipliedColor over(PremultipliedColor dst) const noexcept {
        const std::uint32_t inv = 255u - alpha();
        if (inv == 0) return *this;
        return PremultipliedColor(detail::add_sat8x4(m_rgba, detail::mul8x4(dst.m_rgba, inv)));
    }

    constexpr PremultipliedColor under(PremultipliedColor src) const noexcept { return src.over(*this); }

    constexpr PremultipliedColor add(PremultipliedColor other) const noexcept { return PremultipliedColor(detail::add_sat8x4(m_rgba, other.m_rgba)); }

    constexpr PremultipliedColor scale(std::uint8_t factor) const noexcept { return PremultipliedColor(detail::mul8x4(m_rgba, factor)); }
    constexpr PremultipliedColor scale(float factor) const noexcept { return scale(static_cast<std::uint8_t>(detail::to_unit_byte(factor))); }

    constexpr PremultipliedColor modulate(PremultipliedColor other) const noexcept {
        return PremultipliedColor(detail::lanes_mul_lanes(m_rgba & detail::kLaneMask, other.m_rgba & detail::kLaneMask)
                                | (detail::lanes_mul_lanes((m_rgba >> 8) & detail::kLaneMask, (other.m_rgba >> 8) & detail::kLaneMask) << 8));
    }

    constexpr PremultipliedColor lerp(PremultipliedColor other, std::uint8_t t) const noexcept { return PremultipliedColor(detail::lerp8x4(m_rgba, other.m_rgba, t)); }
    constexpr PremultipliedColor lerp(PremultipliedColor other, float t) const noexcept { return lerp(other, static_cast<std::uint8_t>(detail::to_unit_byte(t))); }

    friend constexpr bool operator==(PremultipliedColor a, PremultipliedColor b) noexcept { return a.m_rgba == b.m_rgba; }
    friend constexpr bool operator!=(PremultipliedColor a, PremultipliedColor b) noexcept { return a.m_rgba != b.m_rgba; }
};

constexpr PremultipliedColor PackedColor::premultiplied() const noexcept { return PremultipliedColor(*this); }

inline void blend_over(PremultipliedColor* dst, const PremultipliedColor* src, std::size_t count) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t s = src[i].value();
        const std::uint32_t a = s >> 24;
        if (a == 255u) dst[i] = src[i];
        else if (s != 0u) dst[i] = src[i].over(dst[i]);
    }
}

inline void blend_over(PremultipliedColor* dst, PremultipliedColor src, std::size_t count) noexcept {
    if (src.opaque()) { for (std::size_t i = 0; i < count; ++i) dst[i] = src; return; }
    if (src.value() == 0u) return;
    for (std::size_t i = 0; i < count; ++i) dst[i] = src.over(dst[i]);
}

inline void blend_add(PremultipliedColor* dst, const PremultipliedColor* src, std::size_t count) noexcept {
    for (std::size_t i = 0; i < count; ++i) dst[i] = dst[i].add(src[i]);
}

static_assert(sizeof(PackedColor) == 4, "PackedColor must be one 32-bit word");
static_assert(sizeof(PremultipliedColor) == 4, "PremultipliedColor must be one 32-bit word");

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_PACKED_COLOR_HPP
