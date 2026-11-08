#include "light.hpp"

#include <cctype>

namespace fizmo {
namespace lighting {

namespace {

std::string lower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '_' || c == ' ') c = '-';
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

float cone_solid_angle(float half_degrees) noexcept {
    if (half_degrees <= 0.0f || half_degrees >= 180.0f) return 4.0f * kPi;
    return kTwoPi * (1.0f - std::cos(half_degrees * kDegToRad));
}

} // namespace

const char* to_string(LightShape v) noexcept {
    switch (v) {
    case LightShape::point: return "point";
    case LightShape::spot: return "spot";
    case LightShape::directional: return "directional";
    case LightShape::sphere: return "sphere";
    case LightShape::disk: return "disk";
    case LightShape::rectangle: return "rectangle";
    case LightShape::tube: return "tube";
    }
    return "point";
}

const char* to_string(LightMobility v) noexcept {
    switch (v) {
    case LightMobility::dynamic: return "dynamic";
    case LightMobility::stationary: return "stationary";
    case LightMobility::baked: return "baked";
    }
    return "dynamic";
}

const char* to_string(LightUnit v) noexcept {
    switch (v) {
    case LightUnit::unitless: return "unitless";
    case LightUnit::candela: return "candela";
    case LightUnit::lumens: return "lumens";
    case LightUnit::lux: return "lux";
    case LightUnit::nits: return "nits";
    case LightUnit::watts: return "watts";
    }
    return "unitless";
}

const char* to_string(Falloff v) noexcept {
    switch (v) {
    case Falloff::physical: return "physical";
    case Falloff::linear: return "linear";
    case Falloff::smooth: return "smooth";
    case Falloff::constant: return "constant";
    case Falloff::custom: return "custom";
    }
    return "physical";
}

std::optional<LightShape> parse_light_shape(const std::string& text) noexcept {
    const std::string s = lower(text);
    if (s == "point" || s == "omni") return LightShape::point;
    if (s == "spot" || s == "cone") return LightShape::spot;
    if (s == "directional" || s == "sun" || s == "direction") return LightShape::directional;
    if (s == "sphere" || s == "ball") return LightShape::sphere;
    if (s == "disk" || s == "disc" || s == "circle") return LightShape::disk;
    if (s == "rectangle" || s == "rect" || s == "area" || s == "quad" || s == "panel" || s == "window") return LightShape::rectangle;
    if (s == "tube" || s == "line" || s == "capsule" || s == "linear") return LightShape::tube;
    return std::nullopt;
}

std::optional<LightMobility> parse_light_mobility(const std::string& text) noexcept {
    const std::string s = lower(text);
    if (s == "dynamic" || s == "movable" || s == "realtime" || s == "real-time") return LightMobility::dynamic;
    if (s == "stationary" || s == "mixed") return LightMobility::stationary;
    if (s == "baked" || s == "static") return LightMobility::baked;
    return std::nullopt;
}

std::optional<LightUnit> parse_light_unit(const std::string& text) noexcept {
    const std::string s = lower(text);
    if (s == "unitless" || s == "none" || s == "artist") return LightUnit::unitless;
    if (s == "candela" || s == "cd") return LightUnit::candela;
    if (s == "lumens" || s == "lumen" || s == "lm") return LightUnit::lumens;
    if (s == "lux" || s == "lx") return LightUnit::lux;
    if (s == "nits" || s == "nit" || s == "cd/m2") return LightUnit::nits;
    if (s == "watts" || s == "watt" || s == "w") return LightUnit::watts;
    return std::nullopt;
}

std::optional<Falloff> parse_falloff(const std::string& text) noexcept {
    const std::string s = lower(text);
    if (s == "physical" || s == "inverse-square" || s == "realistic") return Falloff::physical;
    if (s == "linear") return Falloff::linear;
    if (s == "smooth" || s == "quadratic") return Falloff::smooth;
    if (s == "constant" || s == "none") return Falloff::constant;
    if (s == "custom" || s == "exponent") return Falloff::custom;
    return std::nullopt;
}

LightCookie LightCookie::from_rgba8(unsigned width, unsigned height, const std::uint8_t* rgba) {
    LightCookie c;
    c.width = width;
    c.height = height;
    c.texels.resize(static_cast<std::size_t>(width) * height);
    for (std::size_t i = 0; i < c.texels.size(); ++i) c.texels[i] = Rgb::from_srgb8(rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2]);
    return c;
}

Rgb LightCookie::sample(float u, float v) const noexcept {
    if (width == 0 || height == 0 || texels.empty()) return Rgb(1.0f);
    u = u - std::floor(u);
    v = v - std::floor(v);
    const float x = u * static_cast<float>(width) - 0.5f, y = v * static_cast<float>(height) - 0.5f;
    const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0);
    auto at = [this](int px, int py) {
        px = ((px % static_cast<int>(width)) + static_cast<int>(width)) % static_cast<int>(width);
        py = ((py % static_cast<int>(height)) + static_cast<int>(height)) % static_cast<int>(height);
        return texels[static_cast<std::size_t>(py) * width + static_cast<std::size_t>(px)];
    };
    const Rgb a = at(x0, y0) * (1.0f - fx) + at(x0 + 1, y0) * fx;
    const Rgb b = at(x0, y0 + 1) * (1.0f - fx) + at(x0 + 1, y0 + 1) * fx;
    return a * (1.0f - fy) + b * fy;
}

Light Light::point_light(const Vec3& at, const Rgb& c, float strength, float reach) {
    Light l;
    l.shape = LightShape::point;
    l.position = at;
    l.color = c;
    l.intensity = strength;
    l.range = reach;
    return l;
}

