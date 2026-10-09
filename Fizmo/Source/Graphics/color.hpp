#ifndef FIZMO_COLOR_HPP
#define FIZMO_COLOR_HPP

#include <cmath>
#include <iostream>
#include <ostream>
#include <istream>
#include <cstdint>
#include "../Standard Overloads/num_theory.hpp"
#include "../Standard Overloads/sqrt.hpp"
#include "../Basic/fizmo_defines.hpp"
#include "../Random/random_std_int.hpp"

namespace fizmo {
namespace graphics {

class HSLColor;

class Color {
public:
    constexpr Color() noexcept : m_red(0), m_green(0), m_blue(0), m_alpha(255) {}
    constexpr Color(const std::uint8_t r, const std::uint8_t g, const std::uint8_t b, const std::uint8_t a = 255) noexcept
        : m_red(clamp(r)), m_green(clamp(g)), m_blue(clamp(b)), m_alpha(clamp(a)) {}
    constexpr Color(const Color& other) noexcept
        : m_red(other.m_red), m_green(other.m_green), m_blue(other.m_blue), m_alpha(other.m_alpha) {}

    constexpr Color(Color&& other) noexcept
        : m_red(other.m_red), m_green(other.m_green), m_blue(other.m_blue), m_alpha(other.m_alpha) {
        other.m_red = 0;
        other.m_green = 0;
        other.m_blue = 0;
        other.m_alpha = 255;
    }

    constexpr Color(const HSLColor& other, const std::uint8_t a = 255) noexcept;

