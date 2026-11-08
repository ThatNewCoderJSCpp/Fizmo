#include "renderer.hpp"
#include "integrator.hpp"
#include "photons.hpp"

#include <chrono>

namespace fizmo {
namespace lighting {

using namespace detail;

namespace {

struct GSample {
    Rgb albedo;
    Vec3f normal;
    float depth = 0.0f;
};

Rgb absorb(const Material& m, float t, const Wave& w) noexcept {
    if (m.absorption_distance <= 0.0f || !std::isfinite(t)) return Rgb(1.0f);
    const Rgb c = w.lift(m.absorption_color);
    Rgb out;
    for (int i = 0; i < 3; ++i) out[i] = std::exp(std::log(std::clamp(c[i], 1e-4f, 1.0f)) / m.absorption_distance * t);
    return out;
}

float hg(float cos_t, float g) noexcept {
    const float d = std::max(1.0f + g * g - 2.0f * g * cos_t, 1e-4f);
    return (1.0f - g * g) / (4.0f * kPi * d * std::sqrt(d));
}

Vec3f sample_hg(const Vec3f& forward, float g, float u1, float u2) noexcept {
    float cos_t;
    if (std::fabs(g) < 1e-3f) cos_t = 1.0f - 2.0f * u1;
    else {
        const float sq = (1.0f - g * g) / (1.0f - g + 2.0f * g * u1);
        cos_t = (1.0f + g * g - sq * sq) / (2.0f * g);
    }
    cos_t = std::clamp(cos_t, -1.0f, 1.0f);
    const float sin_t = std::sqrt(std::max(0.0f, 1.0f - cos_t * cos_t));
    const float phi = kTwoPi * u2;
    return normalize(Frame(forward).to_world(Vec3f{ sin_t * std::cos(phi), sin_t * std::sin(phi), cos_t }));
}

bool same(const LightingSettings& a, const LightingSettings& b) {
    return a.quality == b.quality && a.shadows == b.shadows && a.shadow_casters == b.shadow_casters && a.shadow_samples == b.shadow_samples && a.shadow_bias == b.shadow_bias
        && a.gi == b.gi && a.ambient_occlusion_samples == b.ambient_occlusion_samples && a.ambient_occlusion_distance == b.ambient_occlusion_distance && a.ambient == b.ambient
        && a.gi_intensity == b.gi_intensity && a.probe_spacing == b.probe_spacing && a.probe_rays == b.probe_rays && a.probe_hysteresis == b.probe_hysteresis && a.probe_bounces == b.probe_bounces
        && a.reflections == b.reflections && a.reflection_bounces == b.reflection_bounces && a.refraction == b.refraction && a.dispersion == b.dispersion && a.transmission_depth == b.transmission_depth
        && a.transport == b.transport && a.max_bounces == b.max_bounces && a.samples_per_pixel == b.samples_per_pixel && a.russian_roulette_depth == b.russian_roulette_depth
        && a.firefly_clamp == b.firefly_clamp && a.all_lights_threshold == b.all_lights_threshold && a.light_samples == b.light_samples && a.caustics == b.caustics && a.photons == b.photons
        && a.photon_bundle == b.photon_bundle && a.photon_radius == b.photon_radius && a.photon_bounces == b.photon_bounces && a.area_lights == b.area_lights && a.emissive_lights == b.emissive_lights
        && a.use_ies == b.use_ies && a.use_cookies == b.use_cookies && a.baked_static == b.baked_static && a.fog.enabled == b.fog.enabled && a.fog.density == b.fog.density
        && a.fog.color == b.fog.color && a.fog.anisotropy == b.fog.anisotropy && a.fog.height == b.fog.height && a.fog.height_falloff == b.fog.height_falloff
        && a.fog.max_distance == b.fog.max_distance && a.fog.steps == b.fog.steps && a.seed == b.seed;
}

bool same(const Camera& a, const Camera& b) {
    return a.position == b.position && a.forward == b.forward && a.up == b.up && a.fov_degrees == b.fov_degrees && a.aperture == b.aperture && a.focus_distance == b.focus_distance;
}

struct Closest {
    bool tri = false;
    Hit h;
    int light = -1;
    float t = kInfinity;
    Vec3f lp;
    Vec3f ln;
};

struct Tracer {
    const Context& ctx;
    const LightingSettings& st;
    const Compiled& c;
    LightFilter runtime;

