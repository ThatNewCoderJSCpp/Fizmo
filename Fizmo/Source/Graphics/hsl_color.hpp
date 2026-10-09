#ifndef HSL_COLOR_HPP
#define HSL_COLOR_HPP

#include <cmath>
#include <algorithm>
#include <functional>
#include "color.hpp"
#include "../Basic/fizmo_defines.hpp"
#include "../Standard Overloads/num_theory.hpp"

namespace fizmo {
namespace graphics {

class HSLColor {
public:
    constexpr HSLColor() noexcept : m_hue(0), m_saturation(0), m_lightness(0) {}
    constexpr HSLColor(const double h, const double s, const double l) noexcept : m_hue(clamp_hue(h)), m_saturation(clamp_percentage(s)), m_lightness(clamp_percentage(l)) {}
    constexpr HSLColor(const HSLColor& other) noexcept : m_hue(other.m_hue), m_saturation(other.m_saturation), m_lightness(other.m_lightness) {}

    constexpr HSLColor(HSLColor&& other) noexcept : m_hue(other.m_hue), m_saturation(other.m_saturation), m_lightness(other.m_lightness) {
        other.m_hue = 0;
        other.m_saturation = 0;
        other.m_lightness = 0;
    }

    constexpr HSLColor(const Color& other) noexcept : m_hue(init_hsl(other).h), m_saturation(init_hsl(other).s), m_lightness(init_hsl(other).l) {}
    constexpr HSLColor(Color&& other) noexcept : m_hue(init_hsl(other).h), m_saturation(init_hsl(other).s), m_lightness(init_hsl(other).l) {}

