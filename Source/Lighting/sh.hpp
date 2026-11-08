#ifndef FIZMO_LIGHTING_SH_HPP
#define FIZMO_LIGHTING_SH_HPP

#include "color.hpp"

#include <array>

namespace fizmo {
namespace lighting {

struct Sh9 {
    std::array<Rgb, 9> c{};

    static std::array<float, 9> basis(const Vec3f& d) noexcept {
        return {
            0.282095f,
            0.488603f * d.y,
            0.488603f * d.z,
            0.488603f * d.x,
            1.092548f * d.x * d.y,
            1.092548f * d.y * d.z,
            0.315392f * (3.0f * d.z * d.z - 1.0f),
            1.092548f * d.x * d.z,
            0.546274f * (d.x * d.x - d.y * d.y),
        };
    }

    void add(const Vec3f& dir, const Rgb& radiance, float weight) noexcept {
        const auto y = basis(dir);
        for (int i = 0; i < 9; ++i) c[static_cast<std::size_t>(i)] += radiance * (y[static_cast<std::size_t>(i)] * weight);
    }

    void scale(float s) noexcept {
        for (auto& v : c) v *= s;
    }

    void lerp_to(const Sh9& o, float t) noexcept {
        for (std::size_t i = 0; i < 9; ++i) c[i] = c[i] * (1.0f - t) + o.c[i] * t;
    }

    Rgb irradiance(const Vec3f& n) const noexcept {
        const float a0 = kPi, a1 = 2.0f * kPi / 3.0f, a2 = kPi / 4.0f;
        const auto y = basis(n);
        Rgb e = c[0] * (a0 * y[0]);
        for (std::size_t i = 1; i < 4; ++i) e += c[i] * (a1 * y[i]);
        for (std::size_t i = 4; i < 9; ++i) e += c[i] * (a2 * y[i]);
        return Rgb(std::max(e.r, 0.0f), std::max(e.g, 0.0f), std::max(e.b, 0.0f));
    }

    Rgb average() const noexcept { return c[0] * 0.282095f; }
};

} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_SH_HPP
