#ifndef FIZMO_LIGHTING_LIGHT_HPP
#define FIZMO_LIGHTING_LIGHT_HPP

#include "color.hpp"
#include "ies.hpp"
#include "math.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fizmo {
namespace lighting {

enum class LightShape : std::uint8_t {
    point,
    spot,
    directional,
    sphere,
    disk,
    rectangle,
    tube
};

enum class LightMobility : std::uint8_t {
    dynamic,
    stationary,
    baked
};

enum class LightUnit : std::uint8_t {
    unitless,
    candela,
    lumens,
    lux,
    nits,
    watts
};

enum class LightSpread : std::uint8_t {
    natural,
    blob
};

enum class Falloff : std::uint8_t {
    physical,
    linear,
    smooth,
    constant,
    custom
};

const char* to_string(LightShape v) noexcept;
const char* to_string(LightMobility v) noexcept;
const char* to_string(LightUnit v) noexcept;
const char* to_string(Falloff v) noexcept;
std::optional<LightShape> parse_light_shape(const std::string& text) noexcept;
std::optional<LightMobility> parse_light_mobility(const std::string& text) noexcept;
std::optional<LightUnit> parse_light_unit(const std::string& text) noexcept;
std::optional<Falloff> parse_falloff(const std::string& text) noexcept;

struct LightCookie {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<Rgb> texels;

    static LightCookie from_rgba8(unsigned width, unsigned height, const std::uint8_t* rgba);
    Rgb sample(float u, float v) const noexcept;
};

struct Light {
    std::string name;
    std::uint64_t id = 0;
    bool enabled = true;
    LightShape shape = LightShape::point;
    LightMobility mobility = LightMobility::dynamic;

    Vec3 position{ 0.0, 0.0, 0.0 };
    Vec3 direction{ 0.0, -1.0, 0.0 };
    Vec3 up{ 0.0, 0.0, 1.0 };

    Rgb color{ 1.0f, 1.0f, 1.0f };
    float temperature = 0.0f;
    float intensity = 1.0f;
    LightUnit unit = LightUnit::unitless;

    float range = 0.0f;
    Falloff falloff = Falloff::physical;
    float falloff_exponent = 2.0f;
    LightSpread spread = LightSpread::natural;
    float spill = 0.35f;

    float radius = 0.0f;
    float length = 1.0f;
    float width = 1.0f;
    float height = 1.0f;
    bool two_sided = false;

    float inner_cone = 0.0f;
    float outer_cone = 0.0f;
    float angular_diameter = 0.53f;

    bool casts_shadows = true;
    float shadow_strength = 1.0f;
    float shadow_bias = 0.0f;
    int shadow_samples = 0;

    float diffuse = 1.0f;
    float specular = 1.0f;
    float indirect = 1.0f;
    float volumetric = 1.0f;
    bool visible = true;
    std::uint32_t channels = 1u;

    std::shared_ptr<const IesProfile> ies;
    std::shared_ptr<const LightCookie> cookie;
    float cookie_size = 1.0f;

    static Light point_light(const Vec3& at, const Rgb& color, float intensity, float range = 0.0f);
    static Light spot_light(const Vec3& at, const Vec3& facing, const Rgb& color, float intensity, float outer_degrees, float inner_degrees = -1.0f, float range = 0.0f);
    static Light sun(const Vec3& toward_ground, const Rgb& color, float illuminance, float angular_diameter_degrees = 0.53f);
    static Light sphere_light(const Vec3& at, float radius, const Rgb& color, float intensity, float range = 0.0f);
    static Light disk_light(const Vec3& at, const Vec3& facing, float radius, const Rgb& color, float intensity);
    static Light rectangle_light(const Vec3& at, const Vec3& facing, const Vec3& up, float width, float height, const Rgb& color, float intensity);
    static Light tube_light(const Vec3& center, const Vec3& axis, float length, float radius, const Rgb& color, float intensity);

    Light& with_temperature(float kelvin) { temperature = kelvin; return *this; }
    Light& with_unit(LightUnit u) { unit = u; return *this; }
    Light& with_mobility(LightMobility m) { mobility = m; return *this; }
    Light& with_shadows(bool on) { casts_shadows = on; return *this; }
    Light& with_range(float r) { range = r; return *this; }
    Light& with_name(std::string n) { name = std::move(n); return *this; }

    Rgb tint() const noexcept;
    bool is_local() const noexcept { return shape != LightShape::directional; }
    bool is_area() const noexcept;
    bool has_cone() const noexcept;
    float area() const noexcept;
    float emitted_scale() const noexcept;
    float luminous_power() const noexcept;
};

float exposure_from_ev100(float ev100) noexcept;
float ev100_from_camera(float aperture, float shutter_seconds, float iso) noexcept;

namespace units {

constexpr float kLuminousEfficacy = 683.0f;
float lumens_to_candela(float lumens, float cone_half_degrees = 180.0f) noexcept;
float candela_to_lumens(float candela, float cone_half_degrees = 180.0f) noexcept;
float watts_to_lumens(float watts) noexcept;
float lumens_to_nits(float lumens, float area, bool two_sided) noexcept;

} // namespace units

} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_LIGHT_HPP
