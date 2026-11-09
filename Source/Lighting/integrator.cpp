#include "integrator.hpp"

#include <atomic>
#include <thread>

namespace fizmo {
namespace lighting {
namespace detail {

namespace {

const float kChannelWavelengths[3] = { 610.0f, 550.0f, 465.0f };

float hg_phase(float cos_t, float g) noexcept {
    const float d = std::max(1.0f + g * g - 2.0f * g * cos_t, 1e-4f);
    return (1.0f - g * g) / (4.0f * kPi * d * std::sqrt(d));
}

float fog_density_at(const Fog& f, const Vec3f& x) noexcept {
    if (f.height_falloff <= 0.0f) return f.density;
    return f.density * std::exp(-f.height_falloff * std::max(x.y - f.height, -20.0f));
}

const Material& fallback_material() {
    static const Material m;
    return m;
}

Vec2f interpolate_uv(const Tri& tri, const Hit& hit) noexcept {
    const float w = 1.0f - hit.u - hit.v;
    return { tri.tex0.x * w + tri.tex1.x * hit.u + tri.tex2.x * hit.v, tri.tex0.y * w + tri.tex1.y * hit.u + tri.tex2.y * hit.v };
}

bool wants_triplanar(const Material& m) noexcept {
    return (m.base_color_map.valid() && m.base_color_map.triplanar) || (m.roughness_map.valid() && m.roughness_map.triplanar)
        || (m.metallic_map.valid() && m.metallic_map.triplanar) || (m.emission_map.valid() && m.emission_map.triplanar)
        || (m.opacity_map.valid() && m.opacity_map.triplanar) || (m.transmission_map.valid() && m.transmission_map.triplanar);
}

struct MapPoint {
    Vec2f uv;
    Vec3f p;
    Vec3f n;
};

MapPoint map_point(const Context& ctx, const Tri& tri, const Hit& hit, const Material& m, const Vec3f& world, const Vec3f& normal) {
    MapPoint mp;
    mp.uv = (tri.flags & tri_texcoords) ? interpolate_uv(tri, hit) : Vec2f{};
    mp.p = world;
    mp.n = normal;
    if (!wants_triplanar(m) || !ctx.scene || tri.instance >= ctx.scene->instances().size()) return mp;
    const auto& x = ctx.scene->instances()[tri.instance].transform.m;
    const float a = x[0], b = x[1], c = x[2], d = x[4], e = x[5], f = x[6], g = x[8], h = x[9], i = x[10];
    const float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (std::fabs(det) < 1e-20f) return mp;
    const float inv = 1.0f / det;
    const Vec3f q{ world.x - x[3], world.y - x[7], world.z - x[11] };
    mp.p = Vec3f{ (e * i - f * h) * q.x + (c * h - b * i) * q.y + (b * f - c * e) * q.z,
                  (f * g - d * i) * q.x + (a * i - c * g) * q.y + (c * d - a * f) * q.z,
                  (d * h - e * g) * q.x + (b * g - a * h) * q.y + (a * e - b * d) * q.z } * inv;
    mp.n = normalize(Vec3f{ a * normal.x + d * normal.y + g * normal.z, b * normal.x + e * normal.y + h * normal.z, c * normal.x + f * normal.y + i * normal.z });
    return mp;
}

Rgba sample_map(const TextureMap& map, const Material& m, const MapPoint& mp) noexcept {
    return map.triplanar ? map.sample_triplanar(mp.p, mp.n) : map.sample(mp.uv, m.uv);
}

float map_value(const TextureMap& map, const Material& m, const MapPoint& mp) noexcept {
    return sample_map(map, m, mp)[std::clamp(map.channel, 0, 3)];
}

float mapped_opacity(const Material& m, const MapPoint& mp) noexcept {
    float a = m.opacity;
    if (m.base_color_map.valid()) a *= sample_map(m.base_color_map, m, mp).a;
    if (m.opacity_map.valid()) a *= map_value(m.opacity_map, m, mp);
    return m.has_opacity_maps() ? m.cutout(a) : a;
}

Vec3f mapped_normal(const Context& ctx, const Tri& tri, const Hit& hit, const Material& m, const Vec3f& ns) {
    if (!(tri.flags & tri_texcoords)) return ns;
    const Vec2f uv = interpolate_uv(tri, hit);
    const Vec3f ts = decode_normal(m.normal_map.sample(uv, m.uv), m.normal_map.strength);
    Vec3f t, up;
    if (const TriTangents* found = tangents_of(*ctx.c, tri, hit)) {
        const TriTangents& tt = *found;
        const float w = 1.0f - hit.u - hit.v;
        t = tt.t[0] * w + tt.t[1] * hit.u + tt.t[2] * hit.v;
        t = t - ns * dot(ns, t);
        if (length_squared(t) < 1e-12f) return ns;
        t = normalize(t);
        up = cross(ns, t) * tt.sign[0];
    } else {
        const float du1 = tri.tex1.x - tri.tex0.x, dv1 = tri.tex1.y - tri.tex0.y;
        const float du2 = tri.tex2.x - tri.tex0.x, dv2 = tri.tex2.y - tri.tex0.y;
        const float det = du1 * dv2 - du2 * dv1;
        if (std::fabs(det) < 1e-12f) return ns;
        const float r = 1.0f / det;
        const Vec3f dpdu = (tri.e1 * dv2 - tri.e2 * dv1) * r;
        const Vec3f dpdv = (tri.e2 * du1 - tri.e1 * du2) * r;
        t = dpdu - ns * dot(ns, dpdu);
        if (length_squared(t) < 1e-12f) return ns;
        t = normalize(t);
        const Vec3f b = cross(ns, t);
        up = dot(b, dpdv) > 0.0f ? -b : b;
    }
    const Vec3f n = perturb_normal(ns, t, up, ts);
    return dot(n, n) > 0.0f ? n : ns;
}

} // namespace

void apply_material_maps(const Context& ctx, const Tri& tri, const Hit& hit, Surface& s) {
    const Material& m = *s.m;
    s.base_color = m.base_color;
    s.metallic = m.metallic;
    s.roughness = m.roughness;
    s.opacity = m.opacity;
    s.transmission = m.transmission;
    s.emission = m.emitted();
    s.mapped = false;
    if (const TriColors* vc = colors_of(*ctx.c, tri, hit)) {
        const float w = 1.0f - hit.u - hit.v;
        const Vec4f a = vc->c[0], b = vc->c[1], c = vc->c[2];
        s.base_color = s.base_color * Rgb(a.x * w + b.x * hit.u + c.x * hit.v, a.y * w + b.y * hit.u + c.y * hit.v, a.z * w + b.z * hit.u + c.z * hit.v);
        s.opacity *= a.w * w + b.w * hit.u + c.w * hit.v;
        s.mapped = true;
    }
    if (!m.has_maps()) return;
    const float vertex_alpha = m.opacity > 0.0f ? s.opacity / m.opacity : 1.0f;
    s.mapped = true;
    const MapPoint mp = map_point(ctx, tri, hit, m, s.p, s.ns);
    s.texcoord = mp.uv;
    if (m.base_color_map.valid()) s.base_color = s.base_color * sample_map(m.base_color_map, m, mp).rgb();
    s.opacity = mapped_opacity(m, mp) * vertex_alpha;
    if (m.roughness_map.valid()) s.roughness *= map_value(m.roughness_map, m, mp);
    if (m.metallic_map.valid()) s.metallic *= map_value(m.metallic_map, m, mp);
    if (m.transmission_map.valid()) s.transmission *= map_value(m.transmission_map, m, mp);
    if (m.emission_map.valid()) s.emission = s.emission * sample_map(m.emission_map, m, mp).rgb();
}

float hit_opacity(const Context& ctx, const Tri& tri, const Hit& hit, const Material& m) {
    if (!m.has_opacity_maps()) return m.opacity;
    const Vec3f n = normalize(cross(tri.e1, tri.e2));
    const Vec3f p = tri.v0 + tri.e1 * hit.u + tri.e2 * hit.v;
    return mapped_opacity(m, map_point(ctx, tri, hit, m, p, n));
}

float Wave::ior(const Material& m, bool dispersion) const noexcept {
    if (!dispersion || m.abbe <= 0.0f) return m.ior;
    if (spectral) return m.ior_at(nm);
    if (channel >= 0 && channel < 3) return m.ior_at(kChannelWavelengths[channel]);
    return m.ior;
}

const Material& Context::material(std::uint32_t i) const {
    const auto& mats = scene->materials();
    return i < mats.size() ? mats[i] : fallback_material();
}

unsigned thread_count(unsigned requested) {
    if (requested > 0) return requested;
    const unsigned hw = std::thread::hardware_concurrency();
    return hw > 0 ? hw : 4u;
}

void parallel_for(std::size_t count, unsigned threads, const std::function<void(std::size_t, std::size_t, unsigned)>& fn) {
    if (count == 0) return;
    const unsigned n = std::max(1u, std::min<unsigned>(thread_count(threads), static_cast<unsigned>(count)));
    if (n == 1) {
        fn(0, count, 0);
        return;
    }
    const std::size_t chunk = std::max<std::size_t>(1, count / (static_cast<std::size_t>(n) * 8));
    std::atomic<std::size_t> next{ 0 };
    std::vector<std::thread> pool;
    pool.reserve(n);
    for (unsigned t = 0; t < n; ++t) {
        pool.emplace_back([&, t]() {
            for (;;) {
                const std::size_t begin = next.fetch_add(chunk);
                if (begin >= count) break;
                fn(begin, std::min(count, begin + chunk), t);
            }
        });
    }
    for (auto& th : pool) th.join();
}

void build_tri_light_map(Context& ctx) {
    ctx.cutouts = false;
    if (ctx.scene)
        for (const Material& m : ctx.scene->materials()) ctx.cutouts = ctx.cutouts || m.has_opacity_maps();
    ctx.tri_light.assign(ctx.c->tris.size(), -1);
    for (std::size_t i = ctx.c->emissive_begin.empty() ? ctx.c->lights.size() : ctx.c->emissive_begin.front(); i < ctx.c->lights.size(); ++i) {
        const PreparedLight& l = ctx.c->lights[i];
        if (l.kind == LightKind::triangle && l.tri < ctx.tri_light.size()) ctx.tri_light[l.tri] = static_cast<int>(i);
    }
}

Surface make_surface(const Context& ctx, const Ray& ray, const Hit& hit) {
    const Tri& tri = tri_of(*ctx.c, hit);
    Surface s;
    s.tri = hit.tri;
    s.local = tri.local;
    s.instance = tri.instance;
    s.u = hit.u;
    s.v = hit.v;
    s.p = ray.o + ray.d * hit.t;
    s.m = &ctx.material(tri.material);
    s.is_static = (tri.flags & tri_static) != 0;
    s.channels = tri.instance < ctx.c->instance_channels.size() ? ctx.c->instance_channels[tri.instance] : 1u;
    Vec3f gn = normalize(cross(tri.e1, tri.e2));
    const float w = 1.0f - hit.u - hit.v;
    Vec3f ns = (tri.flags & tri_smooth) ? normalize(tri.n0 * w + tri.n1 * hit.u + tri.n2 * hit.v) : gn;
    if (length_squared(ns) == 0.0f) ns = gn;
    if (s.m->normal_map.valid()) ns = mapped_normal(ctx, tri, hit, *s.m, ns);
    if (dot(gn, ns) < 0.0f) gn = -gn;
    s.front = dot(ray.d, gn) < 0.0f;
    if (!s.front) {
        gn = -gn;
        ns = -ns;
    }
    s.ng = gn;
    s.wo = -ray.d;
    if (dot(ns, s.wo) <= 1e-4f) ns = gn;
    s.ns = ns;
    if (tri.flags & tri_lightmap) {
        s.lightmap = true;
        s.uv = { tri.uv0.x * w + tri.uv1.x * hit.u + tri.uv2.x * hit.v, tri.uv0.y * w + tri.uv1.y * hit.u + tri.uv2.y * hit.v };
    }
    apply_material_maps(ctx, tri, hit, s);
    return s;
}

Bsdf surface_bsdf(const Context& ctx, const Surface& s, const Wave& w, bool dispersion, bool diffuse_only) {
    (void)ctx;
    const Material& m = *s.m;
    Bsdf b = s.mapped ? make_bsdf(m, w.lift(s.base_color), s.metallic, s.roughness, s.transmission, std::max(dot(s.ns, s.wo), 1e-4f), s.front, w.ior(m, dispersion))
                      : make_bsdf(m, w.lift(m.base_color), std::max(dot(s.ns, s.wo), 1e-4f), s.front, w.ior(m, dispersion));
    if (diffuse_only) {
        b.diffuse_only = true;
        b.p_trans = 0.0f;
        b.p_spec = 0.0f;
        b.p_diffuse = b.diffuse.is_black() ? 0.0f : 1.0f;
        b.delta_spec = false;
        b.f0 = Rgb();
    }
    return b;
}

float surface_eps(const Surface& s) noexcept {
    const float m = std::max(std::fabs(s.p.x), std::max(std::fabs(s.p.y), std::fabs(s.p.z)));
    return 2e-4f * (1.0f + m * 0.01f) + m * 2e-6f;
}

Vec3f offset_ray(const Surface& s, const Vec3f& dir) noexcept {
    const float e = surface_eps(s);
    return s.p + s.ng * (dot(dir, s.ng) >= 0.0f ? e : -e);
}

Rgb transmittance(const Context& ctx, const Vec3f& from, const Vec3f& dir, float dist, bool colored, const Wave& w) {
    if (!colored && !ctx.cutouts) {
        Ray r{ from, dir, dist };
        return occluded(*ctx.c, r) ? Rgb() : Rgb(1.0f);
    }
    if (!colored) {
        Vec3f o = from;
        float left = dist;
        for (int i = 0; i < 16 && left > 1e-5f; ++i) {
            Ray r{ o, dir, left };
            Hit h;
            if (!intersect(*ctx.c, r, h, tri_casts_shadows)) return Rgb(1.0f);
            const Tri& tri = tri_of(*ctx.c, h);
            const Material& m = ctx.material(tri.material);
            if (!m.has_opacity_maps() || hit_opacity(ctx, tri, h, m) >= 0.5f) return Rgb();
            const float step = h.t + 1e-4f;
            o = o + dir * step;
            left -= step;
        }
        return Rgb();
    }
    Rgb t(1.0f);
    Vec3f o = from;
    float left = dist;
    for (int i = 0; i < 16 && left > 1e-5f; ++i) {
        Ray r{ o, dir, left };
        Hit h;
        if (!intersect(*ctx.c, r, h, tri_casts_shadows)) return t;
        const Tri& hit_tri = tri_of(*ctx.c, h);
        const Material& m = ctx.material(hit_tri.material);
        const bool cut = m.has_opacity_maps();
        const float alpha = cut ? hit_opacity(ctx, hit_tri, h, m) : m.opacity;
        if (cut && alpha <= 0.0f) {
            t *= Rgb(1.0f);
        } else if (m.is_transmissive()) {
            const float pass = clamp01(m.transmission) * (1.0f - clamp01(m.metallic));
            Rgb tint = w.lift(m.base_color);
            const float cos_t = std::fabs(dot(dir, normalize(cross(hit_tri.e1, hit_tri.e2))));
            const float f = fresnel_dielectric(cos_t, 1.0f / std::max(m.ior, 1.0001f));
            t *= tint * (pass * (1.0f - f) + (1.0f - pass) * 0.0f);
        } else if (alpha < 1.0f) {
            t *= Rgb(1.0f - clamp01(alpha));
        } else {
            return Rgb();
        }
        if (t.max_component() < 1e-4f) return Rgb();
        const float step = h.t + 1e-4f;
        o = o + dir * step;
        left -= step;
    }
    return t;
}

bool light_allowed(const PreparedLight& l, LightFilter f) noexcept {
    switch (f) {
    case LightFilter::all: return true;
    case LightFilter::runtime: return l.mobility != LightMobility::baked;
    case LightFilter::baked_only: return l.mobility == LightMobility::baked;
    case LightFilter::baked_and_stationary: return l.mobility != LightMobility::dynamic;
    }
    return true;
}

namespace {

bool casts_for(const PreparedLight& l, ShadowCasters casters) noexcept {
    switch (casters) {
    case ShadowCasters::everything: return true;
    case ShadowCasters::flagged: return l.shadows;
    case ShadowCasters::sun_only: return l.shadows && l.kind == LightKind::directional;
    }
    return l.shadows;
}

bool usable(const Context& ctx, const PreparedLight& l, const Surface& s, const DirectOptions& o) noexcept {
    if (!light_allowed(l, o.filter)) return false;
    if ((l.channels & s.channels) == 0) return false;
    if (l.kind == LightKind::triangle) {
        if (!o.emissive || !ctx.s->emissive_lights) return false;
        if (l.tri == s.tri) return false;
    }
    if (o.indirect_scale && l.indirect <= 0.0f) return false;
    return light_reaches(l, s.p);
}

} // namespace

float light_select_pdf(const Context& ctx, std::size_t light, const Vec3f& p, const Vec3f& n) {
    const auto& lights = ctx.c->lights;
    const std::size_t na = ctx.c->analytic_count;
    const int threshold = std::max(ctx.s->all_lights_threshold, 0);
    const float k = static_cast<float>(std::max(ctx.s->light_samples, 1));
    if (light >= na) {
        const std::size_t ne = lights.size() - na;
        if (static_cast<int>(ne) <= threshold) return 1.0f;
        return ctx.c->emissive_power > 0.0f ? k * lights[light].power / ctx.c->emissive_power : 0.0f;
    }
    if (static_cast<int>(na) <= threshold) return 1.0f;
    float total = 0.0f, mine = 0.0f;
    for (std::size_t i = 0; i < na; ++i) {
        const float imp = light_importance(lights[i], p, n);
        total += imp;
        if (i == light) mine = imp;
    }
    if (total <= 0.0f) return 0.0f;
    return k * mine / total;
}

Rgb direct_lighting(const Context& ctx, const Surface& s, const Bsdf& b, const Frame& frame, const Vec3f& wo, Rng& rng, const DirectOptions& o, const Wave& w) {
    Rgb sum;
    if (!b.has_smooth_lobes()) return sum;
    const auto& lights = ctx.c->lights;
    const LightingSettings& st = *ctx.s;
    auto shade_one = [&](std::size_t idx, float select_pdf, float scale) {
        const PreparedLight& l = lights[idx];
        const bool area_light = l.kind != LightKind::point && !(l.kind == LightKind::directional && l.cos_sun >= 1.0f);
        int n = 1;
        if (!o.mis && area_light && o.area && o.shadows == ShadowMethod::soft) n = o.samples > 0 ? o.samples : (l.shadow_samples > 0 ? l.shadow_samples : std::max(st.shadow_samples, 1));
        const bool shadow = o.shadows != ShadowMethod::none && casts_for(l, o.casters);
        Rgb acc;
        for (int j = 0; j < n; ++j) {
            LightSample ls;
            float u1, u2;
            if (n > 1) {
                const int side = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(n))));
                u1 = (static_cast<float>(j % side) + rng.uniform()) / static_cast<float>(side);
                u2 = (static_cast<float>((j / side) % side) + rng.uniform()) / static_cast<float>(side);
            } else {
                u1 = rng.uniform();
                u2 = rng.uniform();
            }
            if (!sample_light(l, s.p, u1, u2, o.area, o.use_ies && st.use_ies, o.use_cookie && st.use_cookies, ls)) continue;
            const Vec3f wi = frame.to_local(ls.wi);
            if (wi.z <= 0.0f) continue;
            const BsdfEval e = bsdf_eval(b, wo, wi);
            Rgb f = e.diffuse * l.diffuse + e.specular * l.specular;
            if (f.is_black()) continue;
            Rgb c = f * w.lift(ls.radiance) * (wi.z / ls.pdf);
            if (o.mis && !ls.delta) {
                const float bp = bsdf_pdf(b, wo, wi);
                c *= power_heuristic(ls.pdf * select_pdf, bp);
            }
            if (shadow) {
                Vec3f from = offset_ray(s, ls.wi);
                if (l.bias > 0.0f) from = from + ls.wi * l.bias;
                if (st.shadow_bias > 0.0f) from = from + ls.wi * st.shadow_bias;
                const float dist = std::isfinite(ls.distance) ? std::max(ls.distance * (1.0f - 1e-3f) - surface_eps(s) * 2.0f, 0.0f) : 1e30f;
                Rgb vis = transmittance(ctx, from, ls.wi, dist, o.colored_shadows, w);
                if (l.shadow_strength < 1.0f) vis = Rgb(1.0f) - (Rgb(1.0f) - vis) * l.shadow_strength;
                c *= vis;
            }
            acc += c;
        }
        if (n > 0) acc *= 1.0f / static_cast<float>(n);
        if (o.indirect_scale) acc *= l.indirect;
        sum += acc * scale;
    };
    const std::size_t na = ctx.c->analytic_count;
    const std::size_t ne = lights.size() - std::min(na, lights.size());
    const int threshold = std::max(st.all_lights_threshold, 0);
    const int k = std::max(st.light_samples, 1);
    if (!o.mis || static_cast<int>(na) <= threshold) {
        for (std::size_t i = 0; i < na; ++i) {
            if (!usable(ctx, lights[i], s, o)) continue;
            shade_one(i, 1.0f, 1.0f);
        }
    } else if (na > 0) {
        std::vector<float> cdf(na + 1, 0.0f);
        for (std::size_t i = 0; i < na; ++i) cdf[i + 1] = cdf[i] + (usable(ctx, lights[i], s, o) ? light_importance(lights[i], s.p, s.ns) : 0.0f);
        const float total = cdf.back();
        if (total > 0.0f) {
            for (int j = 0; j < k; ++j) {
                const float target = rng.uniform() * total;
                std::size_t idx = static_cast<std::size_t>(std::upper_bound(cdf.begin(), cdf.end(), target) - cdf.begin());
                idx = std::min(std::max<std::size_t>(idx, 1), na) - 1;
                const float p = (cdf[idx + 1] - cdf[idx]) / total;
                if (p <= 0.0f) continue;
                shade_one(idx, static_cast<float>(k) * p, 1.0f / (static_cast<float>(k) * p));
            }
        }
    }
    if (ne > 0 && o.emissive && st.emissive_lights) {
        if (static_cast<int>(ne) <= threshold) {
            for (std::size_t i = na; i < lights.size(); ++i) {
                if (!usable(ctx, lights[i], s, o)) continue;
                shade_one(i, 1.0f, 1.0f);
            }
        } else if (ctx.c->emissive_power > 0.0f) {
            const auto& cdf = ctx.c->emissive_cdf;
            for (int j = 0; j < k; ++j) {
                const float target = rng.uniform() * ctx.c->emissive_power;
                std::size_t e = static_cast<std::size_t>(std::upper_bound(cdf.begin(), cdf.end(), target) - cdf.begin());
                e = std::min(std::max<std::size_t>(e, 1), ne) - 1;
                const std::size_t idx = na + e;
                const float p = lights[idx].power / ctx.c->emissive_power;
                if (p <= 0.0f || !usable(ctx, lights[idx], s, o)) continue;
                shade_one(idx, static_cast<float>(k) * p, 1.0f / (static_cast<float>(k) * p));
            }
        }
    }
    if (o.include_environment && ctx.c->env_sampler.ready() && ctx.c->env.lights_scene) {
        Vec3f dir;
        Rgb rad;
        float pdf;
        if (sample_environment(*ctx.c, rng.uniform(), rng.uniform(), dir, rad, pdf)) {
            const Vec3f wi = frame.to_local(dir);
            if (wi.z > 0.0f) {
                const Rgb f = bsdf_eval(b, wo, wi).total();
                if (!f.is_black()) {
                    Rgb c = f * w.lift(rad) * (wi.z / pdf);
                    if (o.mis) c *= power_heuristic(pdf, bsdf_pdf(b, wo, wi));
                    if (o.shadows != ShadowMethod::none) c *= transmittance(ctx, offset_ray(s, dir), dir, 1e30f, false, w);
                    sum += c;
                }
            }
        }
    }
    return sum;
}

