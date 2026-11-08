#include "bake.hpp"
#include "integrator.hpp"

#include <atomic>
#include <cstring>
#include <fstream>

namespace fizmo {
namespace lighting {

using namespace detail;

namespace {

struct Texel {
    Vec3f p;
    Vec3f n;
    std::uint32_t index = 0;
};

Rgb trace_radiance(const Context& ctx, Vec3f o, Vec3f dir, int bounces, LightFilter filter, bool include_sky, Rng& rng) {
    const Wave wave;
    Rgb L, beta(1.0f);
    DirectOptions od;
    od.area = true;
    od.samples = 1;
    od.shadows = ShadowMethod::soft;
    od.casters = ShadowCasters::flagged;
    od.colored_shadows = true;
    od.filter = filter;
    od.emissive = false;
    od.indirect_scale = true;
    for (int depth = 0; depth < std::max(bounces, 1); ++depth) {
        Ray r{ o, dir, kInfinity };
        Hit h;
        if (!intersect(*ctx.c, r, h, 0)) {
            if (include_sky) L += beta * sky(ctx, dir, wave);
            break;
        }
        const Surface s = make_surface(ctx, r, h);
        if (s.emits()) {
            PreparedLight probe;
            probe.mobility = s.is_static ? LightMobility::baked : LightMobility::dynamic;
            if (light_allowed(probe, filter)) L += beta * s.emitted();
        }
        if (!s.front && !s.m->is_transmissive()) break;
        const Bsdf b = surface_bsdf(ctx, s, wave, false);
        const Frame f(s.ns);
        const Vec3f wo = f.to_local(s.wo);
        L += beta * direct_lighting(ctx, s, b, f, wo, rng, od, wave);
        BsdfSample bs;
        if (!bsdf_sample(b, wo, rng.uniform(), rng.uniform(), rng.uniform(), rng.uniform(), bs)) break;
        beta *= bs.weight;
        if (beta.max_component() <= 1e-5f) break;
        dir = normalize(f.to_world(bs.wi));
        o = offset_ray(s, dir);
        if (depth >= 2) {
            const float q = std::clamp(1.0f - beta.max_component(), 0.05f, 0.95f);
            if (rng.uniform() < q) break;
            beta *= 1.0f / (1.0f - q);
        }
    }
    return L;
}

void dilate(Lightmap& lm, std::vector<std::uint8_t>& covered, int passes) {
    const int w = static_cast<int>(lm.width), h = static_cast<int>(lm.height);
    for (int it = 0; it < passes; ++it) {
        std::vector<std::uint8_t> next = covered;
        std::vector<Rgb> texels = lm.texels;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * lm.width + static_cast<std::size_t>(x);
                if (covered[i]) continue;
                Rgb sum;
                int n = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int xx = x + dx, yy = y + dy;
                        if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                        const std::size_t j = static_cast<std::size_t>(yy) * lm.width + static_cast<std::size_t>(xx);
                        if (!covered[j]) continue;
                        sum += lm.texels[j];
                        ++n;
                    }
                }
                if (n > 0) {
                    texels[i] = sum / static_cast<float>(n);
                    next[i] = 1;
                }
            }
        }
        lm.texels.swap(texels);
        covered.swap(next);
    }
}

template <class T>
void put(std::ofstream& f, const T& v) {
    f.write(reinterpret_cast<const char*>(&v), sizeof(T));
}

template <class T>
bool get(std::ifstream& f, T& v) {
    return static_cast<bool>(f.read(reinterpret_cast<char*>(&v), sizeof(T)));
}

} // namespace

Rgb Lightmap::sample(float u, float v) const noexcept {
    if (empty()) return Rgb();
    const float x = clamp01(u) * static_cast<float>(width) - 0.5f, y = clamp01(v) * static_cast<float>(height) - 0.5f;
    const int x0 = std::clamp(static_cast<int>(std::floor(x)), 0, static_cast<int>(width) - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(y)), 0, static_cast<int>(height) - 1);
    const int x1 = std::min(x0 + 1, static_cast<int>(width) - 1), y1 = std::min(y0 + 1, static_cast<int>(height) - 1);
    const float fx = clamp01(x - static_cast<float>(x0)), fy = clamp01(y - static_cast<float>(y0));
    auto at = [this](int px, int py) { return texels[static_cast<std::size_t>(py) * width + static_cast<std::size_t>(px)]; };
    const Rgb a = at(x0, y0) * (1.0f - fx) + at(x1, y0) * fx;
    const Rgb b = at(x0, y1) * (1.0f - fx) + at(x1, y1) * fx;
    return a * (1.0f - fy) + b * fy;
}

