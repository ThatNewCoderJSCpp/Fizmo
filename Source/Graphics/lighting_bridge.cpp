#include "fizmo_library.hpp"
#include "lighting_bridge.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace graphics {

namespace {

Color to_color(const lighting::Rgb& c, float& scale) {
    const float m = std::max(c.max_component(), 1e-6f);
    scale = m;
    auto q = [m](float v) { return static_cast<std::uint8_t>(std::lround(std::clamp(lighting::linear_to_srgb(v / m), 0.0f, 1.0f) * 255.0f)); };
    return Color(q(c.r), q(c.g), q(c.b));
}

linalg::vector3d to_fizmo(const lighting::Vec3& v) {
    return linalg::vector3d{ v.x, v.y, v.z };
}

float candela_equivalent(const lighting::Light& l) {
    using lighting::LightShape;
    const float s = l.emitted_scale();
    if (!l.is_area()) return s;
    const float kPi = lighting::kPi;
    switch (l.shape) {
    case LightShape::sphere:
    case LightShape::disk: return s * kPi * l.radius * l.radius;
    case LightShape::rectangle: return s * l.width * l.height;
    case LightShape::tube: {
        const float r = std::max(l.radius, 0.005f);
        return s * (2.0f * r * l.length + kPi * r * r);
    }
    default: return s;
    }
}

} // namespace

PointLight3D to_point_light(const lighting::Light& l, float scale) {
    using lighting::LightShape;
    float peak = 1.0f;
    PointLight3D p;
    p.position = to_fizmo(l.position);
    p.color = to_color(l.tint(), peak);
    const float candela = candela_equivalent(l) * peak * scale;
    p.intensity = candela;
    p.radius = l.range > 0.0f ? l.range : std::max(1.0f, std::sqrt(std::max(candela, 0.0f) * 64.0f));
    p.casts_shadows = l.casts_shadows;
    p.direction = to_fizmo(l.direction);
    if (l.has_cone()) {
        p.cone = l.outer_cone;
        p.cone_softness = l.outer_cone > 0.0f ? std::clamp(1.0f - l.inner_cone / l.outer_cone, 0.0f, 1.0f) : 0.2f;
    }
    p.moving = l.mobility == lighting::LightMobility::dynamic;
    p.id = l.id;
    p.two_sided = l.two_sided;
    if (l.spread == lighting::LightSpread::natural) {
        p.falloff = LightFalloff3D::Physical;
        p.reference_distance = std::max(0.25f, l.radius > 0.0f ? l.radius * 4.0f : 1.0f);
        p.spill = std::max(0.0f, l.spill);
        if (l.range <= 0.0f) p.radius = std::max(p.radius, std::sqrt(std::max(candela, 0.0f) * 400.0f));
    }
    switch (l.shape) {
    case LightShape::sphere:
        p.shape = LightShape3D::Sphere;
        p.size_x = l.radius;
        break;
    case LightShape::point:
    case LightShape::spot:
        if (l.radius > 0.0f) {
            p.shape = LightShape3D::Sphere;
            p.size_x = l.radius;
        }
        break;
    case LightShape::tube:
        p.shape = LightShape3D::Tube;
        p.axis = to_fizmo(l.direction);
        p.size_x = l.radius;
        p.size_y = l.length * 0.5f;
        break;
    case LightShape::rectangle: {
        p.shape = LightShape3D::Rectangle;
        const lighting::Vec3 n = l.direction.normalized();
        lighting::Vec3 up = l.up.normalized();
        if (std::fabs(up.dot(n)) > 0.999) up = std::fabs(n.y) < 0.9 ? lighting::Vec3{ 0, 1, 0 } : lighting::Vec3{ 1, 0, 0 };
        p.axis = to_fizmo(up.cross(n).normalized());
        p.size_x = l.width * 0.5f;
        p.size_y = l.height * 0.5f;
        break;
    }
    case LightShape::disk:
        p.shape = LightShape3D::Disk;
        p.size_x = l.radius;
        break;
    case LightShape::directional: break;
    }
    return p;
}

SceneLighting3D to_scene_lighting(const lighting::Scene& scene, const lighting::LightingSettings& settings) {
    return to_scene_lighting(scene.lights(), scene.environment(), settings);
}