Rgb sky(const Context& ctx, const Vec3f& dir, const Wave& w) {
    return w.lift(sky_radiance(*ctx.c, dir));
}

Rgb sky_irradiance(const Context& ctx, const Vec3f& n) {
    if (!ctx.c->env.lights_scene) return Rgb();
    return ctx.c->env_sh.irradiance(n);
}

float ambient_occlusion(const Context& ctx, const Surface& s, int samples, float distance, Rng& rng) {
    if (samples <= 0) return 1.0f;
    const Frame f(s.ns);
    int open = 0;
    for (int i = 0; i < samples; ++i) {
        const Vec3f d = f.to_world(sample_cosine_hemisphere(rng.uniform(), rng.uniform()));
        if (dot(d, s.ng) <= 0.0f) {
            ++open;
            continue;
        }
        Ray r{ offset_ray(s, d), d, distance };
        if (!occluded(*ctx.c, r)) ++open;
    }
    return static_cast<float>(open) / static_cast<float>(samples);
}

Rgb baked_irradiance(const Context& ctx, const Surface& s) {
    const BakedLighting* b = ctx.baked;
    if (!b) return Rgb();
    if (s.instance < b->lightmaps.size() && s.lightmap && !b->lightmaps[s.instance].empty()) return b->lightmaps[s.instance].sample(s.uv.x, s.uv.y);
    if (s.instance < b->vertex.size() && !b->vertex[s.instance].empty()) {
        const auto& insts = ctx.scene->instances();
        if (s.instance < insts.size()) {
            const Instance& inst = insts[s.instance];
            if (inst.mesh < ctx.scene->meshes().size()) {
                const Mesh& m = ctx.scene->meshes()[inst.mesh];
                const std::size_t t = static_cast<std::size_t>(s.local) * 3;
                const auto& vx = b->vertex[s.instance];
                if (t + 2 < m.indices.size() && m.indices[t + 2] < vx.size() && m.indices[t] < vx.size() && m.indices[t + 1] < vx.size()) {
                    const float w = 1.0f - s.u - s.v;
                    return vx[m.indices[t]] * w + vx[m.indices[t + 1]] * s.u + vx[m.indices[t + 2]] * s.v;
                }
            }
        }
    }
    if (!b->volume.empty()) return b->volume.irradiance_f(s.p, s.ns);
    return Rgb();
}