    Closest closest(const Ray& r, bool lights) const {
        Closest out;
        Hit h;
        if (intersect(c, r, h, tri_visible)) {
            out.tri = true;
            out.h = h;
            out.t = h.t;
        }
        if (lights && st.area_lights) {
            for (std::size_t i = 0; i < c.lights.size(); ++i) {
                const PreparedLight& l = c.lights[i];
                if (!l.visible || l.kind == LightKind::point || l.kind == LightKind::directional || l.kind == LightKind::triangle) continue;
                float t;
                Vec3f p, n;
                Ray rr = r;
                rr.tmax = out.t;
                if (intersect_light(l, rr, t, p, n) && t < out.t) {
                    out.t = t;
                    out.tri = false;
                    out.light = static_cast<int>(i);
                    out.lp = p;
                    out.ln = n;
                }
            }
        }
        return out;
    }

    DirectOptions direct_options() const {
        DirectOptions o;
        o.area = st.area_lights && st.quality != LightingQuality::simple;
        o.shadows = st.shadows;
        o.casters = st.shadow_casters;
        o.colored_shadows = st.quality != LightingQuality::simple;
        o.filter = runtime;
        o.emissive = st.emissive_lights;
        o.mis = false;
        return o;
    }

    Rgb with_fog(const Rgb& L, const Vec3f& o, const Vec3f& d, float t, Rng& rng, const Wave& w) const {
        if (!st.fog.enabled || st.fog.density <= 0.0f) return L;
        const float tt = std::isfinite(t) ? t : st.fog.max_distance;
        return L * fog_transmittance(st, o, d, tt) + fog_inscatter(ctx, o, d, tt, rng, w);
    }

    Rgb glossy_environment(const Surface& s, const Vec3f& R, float rough, const Wave& w) const {
        if (st.gi == GlobalIllumination::probes && ctx.probes && !ctx.probes->empty()) {
            const Rgb blurry = ctx.probes->irradiance_f(s.p, R) * kInvPi * st.gi_intensity;
            return blurry;
        }
        if (st.gi == GlobalIllumination::baked && ctx.baked && !ctx.baked->volume.empty()) return ctx.baked->volume.irradiance_f(s.p, R) * kInvPi;
        const Rgb sharp = sky(ctx, R, w);
        const Rgb blur = w.lift(sky_irradiance(ctx, R)) * kInvPi;
        const float k = clamp01(rough * rough * 1.5f);
        return sharp * (1.0f - k) + blur * k;
    }

    Rgb unlit(const Ray& r, GSample* g) const {
        Hit h;
        if (!intersect(c, r, h, tri_visible)) return c.env.visible ? sky_radiance(c, r.d) : Rgb();
        const Surface s = make_surface(ctx, r, h);
        if (g) {
            g->albedo = s.m->base_color;
            g->normal = s.ns;
            g->depth = h.t;
        }
        return s.m->base_color + s.m->emitted();
    }

