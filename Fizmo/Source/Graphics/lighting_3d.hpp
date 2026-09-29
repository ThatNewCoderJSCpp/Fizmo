#ifndef FIZMO_LIGHTING_3D_HPP
#define FIZMO_LIGHTING_3D_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include "color.hpp"
#include "../Vectors/vectors.hpp"

namespace fizmo {
namespace graphics {

struct BakedLight {
    std::uint8_t red   = 0;
    std::uint8_t green = 0;
    std::uint8_t blue  = 0;
    std::uint8_t sky   = 255;

    static constexpr std::uint8_t FULL = 255;

    constexpr BakedLight() noexcept = default;
    constexpr BakedLight(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t sky_level) noexcept
        : red(r), green(g), blue(b), sky(sky_level) {}

    static constexpr BakedLight full_sky() noexcept { return BakedLight(0, 0, 0, FULL); }
    static constexpr BakedLight full_bright() noexcept { return BakedLight(FULL, FULL, FULL, FULL); }
    static constexpr BakedLight dark() noexcept { return BakedLight(0, 0, 0, 0); }
    static constexpr BakedLight white(std::uint8_t block, std::uint8_t sky_level) noexcept { return BakedLight(block, block, block, sky_level); }

    constexpr std::uint32_t packed() const noexcept {
        return static_cast<std::uint32_t>(red) | (static_cast<std::uint32_t>(green) << 8)
             | (static_cast<std::uint32_t>(blue) << 16) | (static_cast<std::uint32_t>(sky) << 24);
    }

    static constexpr BakedLight unpack(std::uint32_t v) noexcept {
        return BakedLight(static_cast<std::uint8_t>(v & 0xFFu), static_cast<std::uint8_t>((v >> 8) & 0xFFu),
                          static_cast<std::uint8_t>((v >> 16) & 0xFFu), static_cast<std::uint8_t>(v >> 24));
    }

    constexpr bool operator==(const BakedLight& o) const noexcept { return packed() == o.packed(); }
    constexpr bool operator!=(const BakedLight& o) const noexcept { return packed() != o.packed(); }
};

constexpr std::uint32_t LIGHT_FULL_SKY = BakedLight::full_sky().packed();

struct PointLight3D {
    vector3d position{};
    Color    color         = Color(255, 255, 255);
    float    intensity     = 1.0f;
    float    radius        = 8.0f;
    bool     casts_shadows = false;

    PointLight3D() = default;
    PointLight3D(const vector3d& at, const Color& c, float strength, float reach, bool shadows = false) noexcept
        : position(at), color(c), intensity(strength), radius(reach), casts_shadows(shadows) {}
};

struct SunShadow3D {
    bool         enabled       = false;
    unsigned int resolution    = 2048;
    double       distance      = 64.0;
    double       depth_range   = 192.0;
    float        bias          = 0.0005f;
    float        normal_offset = 1.5f;
    float        softness      = 1.0f;
    float        strength      = 1.0f;
};

struct PointShadows3D {
    static constexpr unsigned int MAX_LIGHTS = 4;

    bool         enabled    = false;
    unsigned int max_lights = MAX_LIGHTS;
    unsigned int resolution    = 256;
    float        bias          = 0.04f;
    float        normal_offset = 0.04f;
    float        strength      = 1.0f;
};

struct SceneLighting3D {
    static constexpr std::size_t MAX_POINT_LIGHTS = 64;

    bool     enabled = false;

    vector3d sun_direction{ 0.35, 0.2, -1.0 };
    Color    sun_color     = Color(255, 244, 222);
    float    sun_intensity = 0.0f;
    float    sun_exposure  = 4.0f;

    Color    sky_color     = Color(255, 255, 255);
    float    sky_intensity = 1.0f;

    Color    block_color     = Color(255, 255, 255);
    float    block_intensity = 1.0f;

    float    ambient   = 0.0f;
    float    min_light = 0.04f;
    float    falloff   = 1.0f;
    float    max_light = 1.0f;

    SunShadow3D    sun_shadow;
    PointShadows3D point_shadows;

    std::vector<PointLight3D> point_lights;

    static SceneLighting3D off() { return {}; }
};

namespace detail {

inline float light_curve(float level, float falloff) noexcept {
    const float k = falloff < 1.0f ? 1.0f : falloff;
    return level / (k - (k - 1.0f) * level);
}

inline float light_channel(const Color& c, int channel) noexcept {
    const std::uint8_t v = channel == 0 ? c.red() : (channel == 1 ? c.green() : c.blue());
    return static_cast<float>(v) / 255.0f;
}

} // namespace detail

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_LIGHTING_3D_HPP