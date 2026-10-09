#include "fizmo_library.hpp"
#include "lighting_3d.hpp"

namespace fizmo {
namespace graphics {

auto PointLight3D::spot(const vector3d& at, const vector3d& facing, const Color& c, float strength, float reach, float half_angle, bool shadows) noexcept -> PointLight3D {
    PointLight3D l(at, c, strength, reach, shadows);
    l.direction = facing;
    l.cone      = half_angle;
    return l;
}

auto SunShadow3D::low() noexcept -> SunShadow3D {
    SunShadow3D s;
    s.enabled = true; s.resolution = 1024; s.distance = 40.0; s.filter_taps = 2; s.crossfade = false;
    return s;
}

auto SunShadow3D::high() noexcept -> SunShadow3D {
    SunShadow3D s;
    s.enabled = true; s.resolution = 4096; s.distance = 96.0; s.soft = true; s.light_size = 1.5f; s.filter_taps = 5;
    return s;
}

auto SunShadow3D::ultra() noexcept -> SunShadow3D {
    SunShadow3D s;
    s.enabled = true; s.resolution = 8192; s.distance = 128.0; s.depth_range = 256.0; s.soft = true; s.light_size = 2.0f; s.filter_taps = 6; s.max_softness = 16.0f;
    return s;
}

auto PointShadows3D::low() noexcept -> PointShadows3D {
    PointShadows3D p;
    p.enabled = true; p.max_lights = 2; p.resolution = 256; p.moving_faces = 6;
    return p;
}

auto PointShadows3D::high() noexcept -> PointShadows3D {
    PointShadows3D p;
    p.enabled = true; p.max_lights = MAX_LIGHTS; p.resolution = 1024; p.moving_faces = 24;
    return p;
}

auto CapsuleOccluder3D::standing(const vector3d& feet, double height, double radius) noexcept -> CapsuleOccluder3D {
    const double top = height > 2.0 * radius ? height - radius : radius;
    return { feet + vector3d{ 0.0, 0.0, radius }, feet + vector3d{ 0.0, 0.0, top }, static_cast<float>(radius) };
}

auto SceneLighting3D::daylight(const SunShadow3D& shadows) -> SceneLighting3D {
    SceneLighting3D l;
    l.enabled = true;
    l.sun_direction = { 0.35, 0.2, -1.0 };
    l.sun_color = Color(255, 244, 222);
    l.sun_intensity = 0.75f;
    l.sky_color = Color(200, 215, 240);
    l.sky_intensity = 0.55f;
    l.ambient = 0.05f;
    l.sun_shadow = shadows;
    l.atmosphere.enabled = true;
    l.atmosphere.sun_position = { -0.35, -0.2, 1.0 };
    l.tone_map.enabled = true;
    return l;
}

auto SceneLighting3D::golden_hour(const SunShadow3D& shadows) -> SceneLighting3D {
    SceneLighting3D l = daylight(shadows);
    l.sun_direction = { 0.9, 0.25, -0.22 };
    l.sun_color = Color(255, 190, 120);
    l.sun_intensity = 0.85f;
    l.sky_color = Color(255, 200, 170);
    l.sky_intensity = 0.4f;
    l.atmosphere.sun_position = { -0.9, -0.25, 0.22 };
    l.atmosphere.zenith = Color(80, 110, 190);
    l.atmosphere.horizon = Color(250, 170, 120);
    l.atmosphere.glow = Color(255, 160, 90);
    l.atmosphere.glow_strength = 1.0f;
    return l;
}

auto SceneLighting3D::overcast() -> SceneLighting3D {
    SceneLighting3D l = daylight(SunShadow3D::off());
    l.sun_intensity = 0.0f;
    l.sky_color = Color(225, 228, 232);
    l.sky_intensity = 0.85f;
    l.ambient = 0.1f;
    l.atmosphere.zenith = Color(150, 158, 170);
    l.atmosphere.horizon = Color(200, 204, 210);
    l.atmosphere.glow_strength = 0.0f;
    l.atmosphere.fog_density = 0.01f;
    return l;
}

auto SceneLighting3D::night() -> SceneLighting3D {
    SceneLighting3D l = daylight(SunShadow3D::off());
    l.sun_direction = { -0.3, 0.4, -0.8 };
    l.sun_color = Color(150, 170, 230);
    l.sun_intensity = 0.12f;
    l.sky_color = Color(60, 70, 110);
    l.sky_intensity = 0.25f;
    l.min_light = 0.03f;
    l.atmosphere.sun_position = { 0.3, -0.4, -0.8 };
    l.atmosphere.moon_position = { 0.3, -0.4, 0.8 };
    l.atmosphere.zenith = Color(8, 12, 30);
    l.atmosphere.horizon = Color(25, 32, 60);
    l.atmosphere.glow_strength = 0.0f;
    l.atmosphere.stars = 1.0f;
    l.point_shadows = PointShadows3D::medium();
    return l;
}

auto SceneLighting3D::indoor() -> SceneLighting3D {
    SceneLighting3D l;
    l.enabled = true;
    l.sun_intensity = 0.0f;
    l.sky_color = Color(255, 240, 220);
    l.sky_intensity = 0.25f;
    l.ambient = 0.08f;
    l.min_light = 0.05f;
    l.point_shadows = PointShadows3D::medium();
    l.tone_map.enabled = true;
    return l;
}

auto SceneLighting3D::studio() -> SceneLighting3D {
    SceneLighting3D l;
    l.enabled = true;
    l.sun_direction = { -0.4, 0.5, -0.75 };
    l.sun_color = Color(255, 255, 255);
    l.sun_intensity = 0.7f;
    l.sky_color = Color(235, 240, 250);
    l.sky_intensity = 0.45f;
    l.ambient = 0.06f;
    l.sun_shadow = SunShadow3D::high();
    l.tone_map.enabled = true;
    return l;
}

} // namespace graphics
} // namespace fizmo

namespace fizmo {
namespace graphics {
namespace detail {

float light_channel(const Color& c, int channel) noexcept {
    const std::uint8_t v = channel == 0 ? c.red() : (channel == 1 ? c.green() : c.blue());
    return static_cast<float>(v) / 255.0f;
}

} // namespace detail
} // namespace graphics
} // namespace fizmo