    Rgb direct(const Ray& r, int refl, int trans, Rng& rng, const Material* medium, bool camera, GSample* g, int guard = 0) const {
        const Wave w;
        const Closest cl = closest(r, true);
        if (!cl.tri && cl.light < 0) {
            Rgb L = (camera && !c.env.visible) ? Rgb() : sky(ctx, r.d, w);
            if (g) {
                g->albedo = Rgb(1.0f);
                g->normal = -r.d;
                g->depth = 1e6f;
            }
            return with_fog(L, r.o, r.d, kInfinity, rng, w);
        }
        if (cl.light >= 0) {
            const PreparedLight& l = c.lights[static_cast<std::size_t>(cl.light)];
            Rgb L = light_hit_radiance(l, cl.lp, cl.ln, r.o, st.use_ies, st.use_cookies);
            if (g) {
                g->albedo = Rgb(1.0f);
                g->normal = cl.ln;
                g->depth = cl.t;
            }
            if (medium) L *= absorb(*medium, cl.t, w);
            return with_fog(L, r.o, r.d, cl.t, rng, w);
        }
        const Surface s = make_surface(ctx, r, cl.h);
        const Material& m = *s.m;
        if (g) {
            g->albedo = m.base_color;
            g->normal = s.ns;
            g->depth = cl.t;
        }
        if (m.opacity < 1.0f && rng.uniform() > m.opacity && guard < 16) {
            Ray next{ s.p + r.d * surface_eps(s) * 4.0f, r.d, kInfinity };
            Rgb L = direct(next, refl, trans, rng, medium, camera, nullptr, guard + 1);
            return L;
        }
        Rgb L = m.emitted();
        const Bsdf b = surface_bsdf(ctx, s, w, false);
        const Frame frame(s.ns);
        const Vec3f wo = frame.to_local(s.wo);
        L += direct_lighting(ctx, s, b, frame, wo, rng, direct_options(), w);
        Rgb E;
        switch (st.gi) {
        case GlobalIllumination::none: break;
        case GlobalIllumination::ambient: {
            E = sky_irradiance(ctx, s.ns) * st.ambient;
            if (st.ambient_occlusion_samples > 0 && st.quality != LightingQuality::simple) E *= ambient_occlusion(ctx, s, st.ambient_occlusion_samples, st.ambient_occlusion_distance, rng);
            break;
        }
        case GlobalIllumination::baked: {
            if (ctx.baked && !ctx.baked->empty()) E = baked_irradiance(ctx, s) * st.gi_intensity;
            else E = sky_irradiance(ctx, s.ns) * st.ambient;
            break;
        }
        case GlobalIllumination::probes:
        case GlobalIllumination::path_traced: {
            if (ctx.probes && !ctx.probes->empty()) E = ctx.probes->irradiance_f(s.p, s.ns) * st.gi_intensity;
            if (st.ambient_occlusion_samples > 0) {
                const float ao = ambient_occlusion(ctx, s, st.ambient_occlusion_samples, std::min(st.ambient_occlusion_distance, st.probe_spacing), rng);
                E *= 0.5f + 0.5f * ao;
            }
            break;
        }
        }
        L += b.diffuse * E * kInvPi;
        const float nv = std::max(wo.z, 1e-4f);
        const float rough = b.roughness();
        const bool wants_reflection = st.reflections != Reflections::none && (b.p_spec > 0.0f || b.p_trans > 0.0f || b.f0.max_component() > 0.0f);
        float F_glass = 0.0f;
        if (b.p_trans > 0.0f) {
            const float f = fresnel_dielectric(nv, b.eta);
            F_glass = b.thin ? (f < 1.0f ? 2.0f * f / (1.0f + f) : 1.0f) : f;
        }
        if (wants_reflection) {
            const Rgb spec_w = env_brdf_approx(b.f0, rough, nv) * (1.0f - b.transmission) + Rgb(F_glass * b.transmission);
            if (spec_w.max_component() > 1e-4f) {
                Vec3f wi_l{ -wo.x, -wo.y, wo.z };
                if (!b.delta_spec) {
                    const Vec3f mh = ggx_sample_vndf(wo, b.alpha, rng.uniform(), rng.uniform());
                    wi_l = mh * (2.0f * dot(wo, mh)) - wo;
                    if (wi_l.z <= 0.0f) wi_l = Vec3f{ -wo.x, -wo.y, wo.z };
                }
                const Vec3f R = normalize(frame.to_world(wi_l));
                Rgb refl_c;
                if (st.reflections == Reflections::ray_traced && refl > 0 && rough < 0.65f) {
                    Ray rr{ offset_ray(s, R), R, kInfinity };
                    refl_c = direct(rr, refl - 1, trans, rng, b.p_trans > 0.0f && !s.front ? &m : medium, false, nullptr);
                } else {
                    refl_c = glossy_environment(s, R, rough, w);
                }
                L += refl_c * spec_w;
            }
        }
        if (b.p_trans > 0.0f && trans > 0) {
            Vec3f wt_l = -wo;
            const Material* next_medium = medium;
            if (st.refraction && !b.thin) {
                Vec3f mh{ 0.0f, 0.0f, 1.0f };
                if (!b.delta_spec) mh = ggx_sample_vndf(wo, b.alpha, rng.uniform(), rng.uniform());
                if (!refract(wo, mh, b.eta, wt_l)) wt_l = Vec3f{ -wo.x, -wo.y, wo.z };
                next_medium = s.front ? &m : nullptr;
            }
            const Vec3f T = normalize(frame.to_world(wt_l));
            Ray tr{ offset_ray(s, T), T, kInfinity };
            const Rgb through = direct(tr, refl, trans - 1, rng, next_medium, false, nullptr);
            L += through * b.tint * (b.transmission * (1.0f - F_glass));
        }
        if (medium) L *= absorb(*medium, cl.t, w);
        return with_fog(L, r.o, r.d, cl.t, rng, w);
    }

