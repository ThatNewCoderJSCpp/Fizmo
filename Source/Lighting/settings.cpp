#include "settings.hpp"

#include <cctype>

namespace fizmo {
namespace lighting {

namespace {

std::string key(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '_' || c == ' ') c = '-';
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

} // namespace

const char* to_string(LightingQuality v) noexcept {
    switch (v) {
    case LightingQuality::off: return "off";
    case LightingQuality::simple: return "simple";
    case LightingQuality::shadowed: return "shadowed";
    case LightingQuality::realistic: return "realistic";
    case LightingQuality::path_traced: return "path-traced";
    case LightingQuality::photon: return "photon";
    }
    return "shadowed";
}

const char* to_string(ShadowMethod v) noexcept {
    switch (v) {
    case ShadowMethod::none: return "none";
    case ShadowMethod::hard: return "hard";
    case ShadowMethod::soft: return "soft";
    }
    return "soft";
}

const char* to_string(ShadowCasters v) noexcept {
    switch (v) {
    case ShadowCasters::sun_only: return "sun-only";
    case ShadowCasters::flagged: return "flagged";
    case ShadowCasters::everything: return "everything";
    }
    return "everything";
}

const char* to_string(GlobalIllumination v) noexcept {
    switch (v) {
    case GlobalIllumination::none: return "none";
    case GlobalIllumination::ambient: return "ambient";
    case GlobalIllumination::baked: return "baked";
    case GlobalIllumination::probes: return "probes";
    case GlobalIllumination::path_traced: return "path-traced";
    }
    return "ambient";
}

const char* to_string(Reflections v) noexcept {
    switch (v) {
    case Reflections::none: return "none";
    case Reflections::environment: return "environment";
    case Reflections::ray_traced: return "ray-traced";
    }
    return "environment";
}

const char* to_string(Transport v) noexcept {
    return v == Transport::spectral ? "spectral" : "rgb";
}

const char* to_string(ToneMapper v) noexcept {
    switch (v) {
    case ToneMapper::none: return "none";
    case ToneMapper::reinhard: return "reinhard";
    case ToneMapper::aces: return "aces";
    case ToneMapper::filmic: return "filmic";
    case ToneMapper::agx: return "agx";
    }
    return "aces";
}

std::optional<LightingQuality> parse_lighting_quality(const std::string& text) noexcept {
    const std::string s = key(text);
    if (s == "off" || s == "unlit" || s == "none" || s == "flat") return LightingQuality::off;
    if (s == "simple" || s == "basic" || s == "raycast" || s == "low" || s == "voxel") return LightingQuality::simple;
    if (s == "shadowed" || s == "shadows" || s == "medium" || s == "standard") return LightingQuality::shadowed;
    if (s == "realistic" || s == "high" || s == "lumen" || s == "gi") return LightingQuality::realistic;
    if (s == "path-traced" || s == "pathtraced" || s == "path" || s == "ultra" || s == "ray-traced") return LightingQuality::path_traced;
    if (s == "photon" || s == "photons" || s == "spectral" || s == "physical" || s == "max" || s == "photon-mapped") return LightingQuality::photon;
    return std::nullopt;
}

std::optional<ShadowMethod> parse_shadow_method(const std::string& text) noexcept {
    const std::string s = key(text);
    if (s == "none" || s == "off") return ShadowMethod::none;
    if (s == "hard" || s == "raycast" || s == "sharp") return ShadowMethod::hard;
    if (s == "soft" || s == "area" || s == "penumbra") return ShadowMethod::soft;
    return std::nullopt;
}

std::optional<ShadowCasters> parse_shadow_casters(const std::string& text) noexcept {
    const std::string s = key(text);
    if (s == "sun-only" || s == "sun") return ShadowCasters::sun_only;
    if (s == "flagged" || s == "chosen" || s == "per-light") return ShadowCasters::flagged;
    if (s == "everything" || s == "all") return ShadowCasters::everything;
    return std::nullopt;
}

std::optional<GlobalIllumination> parse_global_illumination(const std::string& text) noexcept {
    const std::string s = key(text);
    if (s == "none" || s == "off") return GlobalIllumination::none;
    if (s == "ambient" || s == "sky") return GlobalIllumination::ambient;
    if (s == "baked" || s == "lightmaps" || s == "static") return GlobalIllumination::baked;
    if (s == "probes" || s == "dynamic" || s == "lumen" || s == "ddgi") return GlobalIllumination::probes;
    if (s == "path-traced" || s == "path" || s == "full") return GlobalIllumination::path_traced;
    return std::nullopt;
}

std::optional<Reflections> parse_reflections(const std::string& text) noexcept {
    const std::string s = key(text);
    if (s == "none" || s == "off") return Reflections::none;
    if (s == "environment" || s == "sky" || s == "probe") return Reflections::environment;
    if (s == "ray-traced" || s == "raytraced" || s == "traced") return Reflections::ray_traced;
    return std::nullopt;
}

std::optional<Transport> parse_transport(const std::string& text) noexcept {
    const std::string s = key(text);
    if (s == "rgb" || s == "color") return Transport::rgb;
    if (s == "spectral" || s == "wavelength") return Transport::spectral;
    return std::nullopt;
}

std::optional<ToneMapper> parse_tone_mapper(const std::string& text) noexcept {
    const std::string s = key(text);
    if (s == "none" || s == "linear" || s == "off") return ToneMapper::none;
    if (s == "reinhard") return ToneMapper::reinhard;
    if (s == "aces") return ToneMapper::aces;
    if (s == "filmic" || s == "hable" || s == "uncharted") return ToneMapper::filmic;
    if (s == "agx") return ToneMapper::agx;
    return std::nullopt;
}

const std::vector<LightingQuality>& lighting_qualities() {
    static const std::vector<LightingQuality> all{ LightingQuality::off, LightingQuality::simple, LightingQuality::shadowed, LightingQuality::realistic, LightingQuality::path_traced, LightingQuality::photon };
    return all;
}

const char* quality_label(LightingQuality v) noexcept {
    switch (v) {
    case LightingQuality::off: return "Off (unlit)";
    case LightingQuality::simple: return "Simple (ray-cast sun shadows)";
    case LightingQuality::shadowed: return "Shadowed (every object casts soft shadows)";
    case LightingQuality::realistic: return "Realistic (bounced light, reflections)";
    case LightingQuality::path_traced: return "Path traced";
    case LightingQuality::photon: return "Photon (spectral, caustics, dispersion)";
    }
    return "Shadowed";
}

const char* quality_explanation(LightingQuality v) noexcept {
    switch (v) {
    case LightingQuality::off: return "Colors are shown as they are, with emission, and no lights at all.";
    case LightingQuality::simple: return "Each light shines straight from its center. Only the sun casts shadows, with one ray per pixel, like Voxelspire. Ambient light is flat.";
    case LightingQuality::shadowed: return "Every light casts soft shadows from every object, sized by how big the light is. Sky light is darkened in corners with ambient occlusion.";
    case LightingQuality::realistic: return "Adds light that bounces off walls and colors the room, glossy reflections, glass that bends light, and glowing surfaces that light things. Updates as lights move.";
    case LightingQuality::path_traced: return "Follows light paths through the whole scene for exact soft shadows, bounces, reflections and refraction. Noisy at first, then clears up the longer it renders.";
    case LightingQuality::photon: return "Traces light by wavelength from the lights themselves, so glass and water focus light into caustics and crystals split it into rainbows. The slowest and most exact.";
    }
    return "";
}

LightingSettings LightingSettings::off() {
    LightingSettings s;
    s.quality = LightingQuality::off;
    s.shadows = ShadowMethod::none;
    s.gi = GlobalIllumination::none;
    s.reflections = Reflections::none;
    s.refraction = false;
    s.tone_mapper = ToneMapper::none;
    return s;
}

LightingSettings LightingSettings::simple() {
    LightingSettings s;
    s.quality = LightingQuality::simple;
    s.shadows = ShadowMethod::hard;
    s.shadow_casters = ShadowCasters::sun_only;
    s.shadow_samples = 1;
    s.gi = GlobalIllumination::ambient;
    s.ambient_occlusion_samples = 0;
    s.reflections = Reflections::environment;
    s.refraction = false;
    s.area_lights = false;
    s.emissive_lights = false;
    s.use_ies = false;
    return s;
}

LightingSettings LightingSettings::shadowed() {
    LightingSettings s;
    s.quality = LightingQuality::shadowed;
    s.shadows = ShadowMethod::soft;
    s.shadow_casters = ShadowCasters::flagged;
    s.shadow_samples = 4;
    s.gi = GlobalIllumination::ambient;
    s.ambient_occlusion_samples = 4;
    s.reflections = Reflections::environment;
    s.refraction = true;
    s.transmission_depth = 4;
    s.denoise = true;
    s.denoise_passes = 2;
    return s;
}

LightingSettings LightingSettings::realistic() {
    LightingSettings s = shadowed();
    s.quality = LightingQuality::realistic;
    s.shadow_samples = 2;
    s.gi = GlobalIllumination::probes;
    s.ambient_occlusion_samples = 2;
    s.reflections = Reflections::ray_traced;
    s.reflection_bounces = 2;
    s.transmission_depth = 8;
    s.denoise = true;
    return s;
}

LightingSettings LightingSettings::path_traced() {
    LightingSettings s = realistic();
    s.quality = LightingQuality::path_traced;
    s.shadows = ShadowMethod::soft;
    s.shadow_samples = 1;
    s.gi = GlobalIllumination::path_traced;
    s.reflections = Reflections::ray_traced;
    s.max_bounces = 8;
    s.samples_per_pixel = 4;
    s.dispersion = true;
    s.denoise = false;
    s.firefly_clamp = 10.0f;
    return s;
}

LightingSettings LightingSettings::photon() {
    LightingSettings s = path_traced();
    s.quality = LightingQuality::photon;
    s.transport = Transport::spectral;
    s.caustics = true;
    s.dispersion = true;
    s.max_bounces = 12;
    s.samples_per_pixel = 8;
    s.photons = 500000;
    s.photon_radius = 0.04f;
    return s;
}

LightingSettings LightingSettings::preset(LightingQuality q) {
    switch (q) {
    case LightingQuality::off: return off();
    case LightingQuality::simple: return simple();
    case LightingQuality::shadowed: return shadowed();
    case LightingQuality::realistic: return realistic();
    case LightingQuality::path_traced: return path_traced();
    case LightingQuality::photon: return photon();
    }
    return shadowed();
}

LightingSettings& LightingSettings::with_quality(LightingQuality q) {
    const LightingSettings p = preset(q);
    const Fog keep_fog = fog;
    const float keep_exposure = exposure;
    const ToneMapper keep_tone = tone_mapper;
    const unsigned keep_threads = threads;
    const std::uint64_t keep_seed = seed;
    *this = p;
    fog = keep_fog;
    exposure = keep_exposure;
    tone_mapper = keep_tone;
    threads = keep_threads;
    seed = keep_seed;
    return *this;
}

} // namespace lighting
} // namespace fizmo
