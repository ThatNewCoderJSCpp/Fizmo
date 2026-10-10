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
    static constexpr float FULL_CONE = 180.0f;

    vector3d      position{};
    Color         color         = Color(255, 255, 255);
    float         intensity     = 1.0f;
    float         radius        = 8.0f;
    bool          casts_shadows = false;
    vector3d      direction{ 0.0, 0.0, -1.0 };
    float         cone          = 0.0f;
    float         cone_softness = 0.2f;
    bool          moving        = false;
    std::uint64_t id            = 0;

    PointLight3D() = default;
    PointLight3D(const vector3d& at, const Color& c, float strength, float reach, bool shadows = false) noexcept
        : position(at), color(c), intensity(strength), radius(reach), casts_shadows(shadows) {}

    static PointLight3D spot(const vector3d& at, const vector3d& facing, const Color& c, float strength, float reach, float half_angle, bool shadows = false) noexcept;

    bool is_spot() const noexcept { return cone > 0.0f && cone < FULL_CONE; }
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
    double       angle_step    = 0.5;
    bool         crossfade     = true;
    bool         soft          = false;
    float        light_size    = 1.5f;
    float        max_softness  = 12.0f;
    unsigned int filter_taps   = 4;
    double       recenter      = 0.125;

    static SunShadow3D off() noexcept { return {}; }

    static SunShadow3D low() noexcept;

    static SunShadow3D medium() noexcept {
        SunShadow3D s;
        s.enabled = true; s.resolution = 2048; s.distance = 64.0;
        return s;
    }

    static SunShadow3D high() noexcept;

    static SunShadow3D ultra() noexcept;

    static SunShadow3D crisp() noexcept {
        SunShadow3D s = high();
        s.soft = false; s.softness = 0.5f;
        return s;
    }
};

struct PointShadows3D {
    static constexpr unsigned int MAX_LIGHTS = 8;

    bool         enabled    = false;
    unsigned int max_lights = 4;
    double       fade_distance = 4.0;
    bool         hide_unshadowed = true;
    unsigned int resolution    = 512;
    float        bias          = 0.04f;
    float        normal_offset = 0.04f;
    float        softness      = 1.0f;
    float        strength      = 1.0f;
    unsigned int moving_faces  = 12;

    static PointShadows3D off() noexcept { return {}; }

    static PointShadows3D low() noexcept;

    static PointShadows3D medium() noexcept {
        PointShadows3D p;
        p.enabled = true;
        return p;
    }

    static PointShadows3D high() noexcept;
};

struct CapsuleOccluder3D {
    vector3d start{};
    vector3d end{};
    float    radius = 0.3f;

    CapsuleOccluder3D() = default;
    CapsuleOccluder3D(const vector3d& a, const vector3d& b, float r) noexcept : start(a), end(b), radius(r) {}

    static CapsuleOccluder3D standing(const vector3d& feet, double height, double radius) noexcept;
};

struct CapsuleShadows3D {
    static constexpr std::size_t MAX_CAPSULES = 4;

    bool  enabled    = false;
    float sun_size   = 1.0f;
    float point_size = 0.2f;
    float strength   = 1.0f;

    std::vector<CapsuleOccluder3D> capsules;
};

struct Atmosphere3D {
    bool     enabled       = false;
    vector3d sun_position  { 0.35, 0.2, 1.0 };
    vector3d moon_position { -0.35, -0.2, -1.0 };
    Color    zenith        = Color(70, 130, 235);
    Color    horizon       = Color(175, 210, 250);
    Color    glow          = Color(255, 214, 160);
    float    glow_strength = 0.6f;
    float    sun_radius    = 1.2f;
    float    moon_radius   = 1.6f;
    float    sun_disk      = 6.0f;
    float    moon_disk     = 0.9f;
    Color    sun_disk_color = Color(255, 232, 150);
    float    glow_spread   = 6.0f;
    float    glow_focus    = 64.0f;
    float    stars         = 0.0f;
    float    fog_density   = 0.004f;
    float    fog_start     = 24.0f;
};