Light Light::spot_light(const Vec3& at, const Vec3& facing, const Rgb& c, float strength, float outer_degrees, float inner_degrees, float reach) {
    Light l = point_light(at, c, strength, reach);
    l.shape = LightShape::spot;
    l.direction = facing.normalized();
    l.outer_cone = outer_degrees;
    l.inner_cone = inner_degrees < 0.0f ? outer_degrees * 0.8f : inner_degrees;
    return l;
}

Light Light::sun(const Vec3& toward_ground, const Rgb& c, float illuminance, float angular) {
    Light l;
    l.shape = LightShape::directional;
    l.direction = toward_ground.normalized();
    l.color = c;
    l.intensity = illuminance;
    l.angular_diameter = angular;
    return l;
}

Light Light::sphere_light(const Vec3& at, float r, const Rgb& c, float strength, float reach) {
    Light l = point_light(at, c, strength, reach);
    l.shape = LightShape::sphere;
    l.radius = r;
    return l;
}

Light Light::disk_light(const Vec3& at, const Vec3& facing, float r, const Rgb& c, float strength) {
    Light l = point_light(at, c, strength);
    l.shape = LightShape::disk;
    l.direction = facing.normalized();
    l.radius = r;
    return l;
}

Light Light::rectangle_light(const Vec3& at, const Vec3& facing, const Vec3& up_dir, float w, float h, const Rgb& c, float strength) {
    Light l = point_light(at, c, strength);
    l.shape = LightShape::rectangle;
    l.direction = facing.normalized();
    l.up = up_dir.normalized();
    l.width = w;
    l.height = h;
    return l;
}

Light Light::tube_light(const Vec3& center, const Vec3& axis, float len, float r, const Rgb& c, float strength) {
    Light l = point_light(center, c, strength);
    l.shape = LightShape::tube;
    l.direction = axis.normalized();
    l.length = len;
    l.radius = r;
    return l;
}

Rgb Light::tint() const noexcept {
    return temperature > 0.0f ? color * Rgb::from_kelvin(temperature) : color;
}

bool Light::is_area() const noexcept {
    switch (shape) {
    case LightShape::sphere: return radius > 0.0f;
    case LightShape::disk: return radius > 0.0f;
    case LightShape::rectangle: return width > 0.0f && height > 0.0f;
    case LightShape::tube: return length > 0.0f || radius > 0.0f;
    default: return false;
    }
}

bool Light::has_cone() const noexcept {
    if (shape == LightShape::directional) return false;
    if (shape == LightShape::spot) return outer_cone > 0.0f && outer_cone < 180.0f;
    return outer_cone > 0.0f && outer_cone < 180.0f;
}

float Light::area() const noexcept {
    switch (shape) {
    case LightShape::sphere: return 4.0f * kPi * radius * radius;
    case LightShape::disk: return kPi * radius * radius;
    case LightShape::rectangle: return width * height;
    case LightShape::tube: {
        const float r = std::max(radius, 1e-4f);
        return kTwoPi * r * std::max(length, 0.0f) + 4.0f * kPi * r * r;
    }
    default: return 0.0f;
    }
}

float Light::emitted_scale() const noexcept {
    const float watts_or_lumens = unit == LightUnit::watts ? units::watts_to_lumens(intensity) : intensity;
    if (shape == LightShape::directional) return unit == LightUnit::watts ? watts_or_lumens : intensity;
    if (!is_area()) {
        if (unit == LightUnit::lumens || unit == LightUnit::watts) return units::lumens_to_candela(watts_or_lumens, has_cone() ? outer_cone : 180.0f);
        return intensity;
    }
    if (unit == LightUnit::nits) return intensity;
    if (unit == LightUnit::lumens || unit == LightUnit::watts) {
        const bool both = two_sided && (shape == LightShape::disk || shape == LightShape::rectangle);
        return units::lumens_to_nits(watts_or_lumens, area(), both);
    }
    float projected = 0.0f;
    switch (shape) {
    case LightShape::sphere: projected = kPi * radius * radius; break;
    case LightShape::disk: projected = kPi * radius * radius; break;
    case LightShape::rectangle: projected = width * height; break;
    case LightShape::tube: {
        const float r = std::max(radius, 1e-4f);
        projected = 2.0f * r * std::max(length, 0.0f) + kPi * r * r;
        break;
    }
    default: break;
    }
    return projected > 0.0f ? intensity / projected : intensity;
}

float Light::luminous_power() const noexcept {
    const float s = emitted_scale() * tint().luminance();
    if (shape == LightShape::directional) return s;
    if (!is_area()) return s * cone_solid_angle(has_cone() ? outer_cone : 180.0f);
    const bool both = two_sided && (shape == LightShape::disk || shape == LightShape::rectangle);
    return s * kPi * area() * (both ? 2.0f : 1.0f);
}

float exposure_from_ev100(float ev100) noexcept {
    return 1.0f / (1.2f * std::pow(2.0f, ev100));
}

float ev100_from_camera(float aperture, float shutter_seconds, float iso) noexcept {
    return std::log2((aperture * aperture) / shutter_seconds * 100.0f / iso);
}

namespace units {

float lumens_to_candela(float lumens, float cone_half_degrees) noexcept {
    return lumens / cone_solid_angle(cone_half_degrees);
}

float candela_to_lumens(float candela, float cone_half_degrees) noexcept {
    return candela * cone_solid_angle(cone_half_degrees);
}

float watts_to_lumens(float watts) noexcept {
    return watts * kLuminousEfficacy;
}

float lumens_to_nits(float lumens, float area, bool two_sided) noexcept {
    if (area <= 0.0f) return 0.0f;
    return lumens / (kPi * area * (two_sided ? 2.0f : 1.0f));
}

} // namespace units

} // namespace lighting
} // namespace fizmo