    OPTIONAL_CPP14_CONSTEXPR Color& operator=(const Color& other) noexcept {
        if (this != &other) {
            m_red = other.m_red;
            m_green = other.m_green;
            m_blue = other.m_blue;
            m_alpha = other.m_alpha;
        }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR Color& operator=(Color&& other) noexcept {
        if (this != &other) {
            m_red = other.m_red;
            m_green = other.m_green;
            m_blue = other.m_blue;
            m_alpha = other.m_alpha;
            other.m_red = 0;
            other.m_green = 0;
            other.m_blue = 0;
            other.m_alpha = 255;
        }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR Color& operator=(const HSLColor& other) noexcept;

public:
    std::string to_string() const {
        std::stringstream ss;
        if (m_alpha == 255) {
            ss << "RGB(" << +m_red << ", " << +m_green << ", " << +m_blue << ")";
        } else {
            ss << "RGBA(" << +m_red << ", " << +m_green << ", " << +m_blue << ", " << +m_alpha << ")";
        }
        return ss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const Color& color) {
        os << color.to_string();
        return os;
    }

public:
    constexpr bool operator==(const Color& other) const noexcept {
        return m_red == other.m_red && m_green == other.m_green
            && m_blue == other.m_blue && m_alpha == other.m_alpha;
    }
    constexpr bool operator!=(const Color& other) const noexcept { return !(*this == other); }
    constexpr bool operator<(const Color& other) const noexcept { return magnitude() < other.magnitude(); }
    constexpr bool operator>(const Color& other) const noexcept { return !(*this < other); }
    constexpr bool operator<=(const Color& other) const noexcept { return (*this < other) || (*this == other); }
    constexpr bool operator>=(const Color& other) const noexcept { return (*this > other) || (*this == other); }

public:
    constexpr Color operator+(const Color& other) const noexcept {
        return Color(
            clamp(m_red + other.m_red), clamp(m_green + other.m_green),
            clamp(m_blue + other.m_blue), clamp(m_alpha + other.m_alpha)
        );
    }

    constexpr Color operator-(const Color& other) const noexcept {
        return Color(
            clamp(m_red - other.m_red), clamp(m_green - other.m_green),
            clamp(m_blue - other.m_blue), clamp(m_alpha - other.m_alpha)
        );
    }

    constexpr Color operator*(const std::uint8_t value) const noexcept {
        return Color(
            clamp(m_red * value), clamp(m_green * value),
            clamp(m_blue * value), m_alpha
        );
    }

    constexpr Color& operator+=(const Color& other) noexcept {
        m_red = clamp(m_red + other.m_red);
        m_green = clamp(m_green + other.m_green);
        m_blue = clamp(m_blue + other.m_blue);
        m_alpha = clamp(m_alpha + other.m_alpha);
        return *this;
    }

    constexpr Color& operator-=(const Color& other) noexcept {
        m_red = clamp(m_red - other.m_red);
        m_green = clamp(m_green - other.m_green);
        m_blue = clamp(m_blue - other.m_blue);
        m_alpha = clamp(m_alpha - other.m_alpha);
        return *this;
    }

    constexpr Color& operator*=(const std::uint8_t value) noexcept {
        m_red = clamp(m_red * value);
        m_green = clamp(m_green * value);
        m_blue = clamp(m_blue * value);
        return *this;
    }

public:
    constexpr std::uint8_t red() const noexcept { return m_red; }
    constexpr std::uint8_t green() const noexcept { return m_green; }
    constexpr std::uint8_t blue() const noexcept { return m_blue; }
    constexpr std::uint8_t alpha() const noexcept { return m_alpha; }
    constexpr std::uint8_t& red() noexcept { return m_red; }
    constexpr std::uint8_t& green() noexcept { return m_green; }
    constexpr std::uint8_t& blue() noexcept { return m_blue; }
    constexpr std::uint8_t& alpha() noexcept { return m_alpha; }

    constexpr void set_red(const std::uint8_t r) noexcept { m_red = clamp(r); }
    constexpr void set_green(const std::uint8_t g) noexcept { m_green = clamp(g); }
    constexpr void set_blue(const std::uint8_t b) noexcept { m_blue = clamp(b); }
    constexpr void set_alpha(const std::uint8_t a) noexcept { m_alpha = clamp(a); }

    constexpr void set(const std::uint8_t r, const std::uint8_t g, const std::uint8_t b) noexcept {
        set_red(r);
        set_green(g);
        set_blue(b);
    }

    constexpr void set(const std::uint8_t r, const std::uint8_t g, const std::uint8_t b, const std::uint8_t a) noexcept {
        set_red(r);
        set_green(g);
        set_blue(b);
        set_alpha(a);
    }

    constexpr bool is_opaque() const noexcept { return m_alpha == 255; }
    constexpr bool is_transparent() const noexcept { return m_alpha == 0; }
    constexpr double alpha_normalized() const noexcept { return m_alpha / 255.0; }
    constexpr Color with_alpha(const std::uint8_t a) const noexcept { return Color(m_red, m_green, m_blue, a); }

public:
    constexpr Color combine_additive(const Color& other) const noexcept {
        return Color(
            clamp(m_red + other.m_red),
            clamp(m_green + other.m_green),
            clamp(m_blue + other.m_blue),
            clamp(m_alpha + other.m_alpha)
        );
    }

    constexpr Color combine_subtractive(const Color& other) const noexcept {
        const std::uint8_t c1 = 255 - m_red;
        const std::uint8_t m1 = 255 - m_green;
        const std::uint8_t y1 = 255 - m_blue;
        const std::uint8_t c2 = 255 - other.m_red;
        const std::uint8_t m2 = 255 - other.m_green;
        const std::uint8_t y2 = 255 - other.m_blue;
        const std::uint8_t c_result = fizmo::min_constexpr(static_cast<std::uint16_t>(c1) + c2, 255);
        const std::uint8_t m_result = fizmo::min_constexpr(static_cast<std::uint16_t>(m1) + m2, 255);
        const std::uint8_t y_result = fizmo::min_constexpr(static_cast<std::uint16_t>(y1) + y2, 255);

        return Color(
            255 - c_result,
            255 - m_result,
            255 - y_result,
            fizmo::min_constexpr(m_alpha, other.m_alpha)
        );
    }

    constexpr Color blend_over(const Color& src) const noexcept {
        const std::uint16_t sa = src.m_alpha;
        const std::uint16_t da = m_alpha;
        const std::uint16_t inv_sa = 255 - sa;
        const std::uint16_t out_a = sa + ((da * inv_sa + 127) / 255);
        if (out_a == 0) return Color(0, 0, 0, 0);

        auto blend_ch = [sa, inv_sa, out_a](std::uint8_t s, std::uint8_t d, std::uint16_t d_alpha) -> std::uint8_t {
            std::uint16_t val = (static_cast<std::uint16_t>(s) * sa + static_cast<std::uint16_t>(d) * ((d_alpha * inv_sa + 127) / 255));
            return static_cast<std::uint8_t>((val + out_a / 2) / out_a);
        };

        return Color(
            blend_ch(src.m_red,   m_red,   da),
            blend_ch(src.m_green, m_green, da),
            blend_ch(src.m_blue,  m_blue,  da),
            static_cast<std::uint8_t>(out_a > 255 ? 255 : out_a)
        );
    }

    constexpr Color blend_over_premultiplied(const Color& src) const noexcept {
        const std::uint16_t inv_sa = 255 - src.m_alpha;

        return Color(
            clamp(src.m_red   + ((m_red   * inv_sa + 127) / 255)),
            clamp(src.m_green + ((m_green * inv_sa + 127) / 255)),
            clamp(src.m_blue  + ((m_blue  * inv_sa + 127) / 255)),
            clamp(src.m_alpha + ((m_alpha * inv_sa + 127) / 255))
        );
    }

    constexpr Color lerp(const Color& other, double t) const noexcept {
        t = fizmo::clamp(t, 0.0, 1.0);

        return Color(
            static_cast<std::uint8_t>(m_red   + (other.m_red   - static_cast<double>(m_red))   * t),
            static_cast<std::uint8_t>(m_green + (other.m_green - static_cast<double>(m_green)) * t),
            static_cast<std::uint8_t>(m_blue  + (other.m_blue  - static_cast<double>(m_blue))  * t),
            static_cast<std::uint8_t>(m_alpha + (other.m_alpha - static_cast<double>(m_alpha)) * t)
        );
    }

    constexpr Color to_premultiplied() const noexcept {
        return Color(
            static_cast<std::uint8_t>((static_cast<std::uint16_t>(m_red)   * m_alpha + 127) / 255),
            static_cast<std::uint8_t>((static_cast<std::uint16_t>(m_green) * m_alpha + 127) / 255),
            static_cast<std::uint8_t>((static_cast<std::uint16_t>(m_blue)  * m_alpha + 127) / 255),
            m_alpha
        );
    }

    constexpr Color to_straight() const noexcept {
        if (m_alpha == 0) return Color(0, 0, 0, 0);

        return Color(
            clamp(static_cast<std::uint16_t>(m_red)   * 255 / m_alpha),
            clamp(static_cast<std::uint16_t>(m_green) * 255 / m_alpha),
            clamp(static_cast<std::uint16_t>(m_blue)  * 255 / m_alpha),
            m_alpha
        );
    }

private:
    constexpr std::uint8_t clamp(const std::uint8_t value) const noexcept {
        return fizmo::clamp(value, static_cast<std::uint8_t>(0), static_cast<std::uint8_t>(255));
    }

    constexpr double magnitude() const noexcept {
        return fizmo::math::sqrt_constexpr(m_red * m_red + m_green * m_green + m_blue * m_blue);
    }

private:
    struct RGBInitializer {
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;
    };

    static constexpr RGBInitializer init_rgb(const HSLColor& hsl) noexcept;
    static constexpr RGBInitializer init_rgb(HSLColor&& hsl) noexcept;

    static constexpr double hue_to_rgb(const double p, const double q, double t) noexcept {
        if (t < 0) t += 1;
        if (t > 1) t -= 1;
        if (t < 1.0 / 6.0) return p + (q - p) * 6 * t;
        if (t < 1.0 / 2.0) return q;
        if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6;
        return p;
    }

private:
    std::uint8_t m_red;
    std::uint8_t m_green;
    std::uint8_t m_blue;
    std::uint8_t m_alpha;
};

inline Color random_color(unsigned int num_draws = 1, std::uint8_t alpha = 255) noexcept {
    if (num_draws == 0) { num_draws = random_int_nothrow<unsigned int>(1u); }
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    for (unsigned int n = 0; n < num_draws; ++n) {
        r = random_int_nothrow<std::uint8_t>();
        g = random_int_nothrow<std::uint8_t>();
        b = random_int_nothrow<std::uint8_t>();
    }

    return Color(r, g, b, alpha);
}

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_COLOR_HPP