struct Volumetrics3D {
    bool         enabled    = false;
    unsigned int steps      = 16;
    float        density    = 0.02f;
    float        anisotropy = 0.6f;
    float        distance   = 96.0f;
    float        intensity  = 1.0f;
    float        near_bias  = 2.0f;
    unsigned int cell_size  = 8;
};

struct LightShafts3D {
    bool         enabled  = false;
    unsigned int samples  = 48;
    float        strength = 0.35f;
    float        decay    = 0.96f;
    float        length   = 0.85f;
    float        focus    = 12.0f;
};

struct Swell3D {
    static constexpr std::size_t COUNT = 3;

    static constexpr float ANGLES[COUNT]     = { 0.0f, 0.5f, -0.7f };
    static constexpr float WAVENUMBER[COUNT] = { 1.0f, 1.37f, 1.83f };
    static constexpr float AMPLITUDE[COUNT]  = { 1.0f, 0.4f, 0.18f };

    float height       = 0.0f;
    float wavenumber   = 0.45f;
    float dir_x        = 1.0f;
    float dir_y        = 0.0f;
    float phase[COUNT] = { 0.0f, 0.0f, 0.0f };
    float detail       = 48.0f;

    bool active() const noexcept { return height > 0.0f; }
};

struct Surfaces3D {
    float wave_strength  = 0.08f;
    float wave_scale     = 0.9f;
    float wave_speed     = 1.2f;
    float reflectivity   = 1.0f;
    float specular_power = 180.0f;
    float specular       = 2.5f;
    bool  refraction          = false;
    float refraction_strength = 1.0f;
    float absorption          = 0.18f;
    float scattering          = 0.35f;
    bool  screen_reflections  = false;
    unsigned int reflection_steps = 32;
    float reflection_distance = 64.0f;
    Swell3D swell;
};

struct ReflectionPlane3D {
    vector3d point{};
    vector3d normal{ 0.0, 0.0, 1.0 };
    vector3d lo{};
    vector3d hi{};
    bool     bounded    = false;
    float    resolution = 0.0f;

    ReflectionPlane3D() = default;
    ReflectionPlane3D(const vector3d& at, const vector3d& facing) noexcept : point(at), normal(facing) {}
    ReflectionPlane3D(const vector3d& at, const vector3d& facing, const vector3d& min_corner, const vector3d& max_corner) noexcept
        : point(at), normal(facing), lo(min_corner), hi(max_corner), bounded(true) {}
};

struct PlanarReflections3D {
    static constexpr unsigned int MAX_PLANES = 2;

    bool         enabled    = false;
    unsigned int max_planes = MAX_PLANES;
    float        resolution = 1.0f;
    float        distortion = 0.02f;
    std::vector<ReflectionPlane3D> planes;
};

struct Medium3D {
    bool  active  = false;
    Color color   = Color(30, 70, 120);
    float density = 0.12f;
};

struct ToneMap3D {
    bool  enabled    = false;
    float exposure   = 1.0f;
    float saturation = 1.0f;
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
    CapsuleShadows3D capsule_shadows;
    Atmosphere3D   atmosphere;
    Volumetrics3D  volumetrics;
    LightShafts3D  shafts;
    PlanarReflections3D planar;
    Surfaces3D     surfaces;
    Medium3D       medium;
    ToneMap3D      tone_map;
    double         time = 0.0;

    std::vector<PointLight3D> point_lights;

    static SceneLighting3D off() { return {}; }

    static SceneLighting3D daylight(const SunShadow3D& shadows = SunShadow3D::medium());

    static SceneLighting3D golden_hour(const SunShadow3D& shadows = SunShadow3D::high());

    static SceneLighting3D overcast();

    static SceneLighting3D night();

    static SceneLighting3D indoor();

    static SceneLighting3D studio();
};

namespace detail {

inline float light_curve(float level, float falloff) noexcept {
    const float k = falloff < 1.0f ? 1.0f : falloff;
    return level / (k - (k - 1.0f) * level);
}

float light_channel(const Color& c, int channel) noexcept;

} // namespace detail

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_LIGHTING_3D_HPP