bool BakedLighting::has(std::size_t instance) const noexcept {
    return (instance < vertex.size() && !vertex[instance].empty()) || (instance < lightmaps.size() && !lightmaps[instance].empty());
}

bool BakedLighting::save(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write("FZLB", 4);
    put(f, std::uint32_t(1));
    put(f, geometry_version);
    put(f, lighting_version);
    put(f, static_cast<std::uint64_t>(vertex.size()));
    for (const auto& v : vertex) {
        put(f, static_cast<std::uint64_t>(v.size()));
        if (!v.empty()) f.write(reinterpret_cast<const char*>(v.data()), static_cast<std::streamsize>(v.size() * sizeof(Rgb)));
    }
    put(f, static_cast<std::uint64_t>(lightmaps.size()));
    for (const auto& lm : lightmaps) {
        put(f, lm.width);
        put(f, lm.height);
        if (!lm.texels.empty()) f.write(reinterpret_cast<const char*>(lm.texels.data()), static_cast<std::streamsize>(lm.texels.size() * sizeof(Rgb)));
    }
    const std::uint8_t has_volume = volume.empty() ? 0 : 1;
    put(f, has_volume);
    if (has_volume) {
        const Vec3 lo = volume.requested_lo(), hi = volume.requested_hi();
        put(f, lo);
        put(f, hi);
        put(f, volume.requested_spacing());
        put(f, static_cast<std::uint64_t>(volume.count()));
        for (std::size_t i = 0; i < volume.count(); ++i) {
            put(f, volume.probe(i));
            put(f, static_cast<std::uint8_t>(volume.live(i) ? 1 : 0));
        }
    }
    return static_cast<bool>(f);
}

std::optional<BakedLighting> BakedLighting::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    char magic[4];
    if (!f.read(magic, 4) || std::memcmp(magic, "FZLB", 4) != 0) return std::nullopt;
    std::uint32_t version = 0;
    if (!get(f, version) || version != 1) return std::nullopt;
    BakedLighting b;
    std::uint64_t n = 0;
    if (!get(f, b.geometry_version) || !get(f, b.lighting_version) || !get(f, n)) return std::nullopt;
    b.vertex.resize(static_cast<std::size_t>(n));
    for (auto& v : b.vertex) {
        std::uint64_t c = 0;
        if (!get(f, c)) return std::nullopt;
        v.resize(static_cast<std::size_t>(c));
        if (c && !f.read(reinterpret_cast<char*>(v.data()), static_cast<std::streamsize>(c * sizeof(Rgb)))) return std::nullopt;
    }
    if (!get(f, n)) return std::nullopt;
    b.lightmaps.resize(static_cast<std::size_t>(n));
    for (auto& lm : b.lightmaps) {
        if (!get(f, lm.width) || !get(f, lm.height)) return std::nullopt;
        lm.texels.resize(static_cast<std::size_t>(lm.width) * lm.height);
        if (!lm.texels.empty() && !f.read(reinterpret_cast<char*>(lm.texels.data()), static_cast<std::streamsize>(lm.texels.size() * sizeof(Rgb)))) return std::nullopt;
    }
    std::uint8_t has_volume = 0;
    if (!get(f, has_volume)) return std::nullopt;
    if (has_volume) {
        Vec3 lo, hi;
        float spacing = 1.0f;
        std::uint64_t count = 0;
        if (!get(f, lo) || !get(f, hi) || !get(f, spacing) || !get(f, count)) return std::nullopt;
        b.volume.configure(lo, hi, spacing);
        if (b.volume.count() != count) return std::nullopt;
        for (std::size_t i = 0; i < count; ++i) {
            std::uint8_t live = 1;
            if (!get(f, b.volume.probe(i)) || !get(f, live)) return std::nullopt;
            b.volume.set_live(i, live != 0);
        }
        b.volume.count_update();
    }
    return b;
}