SceneLighting3D to_scene_lighting(const std::vector<lighting::Light>& lights, const lighting::Environment& env, const lighting::LightingSettings& s) {
    using lighting::LightingQuality;
    SceneLighting3D out;
    if (s.quality == LightingQuality::off) return out;
    out.enabled = true;
    const float exposure = s.exposure;
    const lighting::Light* sun = nullptr;
    float best = -1.0f;
    for (const auto& l : lights) {
        if (!l.enabled || l.shape != lighting::LightShape::directional) continue;
        const float v = l.emitted_scale() * l.tint().luminance();
        if (v > best) {
            best = v;
            sun = &l;
        }
    }
    if (sun) {
        float peak = 1.0f;
        out.sun_direction = to_fizmo(sun->direction.normalized());
        out.sun_color = to_color(sun->tint(), peak);
        out.sun_intensity = sun->emitted_scale() * peak * exposure;
        out.atmosphere.sun_position = to_fizmo(-sun->direction.normalized());
    }
    for (const auto& l : lights) {
        if (!l.enabled || l.shape == lighting::LightShape::directional || l.intensity <= 0.0f) continue;
        if (out.point_lights.size() >= SceneLighting3D::MAX_POINT_LIGHTS * 4) break;
        out.point_lights.push_back(to_point_light(l, exposure));
    }
    float sky_peak = 1.0f;
    lighting::Rgb sky_avg;
    switch (env.kind) {
    case lighting::SkyKind::none: break;
    case lighting::SkyKind::color: sky_avg = env.color; break;
    case lighting::SkyKind::gradient: sky_avg = (env.zenith + env.horizon * 2.0f + env.ground) * 0.25f; break;
    case lighting::SkyKind::physical: sky_avg = lighting::Rgb(0.45f, 0.55f, 0.75f); break;
    case lighting::SkyKind::image: {
        lighting::Rgb sum;
        for (const auto& t : env.image) sum += t;
        if (!env.image.empty()) sky_avg = sum / static_cast<float>(env.image.size());
        break;
    }
    }
    sky_avg *= env.intensity * exposure;
    out.sky_color = to_color(sky_avg, sky_peak);
    out.sky_intensity = env.kind == lighting::SkyKind::none ? 0.0f : sky_peak * s.ambient;
    out.ambient = env.kind == lighting::SkyKind::none ? 0.02f : 0.0f;
    out.atmosphere.enabled = env.visible && env.kind != lighting::SkyKind::none;
    if (env.kind == lighting::SkyKind::gradient) {
        float z = 1.0f, h = 1.0f;
        out.atmosphere.zenith = to_color(env.zenith, z);
        out.atmosphere.horizon = to_color(env.horizon, h);
    }
    out.atmosphere.fog_density = s.fog.enabled ? s.fog.density : 0.0f;
    switch (s.quality) {
    case LightingQuality::off: break;
    case LightingQuality::simple:
        out.sun_shadow = SunShadow3D::low();
        out.point_shadows = PointShadows3D::off();
        break;
    case LightingQuality::shadowed:
        out.sun_shadow = SunShadow3D::high();
        out.point_shadows = PointShadows3D::medium();
        break;
    case LightingQuality::realistic:
    case LightingQuality::path_traced:
    case LightingQuality::photon:
        out.sun_shadow = SunShadow3D::ultra();
        out.point_shadows = PointShadows3D::high();
        out.surfaces.refraction = s.refraction;
        out.surfaces.screen_reflections = s.reflections == lighting::Reflections::ray_traced;
        out.volumetrics.enabled = s.fog.enabled;
        out.volumetrics.density = s.fog.density;
        out.volumetrics.anisotropy = s.fog.anisotropy;
        out.shafts.enabled = s.fog.enabled;
        break;
    }
    if (s.shadows == lighting::ShadowMethod::none) {
        out.sun_shadow = SunShadow3D::off();
        out.point_shadows = PointShadows3D::off();
    } else if (s.shadows == lighting::ShadowMethod::hard) {
        out.sun_shadow.soft = false;
    }
    if (s.shadow_casters == lighting::ShadowCasters::sun_only) out.point_shadows = PointShadows3D::off();
    out.tone_map.enabled = s.tone_mapper != lighting::ToneMapper::none;
    out.tone_map.exposure = 1.0f;
    out.tone_map.saturation = s.saturation;
    return out;
}

} // namespace graphics
} // namespace fizmo
