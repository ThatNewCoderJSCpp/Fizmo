#ifndef FIZMO_LIGHTING_SETTINGS_HPP
#define FIZMO_LIGHTING_SETTINGS_HPP

#include "color.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fizmo {
namespace lighting {

enum class LightingQuality : std::uint8_t {
    off,
    simple,
    shadowed,
    realistic,
    path_traced,
    photon
};

enum class ShadowMethod : std::uint8_t {
    none,
    hard,
    soft
};

enum class ShadowCasters : std::uint8_t {
    sun_only,
    flagged,
    everything
};

enum class GlobalIllumination : std::uint8_t {
    none,
    ambient,
    baked,
    probes,
    path_traced
};

enum class Reflections : std::uint8_t {
    none,
    environment,
    ray_traced
};

enum class Transport : std::uint8_t {
    rgb,
    spectral
};

enum class ToneMapper : std::uint8_t {
    none,
    reinhard,
    aces,
    filmic,
    agx
};

const char* to_string(LightingQuality v) noexcept;
const char* to_string(ShadowMethod v) noexcept;
const char* to_string(ShadowCasters v) noexcept;
const char* to_string(GlobalIllumination v) noexcept;
const char* to_string(Reflections v) noexcept;
const char* to_string(Transport v) noexcept;
const char* to_string(ToneMapper v) noexcept;
std::optional<LightingQuality> parse_lighting_quality(const std::string& text) noexcept;
std::optional<ShadowMethod> parse_shadow_method(const std::string& text) noexcept;
std::optional<ShadowCasters> parse_shadow_casters(const std::string& text) noexcept;
std::optional<GlobalIllumination> parse_global_illumination(const std::string& text) noexcept;
std::optional<Reflections> parse_reflections(const std::string& text) noexcept;
std::optional<Transport> parse_transport(const std::string& text) noexcept;
std::optional<ToneMapper> parse_tone_mapper(const std::string& text) noexcept;
const std::vector<LightingQuality>& lighting_qualities();
const char* quality_label(LightingQuality v) noexcept;
const char* quality_explanation(LightingQuality v) noexcept;

struct Fog {
    bool enabled = false;
    float density = 0.02f;
    Rgb color{ 1.0f, 1.0f, 1.0f };
    float anisotropy = 0.6f;
    float height = 0.0f;
    float height_falloff = 0.0f;
    float max_distance = 120.0f;
    int steps = 24;
};

struct LightingSettings {
    LightingQuality quality = LightingQuality::shadowed;

    ShadowMethod shadows = ShadowMethod::soft;
    ShadowCasters shadow_casters = ShadowCasters::flagged;
    int shadow_samples = 4;
    float shadow_bias = 0.001f;

    GlobalIllumination gi = GlobalIllumination::ambient;
    int ambient_occlusion_samples = 4;
    float ambient_occlusion_distance = 2.0f;
    float ambient = 1.0f;
    float gi_intensity = 1.0f;
    float probe_spacing = 1.0f;
    int probe_rays = 96;
    float probe_hysteresis = 0.85f;
    int probe_bounces = 3;

    Reflections reflections = Reflections::environment;
    int reflection_bounces = 1;
    bool refraction = true;
    bool dispersion = false;
    int transmission_depth = 8;

    Transport transport = Transport::rgb;
    int max_bounces = 6;
    int samples_per_pixel = 1;
    int russian_roulette_depth = 3;
    float firefly_clamp = 0.0f;
    int all_lights_threshold = 8;
    int light_samples = 2;

    bool caustics = false;
    std::size_t photons = 200000;
    int photon_bundle = 1;
    float photon_radius = 0.05f;
    int photon_bounces = 12;

    bool denoise = false;
    int denoise_passes = 3;
    bool area_lights = true;
    bool emissive_lights = true;
    bool use_ies = true;
    bool use_cookies = true;
    bool baked_static = true;

    Fog fog;

    float exposure = 1.0f;
    ToneMapper tone_mapper = ToneMapper::aces;
    float white_balance = 0.0f;
    float saturation = 1.0f;
    float contrast = 1.0f;
    float gamma = 2.2f;
    bool srgb_output = true;

    unsigned threads = 0;
    std::uint64_t seed = 1;

    static LightingSettings off();
    static LightingSettings simple();
    static LightingSettings shadowed();
    static LightingSettings realistic();
    static LightingSettings path_traced();
    static LightingSettings photon();
    static LightingSettings preset(LightingQuality quality);

    LightingSettings& with_quality(LightingQuality q);
};

} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_SETTINGS_HPP
