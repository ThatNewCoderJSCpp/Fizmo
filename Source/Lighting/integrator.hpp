#ifndef FIZMO_LIGHTING_INTEGRATOR_HPP
#define FIZMO_LIGHTING_INTEGRATOR_HPP

#include "bake.hpp"
#include "bsdf.hpp"
#include "probes.hpp"
#include "scene_internal.hpp"
#include "settings.hpp"

#include <functional>
#include <vector>

namespace fizmo {
namespace lighting {
namespace detail {

class PhotonMap;

struct Surface {
    Vec3f p;
    Vec3f ng;
    Vec3f ns;
    Vec3f wo;
    bool front = true;
    const Material* m = nullptr;
    std::uint32_t tri = 0;
    std::uint32_t instance = 0;
    std::uint32_t local = 0;
    float u = 0.0f;
    float v = 0.0f;
    Vec2f uv;
    bool lightmap = false;
    bool is_static = false;
    std::uint32_t channels = 1u;
    bool mapped = false;
    Vec2f texcoord;
    Rgb base_color;
    float metallic = 0.0f;
    float roughness = 0.5f;
    float opacity = 1.0f;
    float transmission = 0.0f;
    Rgb emission;

    Rgb albedo() const noexcept { return mapped ? base_color : m->base_color; }
    Rgb emitted() const noexcept { return mapped ? emission : m->emitted(); }
    bool emits() const noexcept { return !emitted().is_black(); }
    float alpha() const noexcept { return mapped ? opacity : m->opacity; }
};

struct Wave {
    bool spectral = false;
    float nm = 550.0f;
    int channel = -1;

    Rgb lift(const Rgb& c) const noexcept { return spectral ? Rgb(spectral::uplift(c, nm)) : c; }
    float ior(const Material& m, bool dispersion) const noexcept;
};

enum class LightFilter : std::uint8_t {
    all,
    runtime,
    baked_only,
    baked_and_stationary
};

struct DirectOptions {
    bool area = true;
    int samples = 0;
    ShadowMethod shadows = ShadowMethod::soft;
    ShadowCasters casters = ShadowCasters::flagged;
    bool colored_shadows = true;
    bool mis = false;
    LightFilter filter = LightFilter::all;
    bool emissive = true;
    bool use_ies = true;
    bool use_cookie = true;
    bool indirect_scale = false;
    bool skip_static_baked = false;
    bool include_environment = false;
};

struct Context {
    const Compiled* c = nullptr;
    const Scene* scene = nullptr;
    const LightingSettings* s = nullptr;
    const BakedLighting* baked = nullptr;
    const ProbeVolume* probes = nullptr;
    const PhotonMap* photons = nullptr;
    float photon_radius = 0.05f;
    std::vector<int> tri_light;
    bool cutouts = false;

    const Material& material(std::uint32_t i) const;
};

void build_tri_light_map(Context& ctx);
Surface make_surface(const Context& ctx, const Ray& ray, const Hit& hit);
void apply_material_maps(const Context& ctx, const Tri& tri, const Hit& hit, Surface& s);
float hit_opacity(const Context& ctx, const Tri& tri, const Hit& hit, const Material& m);
Bsdf surface_bsdf(const Context& ctx, const Surface& s, const Wave& w, bool dispersion, bool diffuse_only = false);
float surface_eps(const Surface& s) noexcept;
Vec3f offset_ray(const Surface& s, const Vec3f& dir) noexcept;
Rgb transmittance(const Context& ctx, const Vec3f& from, const Vec3f& dir, float dist, bool colored, const Wave& w);
bool light_allowed(const PreparedLight& l, LightFilter f) noexcept;
Rgb direct_lighting(const Context& ctx, const Surface& s, const Bsdf& b, const Frame& frame, const Vec3f& wo_local, Rng& rng, const DirectOptions& o, const Wave& w);
float light_select_pdf(const Context& ctx, std::size_t light, const Vec3f& p, const Vec3f& n);
Rgb sky(const Context& ctx, const Vec3f& dir, const Wave& w);
Rgb sky_irradiance(const Context& ctx, const Vec3f& n);
float ambient_occlusion(const Context& ctx, const Surface& s, int samples, float distance, Rng& rng);
Rgb baked_irradiance(const Context& ctx, const Surface& s);
Rgb fog_transmittance(const LightingSettings& st, const Vec3f& o, const Vec3f& d, float t);
Rgb fog_inscatter(const Context& ctx, const Vec3f& o, const Vec3f& d, float t, Rng& rng, const Wave& w);
Rgb irradiance_estimate(const Context& ctx, const Vec3f& p, const Vec3f& n, int samples, int bounces, LightFilter direct_filter, LightFilter indirect_filter, bool include_sky, Rng& rng);
void update_probes(ProbeVolume& volume, const Context& ctx, int rays, float hysteresis, std::uint64_t frame, LightFilter filter, bool include_sky, unsigned threads);
void parallel_for(std::size_t count, unsigned threads, const std::function<void(std::size_t begin, std::size_t end, unsigned worker)>& fn);
unsigned thread_count(unsigned requested);

} // namespace detail
} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_INTEGRATOR_HPP