Rgb fog_transmittance(const LightingSettings& st, const Vec3f& o, const Vec3f& d, float t) {
    const Fog& f = st.fog;
    if (!f.enabled || f.density <= 0.0f) return Rgb(1.0f);
    t = std::min(t, f.max_distance);
    float tau;
    if (f.height_falloff > 0.0f && std::fabs(d.y) > 1e-4f) {
        const float k = f.height_falloff * d.y;
        tau = f.density * std::exp(-f.height_falloff * std::max(o.y - f.height, -20.0f)) * (1.0f - std::exp(-k * t)) / k;
    } else {
        tau = fog_density_at(f, o) * t;
    }
    return Rgb(std::exp(-std::max(tau, 0.0f)));
}

Rgb fog_inscatter(const Context& ctx, const Vec3f& o, const Vec3f& d, float t, Rng& rng, const Wave& w) {
    const LightingSettings& st = *ctx.s;
    const Fog& f = st.fog;
    if (!f.enabled || f.density <= 0.0f) return Rgb();
    const float len = std::min(t, f.max_distance);
    const int steps = std::max(f.steps, 1);
    const float ds = len / static_cast<float>(steps);
    const Rgb albedo = w.lift(f.color);
    const Rgb ambient = ctx.c->env.lights_scene ? w.lift(ctx.c->env_sh.average()) * (4.0f * kPi) * kInvFourPi : Rgb();
    Rgb sum;
    const float jitter = rng.uniform();
    for (int i = 0; i < steps; ++i) {
        const float s = (static_cast<float>(i) + jitter) * ds;
        const Vec3f x = o + d * s;
        const float sigma = fog_density_at(f, x);
        if (sigma <= 0.0f) continue;
        const Rgb tr = fog_transmittance(st, o, d, s);
        Rgb in = ambient;
        for (const PreparedLight& l : ctx.c->lights) {
            if (l.volumetric <= 0.0f || l.kind == LightKind::triangle) continue;
            if (!light_reaches(l, x)) continue;
            LightSample ls;
            if (!sample_light(l, x, rng.uniform(), rng.uniform(), false, st.use_ies, st.use_cookies, ls)) continue;
            if (st.quality != LightingQuality::simple && l.shadows) {
                const float dist = std::isfinite(ls.distance) ? ls.distance * 0.999f : 1e30f;
                Ray r{ x, ls.wi, dist };
                if (occluded(*ctx.c, r)) continue;
            }
            in += w.lift(ls.radiance) * (hg_phase(dot(ls.wi, d), f.anisotropy) * l.volumetric);
        }
        sum += tr * albedo * in * (sigma * ds);
    }
    return sum;
}