BakedLighting bake(Scene& scene, const LightingSettings& settings, const BakeOptions& o, const std::function<void(float)>& progress) {
    scene.commit();
    BakedLighting out;
    out.geometry_version = scene.geometry_version();
    out.lighting_version = scene.lighting_version();
    Context ctx;
    ctx.c = &scene.compiled();
    ctx.scene = &scene;
    LightingSettings st = settings;
    st.emissive_lights = true;
    ctx.s = &st;
    build_tri_light_map(ctx);
    const auto& instances = scene.instances();
    const auto& meshes = scene.meshes();
    out.vertex.resize(instances.size());
    out.lightmaps.resize(instances.size());
    std::vector<std::vector<Texel>> lm_tasks(instances.size());
    std::vector<std::vector<std::uint8_t>> lm_cover(instances.size());
    std::size_t total = 0;
    for (std::size_t ii = 0; ii < instances.size(); ++ii) {
        const Instance& inst = instances[ii];
        if (!inst.is_static || inst.mesh >= meshes.size()) continue;
        const Mesh& m = meshes[inst.mesh];
        if (o.vertices) {
            out.vertex[ii].resize(m.positions.size());
            total += m.positions.size();
        }
        if (o.lightmaps && m.has_lightmap_uvs()) {
            float area = 0.0f;
            for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
                const Vec3f a = inst.transform.point(m.positions[m.indices[t]]);
                const Vec3f b = inst.transform.point(m.positions[m.indices[t + 1]]);
                const Vec3f c = inst.transform.point(m.positions[m.indices[t + 2]]);
                area += 0.5f * length(cross(b - a, c - a));
            }
            const int size = std::clamp(static_cast<int>(std::ceil(std::sqrt(area) * inst.lightmap_density * 1.5f)), 8, std::max(o.max_lightmap_size, 8));
            Lightmap& lm = out.lightmaps[ii];
            lm.width = lm.height = static_cast<unsigned>(size);
            lm.texels.assign(static_cast<std::size_t>(size) * size, Rgb());
            lm_cover[ii].assign(lm.texels.size(), 0);
            for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
                const std::uint32_t i0 = m.indices[t], i1 = m.indices[t + 1], i2 = m.indices[t + 2];
                const Vec2f u0 = m.lightmap_uvs[i0], u1 = m.lightmap_uvs[i1], u2 = m.lightmap_uvs[i2];
                const float fs = static_cast<float>(size);
                const float minx = std::min({ u0.x, u1.x, u2.x }) * fs, maxx = std::max({ u0.x, u1.x, u2.x }) * fs;
                const float miny = std::min({ u0.y, u1.y, u2.y }) * fs, maxy = std::max({ u0.y, u1.y, u2.y }) * fs;
                const float d = (u1.y - u2.y) * (u0.x - u2.x) + (u2.x - u1.x) * (u0.y - u2.y);
                if (std::fabs(d) < 1e-12f) continue;
                for (int y = std::max(0, static_cast<int>(std::floor(miny))); y <= std::min(size - 1, static_cast<int>(std::ceil(maxy))); ++y) {
                    for (int x = std::max(0, static_cast<int>(std::floor(minx))); x <= std::min(size - 1, static_cast<int>(std::ceil(maxx))); ++x) {
                        const float px = (static_cast<float>(x) + 0.5f) / fs, py = (static_cast<float>(y) + 0.5f) / fs;
                        const float b0 = ((u1.y - u2.y) * (px - u2.x) + (u2.x - u1.x) * (py - u2.y)) / d;
                        const float b1 = ((u2.y - u0.y) * (px - u2.x) + (u0.x - u2.x) * (py - u2.y)) / d;
                        const float b2 = 1.0f - b0 - b1;
                        if (b0 < -1e-4f || b1 < -1e-4f || b2 < -1e-4f) continue;
                        const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(size) + static_cast<std::size_t>(x);
                        if (lm_cover[ii][idx]) continue;
                        Texel tx;
                        tx.p = inst.transform.point(m.positions[i0] * b0 + m.positions[i1] * b1 + m.positions[i2] * b2);
                        Vec3f n;
                        if (m.has_normals()) n = inst.transform.normal(normalize(m.normals[i0] * b0 + m.normals[i1] * b1 + m.normals[i2] * b2));
                        else n = normalize(cross(inst.transform.point(m.positions[i1]) - inst.transform.point(m.positions[i0]), inst.transform.point(m.positions[i2]) - inst.transform.point(m.positions[i0])));
                        tx.n = n;
                        tx.index = static_cast<std::uint32_t>(idx);
                        lm_cover[ii][idx] = 1;
                        lm_tasks[ii].push_back(tx);
                    }
                }
            }
            total += lm_tasks[ii].size();
        }
    }
    std::atomic<std::size_t> done{ 0 };
    const int bounces = std::max(o.bounces, 1);
    auto report = [&](std::size_t add) {
        const std::size_t d = done.fetch_add(add) + add;
        if (progress && total > 0) progress(std::min(0.95f, 0.9f * static_cast<float>(d) / static_cast<float>(total)));
    };
    for (std::size_t ii = 0; ii < instances.size(); ++ii) {
        const Instance& inst = instances[ii];
        if (!inst.is_static || inst.mesh >= meshes.size()) continue;
        const Mesh& m = meshes[inst.mesh];
        if (o.vertices && !out.vertex[ii].empty()) {
            std::vector<Vec3f> normals(m.positions.size());
            if (m.has_normals()) {
                for (std::size_t v = 0; v < m.positions.size(); ++v) normals[v] = inst.transform.normal(m.normals[v]);
            } else {
                Mesh copy = m;
                copy.compute_normals();
                for (std::size_t v = 0; v < m.positions.size(); ++v) normals[v] = inst.transform.normal(copy.normals[v]);
            }
            parallel_for(m.positions.size(), o.threads, [&](std::size_t b, std::size_t e, unsigned) {
                for (std::size_t v = b; v < e; ++v) {
                    Rng rng(hash_combine(o.seed, ii * 1000003 + v), 11);
                    const Vec3f p = inst.transform.point(m.positions[v]) + normals[v] * 2e-3f;
                    out.vertex[ii][v] = irradiance_estimate(ctx, p, normals[v], o.samples, bounces, LightFilter::baked_only, LightFilter::baked_and_stationary, o.include_sky, rng);
                }
                report(e - b);
            });
        }
        if (!lm_tasks[ii].empty()) {
            Lightmap& lm = out.lightmaps[ii];
            const auto& tasks = lm_tasks[ii];
            parallel_for(tasks.size(), o.threads, [&](std::size_t b, std::size_t e, unsigned) {
                for (std::size_t k = b; k < e; ++k) {
                    Rng rng(hash_combine(o.seed ^ 0x5bd1e995ULL, ii * 7919 + k), 13);
                    lm.texels[tasks[k].index] = irradiance_estimate(ctx, tasks[k].p + tasks[k].n * 2e-3f, tasks[k].n, o.samples, bounces, LightFilter::baked_only, LightFilter::baked_and_stationary, o.include_sky, rng);
                }
                report(e - b);
            });
            dilate(lm, lm_cover[ii], std::max(o.dilation, 0));
        }
    }
    if (o.probes) {
        const Compiled& c = scene.compiled();
        out.volume.configure(c.lo.to_double(), c.hi.to_double(), o.probe_spacing);
        const int rays = std::max(o.probe_rays, 16);
        std::vector<Sh9> sh(out.volume.count());
        std::vector<std::uint8_t> live(out.volume.count(), 1);
        parallel_for(out.volume.count(), o.threads, [&](std::size_t b, std::size_t e, unsigned) {
            for (std::size_t i = b; i < e; ++i) {
                Rng rng(hash_combine(o.seed + 77, i), 17);
                const Vec3f origin = out.volume.position_f(i);
                Sh9 s;
                int back = 0;
                const float w = 4.0f * kPi / static_cast<float>(rays);
                for (int r = 0; r < rays; ++r) {
                    const Vec3f dir = sample_uniform_sphere((static_cast<float>(r) + rng.uniform()) / static_cast<float>(rays), rng.uniform());
                    Ray ray{ origin, dir, kInfinity };
                    Hit h;
                    if (intersect(c, ray, h, 0)) {
                        const Surface surf = make_surface(ctx, ray, h);
                        if (!surf.front && !surf.m->is_transmissive()) {
                            ++back;
                            continue;
                        }
                    }
                    s.add(dir, trace_radiance(ctx, origin, dir, bounces, LightFilter::baked_and_stationary, o.include_sky, rng), w);
                }
                const int direct_samples = 16;
                for (const PreparedLight& l : c.lights) {
                    if (l.mobility != LightMobility::baked || l.kind == LightKind::triangle) continue;
                    for (int k = 0; k < direct_samples; ++k) {
                        LightSample ls;
                        if (!sample_light(l, origin, rng.uniform(), rng.uniform(), true, st.use_ies, st.use_cookies, ls)) continue;
                        const float dist = std::isfinite(ls.distance) ? ls.distance * 0.999f : 1e30f;
                        Ray sr{ origin, ls.wi, dist };
                        if (l.shadows && occluded(c, sr)) continue;
                        s.add(ls.wi, ls.radiance, 1.0f / (ls.pdf * static_cast<float>(direct_samples)));
                    }
                }
                sh[i] = s;
                live[i] = back * 4 < rays ? 1 : 0;
            }
        });
        for (std::size_t i = 0; i < sh.size(); ++i) {
            out.volume.probe(i) = sh[i];
            out.volume.set_live(i, live[i] != 0);
        }
        out.volume.count_update();
    }
    if (progress) progress(1.0f);
    return out;
}

} // namespace lighting
} // namespace fizmo