    Rgb path(Ray r, Rng& rng, Wave& w, GSample* g) const {
        Rgb L, beta(1.0f);
        bool specular = true;
        float pdf_prev = 1.0f;
        Vec3f prev_p = r.o, prev_n{ 0.0f, 1.0f, 0.0f };
        bool had_diffuse = false;
        const Material* medium = nullptr;
        const bool caustics = st.caustics && ctx.photons && ctx.photons->size() > 0;
        const int max_depth = std::max(st.max_bounces, 1);
        DirectOptions od;
        od.area = st.area_lights;
        od.shadows = st.shadows == ShadowMethod::none ? ShadowMethod::none : ShadowMethod::soft;
        od.casters = st.shadow_casters;
        od.colored_shadows = false;
        od.mis = true;
        od.filter = runtime;
        od.emissive = st.emissive_lights;
        od.include_environment = true;
        const float clamp = st.firefly_clamp;
        auto add = [&](const Rgb& v, int depth) {
            if (clamp > 0.0f && depth > 0) {
                const float m = v.max_component();
                L += m > clamp ? v * (clamp / m) : v;
            } else {
                L += v;
            }
        };
        int guard = 0;
        for (int depth = 0; depth <= max_depth; ++depth) {
            const Closest cl = closest(r, true);
            if (st.fog.enabled && st.fog.density > 0.0f) {
                const float sigma = st.fog.height_falloff > 0.0f ? st.fog.density * std::exp(-st.fog.height_falloff * std::max(r.o.y - st.fog.height, -20.0f)) : st.fog.density;
                const float limit = std::min(cl.t, st.fog.max_distance);
                const float ts = -std::log(std::max(1.0f - rng.uniform(), 1e-12f)) / std::max(sigma, 1e-8f);
                if (ts < limit) {
                    const Vec3f x = r.o + r.d * ts;
                    beta *= w.lift(st.fog.color);
                    for (const PreparedLight& l : c.lights) {
                        if (l.volumetric <= 0.0f || l.kind == LightKind::triangle || !light_reaches(l, x)) continue;
                        LightSample ls;
                        if (!sample_light(l, x, rng.uniform(), rng.uniform(), st.area_lights, st.use_ies, st.use_cookies, ls)) continue;
                        const float dist = std::isfinite(ls.distance) ? ls.distance * 0.999f : 1e30f;
                        Ray sr{ x, ls.wi, dist };
                        if (l.shadows && occluded(c, sr)) continue;
                        add(beta * w.lift(ls.radiance) * (hg(dot(ls.wi, r.d), st.fog.anisotropy) * l.volumetric / ls.pdf), depth);
                    }
                    const Vec3f nd = sample_hg(r.d, st.fog.anisotropy, rng.uniform(), rng.uniform());
                    pdf_prev = hg(dot(nd, r.d), st.fog.anisotropy);
                    specular = false;
                    prev_p = x;
                    prev_n = nd;
                    r = Ray{ x, nd, kInfinity };
                    continue;
                }
            }
            if (!cl.tri && cl.light < 0) {
                if (depth == 0 && !c.env.visible) break;
                Rgb Le = sky(ctx, r.d, w);
                float weight = 1.0f;
                if (!specular && c.env_sampler.ready()) weight = power_heuristic(pdf_prev, environment_pdf(c, r.d));
                if (!(caustics && had_diffuse && specular)) add(beta * Le * weight, depth);
                break;
            }
            if (cl.light >= 0) {
                const PreparedLight& l = c.lights[static_cast<std::size_t>(cl.light)];
                if (light_allowed(l, runtime)) {
                    Rgb Le = w.lift(light_hit_radiance(l, cl.lp, cl.ln, r.o, st.use_ies, st.use_cookies));
                    if (medium) Le *= absorb(*medium, cl.t, w);
                    float weight = 1.0f;
                    if (!specular) {
                        const float lp = light_pdf(l, prev_p, r.d, cl.t, cl.lp, cl.ln) * light_select_pdf(ctx, static_cast<std::size_t>(cl.light), prev_p, prev_n);
                        weight = power_heuristic(pdf_prev, lp);
                    }
                    if (!(caustics && had_diffuse && specular)) add(beta * Le * weight, depth);
                }
                break;
            }
            const Surface s = make_surface(ctx, r, cl.h);
            const Material& m = *s.m;
            if (g && depth == 0) {
                g->albedo = m.base_color;
                g->normal = s.ns;
                g->depth = cl.t;
            }
            if (medium) beta *= absorb(*medium, cl.t, w);
            if (m.opacity < 1.0f && rng.uniform() > m.opacity && guard < 32) {
                ++guard;
                --depth;
                r = Ray{ s.p + r.d * surface_eps(s) * 4.0f, r.d, kInfinity };
                continue;
            }
            if (m.emits() && (m.double_sided || s.front)) {
                const Rgb Le = w.lift(m.emitted());
                float weight = 1.0f;
                if (!specular && cl.h.tri < ctx.tri_light.size() && ctx.tri_light[cl.h.tri] >= 0 && st.emissive_lights) {
                    const std::size_t li = static_cast<std::size_t>(ctx.tri_light[cl.h.tri]);
                    const PreparedLight& l = c.lights[li];
                    if (light_allowed(l, runtime)) {
                        const float lp = light_pdf(l, prev_p, r.d, cl.t, s.p, s.ng) * light_select_pdf(ctx, li, prev_p, prev_n);
                        weight = power_heuristic(pdf_prev, lp);
                    }
                }
                if (!(caustics && had_diffuse && specular)) add(beta * Le * weight, depth);
            }
            if (m.is_dispersive() && st.dispersion && !w.spectral && w.channel < 0) {
                const int k = std::min(2, static_cast<int>(rng.uniform() * 3.0f));
                Rgb mask;
                mask[k] = 3.0f;
                beta *= mask;
                w.channel = k;
            }
            const Bsdf b = surface_bsdf(ctx, s, w, st.dispersion);
            const Frame frame(s.ns);
            const Vec3f wo = frame.to_local(s.wo);
            if (caustics && !had_diffuse && b.has_smooth_lobes() && !m.is_transmissive() && !(m.metallic > 0.5f && m.roughness < 0.15f)) {
                const Bsdf rgb = surface_bsdf(ctx, s, Wave(), false);
                add(beta * w.lift(ctx.photons->gather(s, rgb, frame, wo)), depth);
            }
            if (b.has_smooth_lobes()) add(beta * direct_lighting(ctx, s, b, frame, wo, rng, od, w), depth + 1);
            if (depth == max_depth) break;
            BsdfSample bs;
            if (!bsdf_sample(b, wo, rng.uniform(), rng.uniform(), rng.uniform(), rng.uniform(), bs)) break;
            beta *= bs.weight;
            if (!beta.is_finite() || beta.max_component() <= 0.0f) break;
            specular = bs.specular;
            pdf_prev = bs.pdf;
            prev_p = s.p;
            prev_n = s.ns;
            if (!bs.specular && b.has_smooth_lobes()) had_diffuse = true;
            const Vec3f nd = normalize(frame.to_world(bs.wi));
            if (bs.transmitted && !m.thin_walled) medium = s.front ? &m : nullptr;
            r = Ray{ offset_ray(s, nd), nd, kInfinity };
            if (depth >= st.russian_roulette_depth) {
                const float q = std::clamp(1.0f - beta.max_component(), 0.05f, 0.95f);
                if (rng.uniform() < q) break;
                beta *= 1.0f / (1.0f - q);
            }
        }
        return L;
    }
};

void denoise(Image& img, const std::vector<GSample>& g, int passes) {
    const unsigned w = img.width(), h = img.height();
    if (w == 0 || h == 0 || g.size() != static_cast<std::size_t>(w) * h) return;
    std::vector<Rgb> cur(img.pixels().size()), next(cur.size());
    for (std::size_t i = 0; i < cur.size(); ++i) {
        const Rgb a = g[i].albedo;
        const Rgb& p = img.pixels()[i];
        cur[i] = Rgb(a.r > 1e-3f ? p.r / a.r : p.r, a.g > 1e-3f ? p.g / a.g : p.g, a.b > 1e-3f ? p.b / a.b : p.b);
    }
    const float kernel[5] = { 1.0f / 16.0f, 1.0f / 4.0f, 3.0f / 8.0f, 1.0f / 4.0f, 1.0f / 16.0f };
    for (int it = 0; it < passes; ++it) {
        const int step = 1 << it;
        for (unsigned y = 0; y < h; ++y) {
            for (unsigned x = 0; x < w; ++x) {
                const std::size_t ci = static_cast<std::size_t>(y) * w + x;
                const Rgb c = cur[ci];
                const float cl = c.luminance();
                Rgb sum;
                float wsum = 0.0f;
                for (int dy = -2; dy <= 2; ++dy) {
                    const int yy = static_cast<int>(y) + dy * step;
                    if (yy < 0 || yy >= static_cast<int>(h)) continue;
                    for (int dx = -2; dx <= 2; ++dx) {
                        const int xx = static_cast<int>(x) + dx * step;
                        if (xx < 0 || xx >= static_cast<int>(w)) continue;
                        const std::size_t ni = static_cast<std::size_t>(yy) * w + static_cast<std::size_t>(xx);
                        const float nw = std::pow(std::max(0.0f, dot(g[ci].normal, g[ni].normal)), 32.0f);
                        const float dz = std::fabs(g[ci].depth - g[ni].depth) / (0.05f * std::max(g[ci].depth, 1e-3f) * static_cast<float>(step) + 1e-4f);
                        const float zw = std::exp(-dz);
                        const float lw = std::exp(-std::fabs(cur[ni].luminance() - cl) / (0.6f * (cl + 0.05f)));
                        const float k = kernel[dx + 2] * kernel[dy + 2] * nw * zw * lw;
                        sum += cur[ni] * k;
                        wsum += k;
                    }
                }
                next[ci] = wsum > 0.0f ? sum / wsum : c;
            }
        }
        cur.swap(next);
    }
    for (std::size_t i = 0; i < cur.size(); ++i) {
        const Rgb a = g[i].albedo;
        img.pixels()[i] = Rgb(a.r > 1e-3f ? cur[i].r * a.r : cur[i].r, a.g > 1e-3f ? cur[i].g * a.g : cur[i].g, a.b > 1e-3f ? cur[i].b * a.b : cur[i].b);
    }
}

} // namespace

Camera Camera::look_at(const Vec3& eye, const Vec3& target, const Vec3& up_dir, float fov) {
    Camera c;
    c.position = eye;
    c.forward = (target - eye).normalized();
    c.up = up_dir;
    c.fov_degrees = fov;
    c.focus_distance = static_cast<float>((target - eye).length());
    return c;
}

struct Renderer::State {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<Rgb> sum;
    std::vector<GSample> gbuf;
    std::size_t samples = 0;
    std::size_t passes = 0;
    LightingSettings last;
    Camera camera;
    std::uint64_t geometry = ~0ULL;
    std::uint64_t lights = ~0ULL;
    const BakedLighting* baked = nullptr;
    const Scene* scene = nullptr;
    bool valid = false;
    std::uint64_t probe_geometry = ~0ULL;
    std::uint64_t probe_lights = ~0ULL;
    PhotonMap photons;
    Context ctx;
};

Renderer::Renderer() : m_state(std::make_unique<State>()) {}

Renderer::Renderer(const LightingSettings& s) : m_settings(s), m_state(std::make_unique<State>()) {}

Renderer::~Renderer() = default;

void Renderer::set_settings(const LightingSettings& s) {
    m_settings = s;
}

void Renderer::set_baked(std::shared_ptr<const BakedLighting> b) {
    m_baked = std::move(b);
}

void Renderer::reset() {
    m_state->valid = false;
    m_state->samples = 0;
    m_state->passes = 0;
}

void Renderer::prepare(Scene& scene, const Camera& camera, unsigned width, unsigned height) {
    scene.commit();
    State& st = *m_state;
    const bool changed = !st.valid || st.width != width || st.height != height || !same(st.last, m_settings) || !same(st.camera, camera) || st.geometry != scene.geometry_version() || st.lights != scene.lighting_version() || st.baked != m_baked.get() || st.scene != &scene;
    if (changed) {
        st.width = width;
        st.height = height;
        st.sum.assign(static_cast<std::size_t>(width) * height, Rgb());
        st.gbuf.assign(st.sum.size(), GSample());
        st.samples = 0;
        st.passes = 0;
        st.last = m_settings;
        st.camera = camera;
        st.geometry = scene.geometry_version();
        st.lights = scene.lighting_version();
        st.baked = m_baked.get();
        st.scene = &scene;
        st.valid = true;
    }
    Context& ctx = st.ctx;
    ctx.c = &scene.compiled();
    ctx.scene = &scene;
    ctx.s = &m_settings;
    ctx.baked = (m_settings.gi == GlobalIllumination::baked && m_baked && !m_baked->empty()) ? m_baked.get() : nullptr;
    ctx.probes = &m_probes;
    ctx.photons = nullptr;
    build_tri_light_map(ctx);
    const bool wants_probes = m_settings.quality != LightingQuality::off && (m_settings.gi == GlobalIllumination::probes || (m_settings.gi == GlobalIllumination::path_traced && m_settings.quality <= LightingQuality::realistic));
    if (wants_probes) {
        const Compiled& c = scene.compiled();
        const LightFilter filter = ctx.baked ? LightFilter::runtime : LightFilter::all;
        if (!m_probes.matches(c.lo, c.hi, m_settings.probe_spacing)) {
            m_probes.configure(c.lo.to_double(), c.hi.to_double(), m_settings.probe_spacing);
            st.probe_geometry = ~0ULL;
        }
        const bool stale = st.probe_geometry != scene.geometry_version() || st.probe_lights != scene.lighting_version();
        if (stale) {
            const bool fresh = st.probe_geometry != scene.geometry_version();
            if (fresh) m_probes.configure(c.lo.to_double(), c.hi.to_double(), m_settings.probe_spacing);
            const int bounces = std::max(m_settings.probe_bounces, 1);
            for (int i = 0; i < bounces; ++i) update_probes(m_probes, ctx, m_settings.probe_rays, fresh && i == 0 ? 0.0f : 0.3f, m_settings.seed * 977 + static_cast<std::uint64_t>(i) + st.passes * 31, filter, true, m_settings.threads);
            st.probe_geometry = scene.geometry_version();
            st.probe_lights = scene.lighting_version();
        } else {
            update_probes(m_probes, ctx, m_settings.probe_rays, m_settings.probe_hysteresis, m_settings.seed * 977 + 1000 + st.passes, filter, true, m_settings.threads);
        }
    }
    if (m_settings.caustics && m_settings.quality >= LightingQuality::path_traced) {
        const float shrink = std::pow(static_cast<float>(st.passes + 1), -1.0f / 6.0f);
        PhotonOptions po;
        po.count = m_settings.photons;
        po.bundle = m_settings.photon_bundle;
        po.max_bounces = m_settings.photon_bounces;
        po.radius = m_settings.photon_radius * shrink;
        po.seed = hash_combine(m_settings.seed, st.passes + 1);
        po.spectral = m_settings.transport == Transport::spectral;
        po.dispersion = m_settings.dispersion;
        po.threads = m_settings.threads;
        po.filter = ctx.baked ? LightFilter::runtime : LightFilter::all;
        st.photons.build(ctx, po);
        ctx.photons = &st.photons;
    }
}

void Renderer::pass(Scene& scene, const Camera& cam) {
    State& st = *m_state;
    const Context& ctx = st.ctx;
    const Tracer tracer{ ctx, m_settings, *ctx.c, ctx.baked ? LightFilter::runtime : LightFilter::all };
    (void)scene;
    const unsigned W = st.width, H = st.height;
    const Vec3f f = normalize(Vec3f(cam.forward));
    Vec3f right = normalize(cross(f, Vec3f(cam.up)));
    if (length_squared(right) == 0.0f) right = Frame(f).t;
    const Vec3f up = cross(right, f);
    const float tan_half = std::tan(std::clamp(cam.fov_degrees, 1.0f, 179.0f) * 0.5f * kDegToRad);
    const float aspect = H > 0 ? static_cast<float>(W) / static_cast<float>(H) : 1.0f;
    const Vec3f eye(cam.position);
    const int spp = std::max(1, m_settings.samples_per_pixel);
    const bool first = st.samples == 0;
    const std::uint64_t pass_seed = hash_combine(m_settings.seed, st.passes);
    parallel_for(static_cast<std::size_t>(H), m_settings.threads, [&](std::size_t y0, std::size_t y1, unsigned) {
        for (std::size_t y = y0; y < y1; ++y) {
            for (unsigned x = 0; x < W; ++x) {
                const std::size_t pi = y * W + x;
                Rng rng(hash_combine(pass_seed, pi), pi * 2 + 1);
                const float lambda_offset = static_cast<float>(hash_combine(m_settings.seed, pi) >> 40) * (1.0f / 16777216.0f);
                Rgb sum;
                GSample g;
                for (int s = 0; s < spp; ++s) {
                    const float jx = (st.samples == 0 && s == 0) ? 0.5f : rng.uniform();
                    const float jy = (st.samples == 0 && s == 0) ? 0.5f : rng.uniform();
                    const float px = (2.0f * (static_cast<float>(x) + jx) / static_cast<float>(W) - 1.0f) * tan_half * aspect;
                    const float py = (1.0f - 2.0f * (static_cast<float>(y) + jy) / static_cast<float>(H)) * tan_half;
                    Vec3f dir = normalize(f + right * px + up * py);
                    Vec3f origin = eye;
                    if (cam.aperture > 0.0f && cam.focus_distance > 0.0f) {
                        const Vec3f focus = eye + dir * (cam.focus_distance / std::max(dot(dir, f), 1e-3f));
                        float dx, dy;
                        sample_uniform_disk(rng.uniform(), rng.uniform(), dx, dy);
                        origin = eye + right * (dx * cam.aperture) + up * (dy * cam.aperture);
                        dir = normalize(focus - origin);
                    }
                    Ray ray{ origin, dir, kInfinity };
                    GSample* gp = (first && s == 0) ? &g : nullptr;
                    Rgb v;
                    switch (m_settings.quality) {
                    case LightingQuality::off: v = tracer.unlit(ray, gp); break;
                    case LightingQuality::simple:
                    case LightingQuality::shadowed:
                    case LightingQuality::realistic:
                        v = tracer.direct(ray, std::max(m_settings.reflection_bounces, 0), std::max(m_settings.transmission_depth, 0), rng, nullptr, true, gp);
                        break;
                    case LightingQuality::path_traced:
                    case LightingQuality::photon: {
                        Wave w;
                        float pdf = 1.0f;
                        if (m_settings.transport == Transport::spectral) {
                            w.spectral = true;
                            const float n = static_cast<float>(st.samples + static_cast<std::size_t>(s));
                            float u = lambda_offset + n * 0.6180339887f;
                            u -= std::floor(u);
                            w.nm = spectral::sample_wavelength(std::min(u, 0.9999999f), pdf);
                        }
                        const Rgb L = tracer.path(ray, rng, w, gp);
                        v = w.spectral ? spectral::wavelength_weight(w.nm) * (L.g / pdf) : L;
                        break;
                    }
                    }
                    if (!v.is_finite()) v = Rgb();
                    sum += v;
                }
                st.sum[pi] += sum;
                if (first) st.gbuf[pi] = g;
            }
        }
    });
    st.samples += static_cast<std::size_t>(spp);
    ++st.passes;
}

void Renderer::finish() {
    State& st = *m_state;
    m_accum_view = Image(st.width, st.height);
    const float inv = st.samples > 0 ? 1.0f / static_cast<float>(st.samples) : 0.0f;
    for (std::size_t i = 0; i < st.sum.size(); ++i) m_accum_view.pixels()[i] = st.sum[i] * inv;
    m_output = m_accum_view;
    if (m_settings.denoise && m_settings.quality != LightingQuality::off) denoise(m_output, st.gbuf, std::max(m_settings.denoise_passes, 1));
    m_stats.samples_per_pixel = st.samples;
    m_stats.passes = st.passes;
    m_stats.photons = st.photons.size();
    m_stats.probes = m_probes.count();
}

const Image& Renderer::render(Scene& scene, const Camera& camera, unsigned width, unsigned height) {
    const auto t0 = std::chrono::steady_clock::now();
    prepare(scene, camera, width, height);
    pass(scene, camera);
    finish();
    m_stats.last_pass_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return m_output;
}

const Image& Renderer::render_passes(Scene& scene, const Camera& camera, unsigned width, unsigned height, int passes) {
    for (int i = 0; i < std::max(passes, 1); ++i) render(scene, camera, width, height);
    return m_output;
}

Rgb Renderer::irradiance(Scene& scene, const Vec3& point, const Vec3& normal, int samples) {
    scene.commit();
    Context ctx;
    ctx.c = &scene.compiled();
    ctx.scene = &scene;
    ctx.s = &m_settings;
    ctx.baked = (m_settings.gi == GlobalIllumination::baked && m_baked && !m_baked->empty()) ? m_baked.get() : nullptr;
    ctx.probes = &m_probes;
    build_tri_light_map(ctx);
    const Vec3f n = normalize(Vec3f(normal));
    Rng rng(hash_combine(m_settings.seed, 0xABCDEFULL), 3);
    const LightFilter filter = ctx.baked ? LightFilter::runtime : LightFilter::all;
    if (m_settings.quality >= LightingQuality::path_traced) return irradiance_estimate(ctx, Vec3f(point), n, samples, m_settings.max_bounces, filter, filter, true, rng);
    Surface s;
    s.p = Vec3f(point) + n * 1e-3f;
    s.ng = s.ns = s.wo = n;
    s.tri = 0xFFFFFFFFu;
    static const Material white = Material::diffuse(Rgb(1.0f));
    s.m = &white;
    Bsdf b = surface_bsdf(ctx, s, Wave(), false, true);
    b.diffuse = Rgb(1.0f);
    const Frame frame(n);
    const Tracer tracer{ ctx, m_settings, *ctx.c, filter };
    DirectOptions o = tracer.direct_options();
    Rgb E;
    const int n_s = std::max(samples, 1);
    for (int i = 0; i < n_s; ++i) E += direct_lighting(ctx, s, b, frame, Vec3f{ 0.0f, 0.0f, 1.0f }, rng, o, Wave()) * kPi;
    E *= 1.0f / static_cast<float>(n_s);
    switch (m_settings.gi) {
    case GlobalIllumination::ambient: E += sky_irradiance(ctx, n) * m_settings.ambient * ambient_occlusion(ctx, s, std::max(m_settings.ambient_occlusion_samples, 8), m_settings.ambient_occlusion_distance, rng); break;
    case GlobalIllumination::baked: if (ctx.baked) E += ctx.baked->volume.irradiance_f(s.p, n); break;
    case GlobalIllumination::probes:
    case GlobalIllumination::path_traced: if (!m_probes.empty()) E += m_probes.irradiance_f(s.p, n) * m_settings.gi_intensity; break;
    default: break;
    }
    return E;
}

float Renderer::light_level(Scene& scene, const Vec3& point, const Vec3& normal, int samples) {
    return irradiance(scene, point, normal, samples).luminance();
}

bool Renderer::lit_by(Scene& scene, const Vec3& point, std::size_t light_index) {
    scene.commit();
    const Compiled& c = scene.compiled();
    for (const PreparedLight& l : c.lights) {
        if (l.source != static_cast<int>(light_index)) continue;
        const Vec3f p(point);
        if (!light_reaches(l, p)) return false;
        LightSample ls;
        if (!sample_light(l, p, 0.5f, 0.5f, false, m_settings.use_ies, m_settings.use_cookies, ls)) return false;
        const float dist = std::isfinite(ls.distance) ? ls.distance * 0.999f : 1e30f;
        Ray r{ p + ls.wi * 1e-3f, ls.wi, dist };
        return !occluded(c, r);
    }
    return false;
}

} // namespace lighting
} // namespace fizmo