Rgb irradiance_estimate(const Context& ctx, const Vec3f& p, const Vec3f& n, int samples, int bounces, LightFilter direct_filter, LightFilter indirect_filter, bool include_sky, Rng& rng) {
    Surface s;
    s.p = p;
    s.ng = n;
    s.ns = n;
    s.wo = n;
    s.tri = 0xFFFFFFFFu;
    static const Material white = Material::diffuse(Rgb(1.0f));
    s.m = &white;
    const Wave wave;
    Bsdf b = surface_bsdf(ctx, s, wave, false, true);
    b.diffuse = Rgb(1.0f);
    const Frame frame(n);
    const Vec3f wo{ 0.0f, 0.0f, 1.0f };
    DirectOptions od;
    od.area = true;
    od.samples = 1;
    od.shadows = ShadowMethod::soft;
    od.casters = ShadowCasters::flagged;
    od.colored_shadows = true;
    od.filter = direct_filter;
    od.emissive = false;
    od.include_environment = false;
    Rgb direct;
    const int dn = std::max(1, samples / 8);
    for (int i = 0; i < dn; ++i) direct += direct_lighting(ctx, s, b, frame, wo, rng, od, wave) * kPi;
    direct *= 1.0f / static_cast<float>(dn);
    Rgb indirect;
    const int n_samples = std::max(samples, 1);
    DirectOptions oi = od;
    oi.filter = indirect_filter;
    oi.indirect_scale = true;
    for (int i = 0; i < n_samples; ++i) {
        Vec3f dir = frame.to_world(sample_cosine_hemisphere(rng.uniform(), rng.uniform()));
        Vec3f o = p + n * 2e-3f;
        Rgb beta(1.0f);
        Rgb L;
        for (int depth = 0; depth < std::max(bounces, 1); ++depth) {
            Ray r{ o, dir, kInfinity };
            Hit h;
            if (!intersect(*ctx.c, r, h, 0)) {
                if (include_sky) L += beta * sky(ctx, dir, wave);
                break;
            }
            const Surface hs = make_surface(ctx, r, h);
            if (hs.emits()) {
                PreparedLight probe;
                probe.mobility = hs.is_static ? LightMobility::baked : LightMobility::dynamic;
                const bool counted = light_allowed(probe, indirect_filter);
                if (counted) L += beta * hs.emitted();
            }
            if (!hs.front && !hs.m->is_transmissive()) break;
            const Bsdf hb = surface_bsdf(ctx, hs, wave, false);
            const Frame hf(hs.ns);
            const Vec3f hwo = hf.to_local(hs.wo);
            L += beta * direct_lighting(ctx, hs, hb, hf, hwo, rng, oi, wave);
            BsdfSample bs;
            if (!bsdf_sample(hb, hwo, rng.uniform(), rng.uniform(), rng.uniform(), rng.uniform(), bs)) break;
            beta *= bs.weight;
            if (beta.max_component() <= 1e-5f) break;
            dir = normalize(hf.to_world(bs.wi));
            o = offset_ray(hs, dir);
            if (depth >= 2) {
                const float q = std::max(0.05f, 1.0f - beta.max_component());
                if (rng.uniform() < q) break;
                beta *= 1.0f / (1.0f - q);
            }
        }
        indirect += L;
    }
    indirect *= kPi / static_cast<float>(n_samples);
    return direct + indirect;
}