    OPTIONAL_CPP14_CONSTEXPR HSLColor& operator=(const HSLColor& other) noexcept {
        if (this != &other) {
            m_hue = other.m_hue;
            m_saturation = other.m_saturation;
            m_lightness = other.m_lightness;
        }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR HSLColor& operator=(HSLColor&& other) noexcept {
        if (this != &other) {
            m_hue = other.m_hue;
            m_saturation = other.m_saturation;
            m_lightness = other.m_lightness;
            other.m_hue = 0;
            other.m_saturation = 0;
            other.m_lightness = 0;
        }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR HSLColor& operator=(const Color& other) noexcept {
        const HSLInitializer init = init_hsl(other);
        m_hue = init.h;
        m_saturation = init.s;
        m_lightness = init.l;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR HSLColor& operator=(Color&& other) noexcept {
        const HSLInitializer init = init_hsl(other);
        m_hue = init.h;
        m_saturation = init.s;
        m_lightness = init.l;
        return *this;
    }

public:
    constexpr double hue() const noexcept { return m_hue; }
    constexpr double saturation() const noexcept { return m_saturation; }
    constexpr double lightness() const noexcept { return m_lightness; }
    constexpr double& hue() noexcept { return m_hue; }
    constexpr double& saturation() noexcept { return m_saturation; }
    constexpr double& lightness() noexcept { return m_lightness; }

    constexpr void set_hue(const double h) noexcept { m_hue = clamp_hue(h); }
    constexpr void set_saturation(const double s) noexcept { m_saturation = clamp_percentage(s); }
    constexpr void set_lightness(const double l) noexcept { m_lightness = clamp_percentage(l); }

public:
    std::string to_string() const {
        std::stringstream ss;
        ss << "HSL(" << m_hue << "°, " 
           << m_saturation << "%, " 
           << m_lightness << "%)";
        return ss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const HSLColor& hsl) {
        os << hsl.to_string();
        return os;
    }

private:
    constexpr double clamp_hue(const double h) const noexcept {
        const double h_new = fizmo::math::mod_constexpr(h, 360.0);
        return h_new < 0 ? h_new + 360.0 : h_new;
    }

    constexpr double clamp_percentage(const double v) const noexcept { return fizmo::clamp(v, 0.0, 100.0); }

private:
    struct HSLInitializer {
        double h;
        double s;
        double l;
    };

    static constexpr HSLInitializer init_hsl(const Color& rgb) noexcept {
        HSLInitializer init{};
        const double r = rgb.red() / 255.0;
        const double g = rgb.green() / 255.0;
        const double b = rgb.blue() / 255.0;

        const double c_max = fizmo::max_constexpr(r, g, b);
        const double c_min = fizmo::min_constexpr(r, g, b);
        const double delta = c_max - c_min;
        init.l = (c_max + c_min) * 50.0;

        if (fizmo::abs_constexpr(delta) <= 1e-10) {
            init.h = 0.0;
            init.s = 0.0;
            return init;
        }

        init.s = delta / (1.0 - std::abs(2.0 * (init.l / 100.0) - 1.0)) * 100.0;
        double h = 0.0;

        if (c_max == r) {
            h = 60.0 * fizmo::math::mod_constexpr(((g - b) / delta), 6.0);
        } else if (c_max == g) {
            h = 60.0 * (((b - r) / delta) + 2.0);
        } else if (c_max == b) {
            h = 60.0 * (((r - g) / delta) + 4.0);
        }

        init.h = h < 0 ? h + 360.0 : h;
        return init;
    }

    static constexpr HSLInitializer init_hsl(Color&& rgb) noexcept {
        HSLInitializer init{};
        const double r = rgb.red() / 255.0;
        const double g = rgb.green() / 255.0;
        const double b = rgb.blue() / 255.0;

        const double c_max = fizmo::max_constexpr(r, g, b);
        const double c_min = fizmo::min_constexpr(r, g, b);
        const double delta = c_max - c_min;
        init.l = (c_max + c_min) * 50.0;

        if (fizmo::abs_constexpr(delta) <= 1e-10) {
            init.h = 0.0;
            init.s = 0.0;
            return init;
        }

        init.s = delta / (1.0 - std::abs(2.0 * (init.l / 100.0) - 1.0)) * 100.0;
        double h = 0.0;

        if (c_max == r) {
            h = 60.0 * fizmo::math::mod_constexpr(((g - b) / delta), 6.0);
        } else if (c_max == g) {
            h = 60.0 * (((b - r) / delta) + 2.0);
        } else if (c_max == b) {
            h = 60.0 * (((r - g) / delta) + 4.0);
        }

        init.h = h < 0 ? h + 360.0 : h;
        rgb.set(0, 0, 0);
        return init;
    }

private:
    double m_hue;        
    double m_saturation; 
    double m_lightness;  
};

constexpr Color::RGBInitializer Color::init_rgb(const HSLColor& hsl) noexcept {
    RGBInitializer init{};
    const double h = hsl.hue();
    const double s = hsl.saturation() / 100.0;
    const double l = hsl.lightness() / 100.0;

    if (fizmo::abs_constexpr(s) <= 1e-10) {
        std::uint8_t gray = static_cast<std::uint8_t>(l * 255);
        init.r = gray;
        init.g = gray;
        init.b = gray;
        return init;
    }

    const double q = l < 0.5 ? l * (1 + s) : l + s * (1 - l);
    const double p = 2 * l - q;
    const double h_norm = h / 360.0;
    const double r = hue_to_rgb(p, q, h_norm + 1.0 / 3.0);
    const double g = hue_to_rgb(p, q, h_norm);
    const double b = hue_to_rgb(p, q, h_norm - 1.0 / 3.0);
    init.r = static_cast<std::uint8_t>(r * 255);
    init.g = static_cast<std::uint8_t>(g * 255);
    init.b = static_cast<std::uint8_t>(b * 255);
    return init;
}

constexpr Color::RGBInitializer Color::init_rgb(HSLColor&& hsl) noexcept {
    RGBInitializer init{};
    const double h = hsl.hue();
    const double s = hsl.saturation() / 100.0;
    const double l = hsl.lightness() / 100.0;

    if (fizmo::abs_constexpr(s) <= 1e-10) {
        std::uint8_t gray = static_cast<std::uint8_t>(l * 255);
        init.r = gray;
        init.g = gray;
        init.b = gray;
        hsl.hue() = 0;
        hsl.saturation() = 0;
        hsl.lightness() = 0;
        return init;
    }

    const double q = l < 0.5 ? l * (1 + s) : l + s * (1 - l);
    const double p = 2 * l - q;
    const double h_norm = h / 360.0;
    const double r = hue_to_rgb(p, q, h_norm + 1.0 / 3.0);
    const double g = hue_to_rgb(p, q, h_norm);
    const double b = hue_to_rgb(p, q, h_norm - 1.0 / 3.0);
    init.r = static_cast<std::uint8_t>(r * 255);
    init.g = static_cast<std::uint8_t>(g * 255);
    init.b = static_cast<std::uint8_t>(b * 255);
    hsl.hue() = 0;
    hsl.saturation() = 0;
    hsl.lightness() = 0;
    return init;
}

constexpr Color::Color(const HSLColor& hsl, std::uint8_t a) noexcept : m_red(init_rgb(hsl).r), m_green(init_rgb(hsl).g), m_blue(init_rgb(hsl).b), m_alpha(a) {}

inline OPTIONAL_CPP14_CONSTEXPR Color& Color::operator=(const HSLColor& hsl) noexcept {
    const RGBInitializer init = init_rgb(hsl);
    m_red = init.r;
    m_green = init.g;
    m_blue = init.b;
    return *this;
}

} // namespace graphics
} // namespace fizmo

#endif // HSL_COLOR_HPP