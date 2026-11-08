#include "color.hpp"

#include <array>

namespace fizmo {
namespace lighting {

float srgb_to_linear(float v) noexcept {
    return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

float linear_to_srgb(float v) noexcept {
    if (!(v > 0.0f)) return 0.0f;
    return v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

Rgb Rgb::from_srgb8(std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept {
    return from_srgb(static_cast<float>(red) / 255.0f, static_cast<float>(green) / 255.0f, static_cast<float>(blue) / 255.0f);
}

Rgb Rgb::from_srgb(float red, float green, float blue) noexcept {
    return { srgb_to_linear(red), srgb_to_linear(green), srgb_to_linear(blue) };
}

Rgb Rgb::from_hex(std::uint32_t rgb) noexcept {
    return from_srgb8(static_cast<std::uint8_t>((rgb >> 16) & 0xFFu), static_cast<std::uint8_t>((rgb >> 8) & 0xFFu), static_cast<std::uint8_t>(rgb & 0xFFu));
}

void Rgb::to_srgb8(std::uint8_t& red, std::uint8_t& green, std::uint8_t& blue) const noexcept {
    auto q = [](float v) { return static_cast<std::uint8_t>(std::lround(clamp01(linear_to_srgb(v)) * 255.0f)); };
    red = q(r);
    green = q(g);
    blue = q(b);
}

namespace spectral {

namespace {

float lobe(float x, float mu, float s1, float s2) noexcept {
    const float t = (x - mu) / (x < mu ? s1 : s2);
    return std::exp(-0.5f * t * t);
}

Rgb basis(float nm) noexcept {
    const float s1 = smoothstep(480.0f, 510.0f, nm);
    const float s2 = smoothstep(570.0f, 610.0f, nm);
    return { s2, s1 - s2, 1.0f - s1 };
}

Rgb raw_weight(float nm) noexcept {
    return xyz_to_linear_srgb(cie_xyz(nm));
}

struct Calibration {
    std::array<float, 9> inverse{};
    float white = 1.0f;

    Calibration() noexcept {
        double m[3][3] = {};
        const int steps = 2000;
        const double lo = 360.0, hi = 830.0, h = (hi - lo) / steps;
        for (int s = 0; s <= steps; ++s) {
            const float nm = static_cast<float>(lo + h * s);
            const double w = (s == 0 || s == steps) ? 0.5 * h : h;
            const Rgb bs = basis(nm);
            const Rgb wt = raw_weight(nm);
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) m[i][j] += w * static_cast<double>(wt[i]) * static_cast<double>(bs[j]);
            }
        }
        const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        const double inv[9] = {
            (m[1][1] * m[2][2] - m[1][2] * m[2][1]) / det, (m[0][2] * m[2][1] - m[0][1] * m[2][2]) / det, (m[0][1] * m[1][2] - m[0][2] * m[1][1]) / det,
            (m[1][2] * m[2][0] - m[1][0] * m[2][2]) / det, (m[0][0] * m[2][2] - m[0][2] * m[2][0]) / det, (m[0][2] * m[1][0] - m[0][0] * m[1][2]) / det,
            (m[1][0] * m[2][1] - m[1][1] * m[2][0]) / det, (m[0][1] * m[2][0] - m[0][0] * m[2][1]) / det, (m[0][0] * m[1][1] - m[0][1] * m[1][0]) / det,
        };
        for (int i = 0; i < 9; ++i) inverse[static_cast<std::size_t>(i)] = static_cast<float>(inv[i]);
    }
};

const Calibration& calibration() noexcept {
    static const Calibration c;
    return c;
}

} // namespace

Vec3f cie_xyz(float nm) noexcept {
    const float x = 1.056f * lobe(nm, 599.8f, 37.9f, 31.0f) + 0.362f * lobe(nm, 442.0f, 16.0f, 26.7f) - 0.065f * lobe(nm, 501.1f, 20.4f, 26.2f);
    const float y = 0.821f * lobe(nm, 568.8f, 46.9f, 40.5f) + 0.286f * lobe(nm, 530.9f, 16.3f, 31.1f);
    const float z = 1.217f * lobe(nm, 437.0f, 11.8f, 36.0f) + 0.681f * lobe(nm, 459.0f, 26.0f, 13.8f);
    return { x, y, z };
}

Rgb xyz_to_linear_srgb(const Vec3f& c) noexcept {
    return {
        3.2404542f * c.x - 1.5371385f * c.y - 0.4985314f * c.z,
        -0.9692660f * c.x + 1.8760108f * c.y + 0.0415560f * c.z,
        0.0556434f * c.x - 0.2040259f * c.y + 1.0572252f * c.z,
    };
}

float blackbody(float nm, float kelvin) noexcept {
    if (kelvin <= 0.0f) return 0.0f;
    const double c = 299792458.0, h = 6.62606957e-34, kb = 1.3806488e-23;
    const double l = static_cast<double>(nm) * 1e-9;
    const double le = (2.0 * h * c * c) / (l * l * l * l * l * (std::exp((h * c) / (l * kb * kelvin)) - 1.0));
    return static_cast<float>(le);
}

float blackbody_normalized(float nm, float kelvin) noexcept {
    const float peak = static_cast<float>(2.8977721e-3 / static_cast<double>(kelvin) * 1e9);
    const float p = blackbody(peak, kelvin);
    return p > 0.0f ? blackbody(nm, kelvin) / p : 0.0f;
}

float uplift(const Rgb& color, float nm) noexcept {
    const Rgb bs = basis(nm);
    return color.r * bs.r + color.g * bs.g + color.b * bs.b;
}

Rgb wavelength_weight(float nm) noexcept {
    const Rgb w = raw_weight(nm);
    const auto& m = calibration().inverse;
    return { m[0] * w.r + m[1] * w.g + m[2] * w.b, m[3] * w.r + m[4] * w.g + m[5] * w.b, m[6] * w.r + m[7] * w.g + m[8] * w.b };
}

float sample_wavelength(float u, float& pdf) noexcept {
    const float nm = 538.0f - 138.888889f * std::atanh(0.85691062f - 1.82750197f * u);
    const float c = std::cosh(0.0072f * (nm - 538.0f));
    pdf = 0.003939804229f / (c * c);
    return nm;
}

} // namespace spectral

Rgb Rgb::from_kelvin(float kelvin) noexcept {
    if (kelvin <= 0.0f) return Rgb(1.0f);
    Vec3f xyz{};
    for (int i = 0; i <= 90; ++i) {
        const float nm = 380.0f + 5.0f * static_cast<float>(i);
        xyz += spectral::cie_xyz(nm) * spectral::blackbody_normalized(nm, kelvin);
    }
    Rgb c = spectral::xyz_to_linear_srgb(xyz);
    c = Rgb(std::max(c.r, 0.0f), std::max(c.g, 0.0f), std::max(c.b, 0.0f));
    const float y = c.luminance();
    return y > 0.0f ? c / y : Rgb(1.0f);
}

Rgb Rgb::from_wavelength(float nm) noexcept {
    Rgb c = spectral::xyz_to_linear_srgb(spectral::cie_xyz(nm));
    c = Rgb(std::max(c.r, 0.0f), std::max(c.g, 0.0f), std::max(c.b, 0.0f));
    const float m = c.max_component();
    return m > 0.0f ? c / m : Rgb();
}

} // namespace lighting
} // namespace fizmo