void update_probes(ProbeVolume& volume, const Context& ctx, int rays, float hysteresis, std::uint64_t frame, LightFilter filter, bool include_sky, unsigned threads) {
    if (volume.empty()) return;
    rays = std::max(rays, 8);
    const std::size_t count = volume.count();
    std::vector<Sh9> fresh(count);
    std::vector<std::uint8_t> alive(count, 1);
    const LightingSettings& st = *ctx.s;
    const Wave wave;
    parallel_for(count, threads, [&](std::size_t begin, std::size_t end, unsigned) {
        for (std::size_t i = begin; i < end; ++i) {
            Rng rng(hash_combine(frame * 0x9E3779B97F4A7C15ULL, i), i * 2 + 1);
            const Vec3f origin = volume.position_f(i);
            const Frame rot(sample_uniform_sphere(rng.uniform(), rng.uniform()));
            Sh9 sh;
            int back = 0;
            const float weight = 4.0f * kPi / static_cast<float>(rays);
            for (int r = 0; r < rays; ++r) {
                const float z = 1.0f - (2.0f * static_cast<float>(r) + 1.0f) / static_cast<float>(rays);
                const float rad = std::sqrt(std::max(0.0f, 1.0f - z * z));
                const float phi = static_cast<float>(r) * 2.39996323f;
                const Vec3f dir = normalize(rot.to_world(Vec3f{ rad * std::cos(phi), rad * std::sin(phi), z }));
                Ray ray{ origin, dir, kInfinity };
                Hit h;
                if (!intersect(*ctx.c, ray, h, 0)) {
                    if (include_sky) sh.add(dir, sky(ctx, dir, wave), weight);
                    continue;
                }
                const Surface s = make_surface(ctx, ray, h);
                if (!s.front && !s.m->is_transmissive() && !s.m->double_sided) {
                    ++back;
                    continue;
                }
                if (!s.front && !s.m->is_transmissive()) {
                    ++back;
                    continue;
                }
                Rgb L = s.emitted();
                const Bsdf b = surface_bsdf(ctx, s, wave, false, true);
                if (!b.diffuse.is_black()) {
                    const Frame f(s.ns);
                    DirectOptions o;
                    o.area = st.area_lights;
                    o.samples = 1;
                    o.shadows = st.shadows == ShadowMethod::none ? ShadowMethod::none : ShadowMethod::hard;
                    o.casters = st.shadow_casters;
                    o.colored_shadows = true;
                    o.filter = filter;
                    o.emissive = st.emissive_lights;
                    o.indirect_scale = true;
                    L += direct_lighting(ctx, s, b, f, f.to_local(s.wo), rng, o, wave);
                    L += b.diffuse * volume.irradiance_f(s.p, s.ns) * (kInvPi * st.gi_intensity);
                }
                sh.add(dir, L, weight);
            }
            fresh[i] = sh;
            alive[i] = back * 4 < rays ? 1 : 0;
        }
    });
    const bool first = volume.updates() == 0;
    const float keep = std::clamp(hysteresis, 0.0f, 0.99f);
    for (std::size_t i = 0; i < count; ++i) {
        if (first) volume.probe(i) = fresh[i];
        else volume.probe(i).lerp_to(fresh[i], 1.0f - keep);
        volume.set_live(i, alive[i] != 0);
    }
    volume.count_update();
}

} // namespace detail
} // namespace lighting
} // namespace fizmo
