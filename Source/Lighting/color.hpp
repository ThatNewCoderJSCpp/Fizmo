#ifndef FIZMO_LIGHTING_COLOR_HPP
#define FIZMO_LIGHTING_COLOR_HPP

#include "math.hpp"

#include <cstdint>

namespace fizmo {
namespace lighting {

struct Rgb {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;

    constexpr Rgb() noexcept = default;
    constexpr Rgb(float red, float green, float blue) noexcept : r(red), g(green), b(blue) {}
    explicit constexpr Rgb(float v) noexcept : r(v), g(v), b(v) {}

    static Rgb from_srgb8(std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept;
    static Rgb from_srgb(float red, float green, float blue) noexcept;
    static Rgb from_hex(std::uint32_t rgb) noexcept;
    static Rgb from_kelvin(float kelvin) noexcept;
    static Rgb from_wavelength(float nanometers) noexcept;

    constexpr float operator[](int i) const noexcept { return i == 0 ? r : (i == 1 ? g : b); }
    float& operator[](int i) noexcept { return i == 0 ? r : (i == 1 ? g : b); }
    constexpr Rgb operator+(const Rgb& o) const noexcept { return { r + o.r, g + o.g, b + o.b }; }
    constexpr Rgb operator-(const Rgb& o) const noexcept { return { r - o.r, g - o.g, b - o.b }; }
    constexpr Rgb operator*(const Rgb& o) const noexcept { return { r * o.r, g * o.g, b * o.b }; }
    constexpr Rgb operator*(float s) const noexcept { return { r * s, g * s, b * s }; }
    constexpr Rgb operator/(float s) const noexcept { return { r / s, g / s, b / s }; }
    Rgb& operator+=(const Rgb& o) noexcept { r += o.r; g += o.g; b += o.b; return *this; }
    Rgb& operator*=(const Rgb& o) noexcept { r *= o.r; g *= o.g; b *= o.b; return *this; }
    Rgb& operator*=(float s) noexcept { r *= s; g *= s; b *= s; return *this; }
    constexpr bool operator==(const Rgb& o) const noexcept { return r == o.r && g == o.g && b == o.b; }
    constexpr bool operator!=(const Rgb& o) const noexcept { return !(*this == o); }

    constexpr float luminance() const noexcept { return 0.2126f * r + 0.7152f * g + 0.0722f * b; }
    constexpr float max_component() const noexcept { return r > g ? (r > b ? r : b) : (g > b ? g : b); }
    constexpr bool is_black() const noexcept { return r <= 0.0f && g <= 0.0f && b <= 0.0f; }
    bool is_finite() const noexcept { return std::isfinite(r) && std::isfinite(g) && std::isfinite(b); }
    Rgb clamped(float lo, float hi) const noexcept { return { std::clamp(r, lo, hi), std::clamp(g, lo, hi), std::clamp(b, lo, hi) }; }
    void to_srgb8(std::uint8_t& red, std::uint8_t& green, std::uint8_t& blue) const noexcept;
};

constexpr Rgb operator*(float s, const Rgb& c) noexcept { return c * s; }

float srgb_to_linear(float v) noexcept;
float linear_to_srgb(float v) noexcept;

namespace spectral {

constexpr float kMinWavelength = 380.0f;
constexpr float kMaxWavelength = 780.0f;
constexpr float kWavelengthRange = kMaxWavelength - kMinWavelength;
constexpr float kSodiumD = 589.3f;
constexpr float kHydrogenF = 486.1f;
constexpr float kHydrogenC = 656.3f;

Vec3f cie_xyz(float nanometers) noexcept;
Rgb xyz_to_linear_srgb(const Vec3f& xyz) noexcept;
float blackbody(float nanometers, float kelvin) noexcept;
float blackbody_normalized(float nanometers, float kelvin) noexcept;
float uplift(const Rgb& color, float nanometers) noexcept;
Rgb wavelength_weight(float nanometers) noexcept;
float sample_wavelength(float u, float& pdf) noexcept;

} // namespace spectral

} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_COLOR_HPP
