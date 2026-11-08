#include "photons.hpp"

#include <algorithm>
#include <mutex>

namespace fizmo {
namespace lighting {
namespace detail {

namespace {

struct PhotonState {
    Vec3f o;
    Vec3f d;
    Rgb power;
    Wave wave;
    bool split = false;
    bool specular = false;
    float wave_weight = 1.0f;
    int depth = 0;
    const Material* medium = nullptr;
};

bool caustic_caster(const Material& m) noexcept {
    if (m.is_transmissive()) return true;
    return m.metallic > 0.5f && m.roughness < 0.15f;
}

Rgb absorption(const Material& m, float t) noexcept {
    if (m.absorption_distance <= 0.0f) return Rgb(1.0f);
    Rgb out;
    for (int i = 0; i < 3; ++i) {
        const float c = std::clamp(m.absorption_color[i], 1e-4f, 1.0f);
        out[i] = std::exp(std::log(c) / m.absorption_distance * t);
    }
    return out;
}

bool emit_photon(const Context& ctx, Rng& rng, PhotonState& st, LightFilter filter) {
    const Compiled& c = *ctx.c;
    if (c.total_power <= 0.0f || c.lights.empty()) return false;
    const float target = rng.uniform() * c.total_power;
    std::size_t idx = static_cast<std::size_t>(std::upper_bound(c.light_cdf.begin(), c.light_cdf.end(), target) - c.light_cdf.begin());
    idx = std::min(std::max<std::size_t>(idx, 1), c.lights.size()) - 1;
    const PreparedLight& l = c.lights[idx];
    if (!light_allowed(l, filter)) return false;
    const float p_light = l.power / c.total_power;
    if (p_light <= 0.0f) return false;
    Vec3f q, n, d;
    Rgb phi;
    const float u1 = rng.uniform(), u2 = rng.uniform(), u3 = rng.uniform(), u4 = rng.uniform();
    auto cosine_out = [&](const Vec3f& normal, bool both, float area) {
        Vec3f nn = normal;
        float scale = 1.0f;
        if (both) {
            if (u4 < 0.5f) nn = -nn;
            scale = 2.0f;
        }
        d = normalize(Frame(nn).to_world(sample_cosine_hemisphere(u3, rng.uniform())));
        phi = l.emit * (kPi * area * scale);
        n = nn;
    };
    switch (l.kind) {
    case LightKind::point: {
        q = l.p;
        if (l.cone) {
            d = normalize(Frame(l.n).to_world(sample_uniform_cone(u1, u2, l.cos_outer)));
            phi = l.emit * (kTwoPi * (1.0f - l.cos_outer));
        } else {
            d = sample_uniform_sphere(u1, u2);
            phi = l.emit * (4.0f * kPi);
        }
        n = d;
        break;
    }
    case LightKind::directional: {
        float x, y;
        sample_uniform_disk(u1, u2, x, y);
        const float r = c.radius * 1.05f;
        q = c.center - l.n * (r * 2.0f) + (l.t * x + l.b * y) * r;
        d = l.n;
        if (l.cos_sun < 1.0f) d = normalize(Frame(l.n).to_world(sample_uniform_cone(u3, u4, l.cos_sun)));
        phi = l.emit * (kPi * r * r);
        n = d;
        break;
    }
    case LightKind::sphere: {
        const Vec3f w = sample_uniform_sphere(u1, u2);
        q = l.p + w * l.radius;
        cosine_out(w, false, l.area);
        break;
    }
    case LightKind::disk: {
        float x, y;
        sample_uniform_disk(u1, u2, x, y);
        q = l.p + l.t * (x * l.radius) + l.b * (y * l.radius);
        cosine_out(l.n, l.two_sided, l.area);
        break;
    }
    case LightKind::rect:
        q = l.p + l.t * ((2.0f * u1 - 1.0f) * l.half_width) + l.b * ((2.0f * u2 - 1.0f) * l.half_height);
        cosine_out(l.n, l.two_sided, l.area);
        break;
    case LightKind::tube: {
        const float pc = l.area > 0.0f ? l.cyl_area / l.area : 0.0f;
        Vec3f nq;
        if (u1 < pc) {
            const float phi_a = kTwoPi * u2;
            const Vec3f radial = l.t * std::cos(phi_a) + l.b * std::sin(phi_a);
            q = l.p + l.n * ((2.0f * (u1 / pc) - 1.0f) * l.half_length) + radial * l.radius;
            nq = radial;
        } else {
            const Vec3f w = sample_uniform_sphere(pc < 1.0f ? (u1 - pc) / (1.0f - pc) : 0.5f, u2);
            const Vec3f world = l.t * w.x + l.b * w.y + l.n * w.z;
            q = l.p + l.n * (w.z >= 0.0f ? l.half_length : -l.half_length) + world * l.radius;
            nq = world;
        }
        cosine_out(nq, false, l.area);
        break;
    }
    case LightKind::triangle: {
        const float su = std::sqrt(u1);
        q = l.p + l.e1 * (su * (1.0f - u2)) + l.e2 * (su * u2);
        cosine_out(l.n, l.two_sided, l.area);
        break;
    }
    }
    if (l.kind != LightKind::directional && l.kind != LightKind::triangle) phi *= light_factor(l, l.p + d, ctx.s->use_ies, ctx.s->use_cookies);
    else if (l.kind == LightKind::directional) phi *= light_factor(l, q, ctx.s->use_ies, ctx.s->use_cookies);
    st = PhotonState();
    st.o = q + n * 1e-4f;
    st.d = d;
    st.power = phi / p_light;
    return !st.power.is_black();
}

} // namespace

std::uint64_t PhotonMap::key(int x, int y, int z) const noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x) & 0x1FFFFFu) << 42) | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y) & 0x1FFFFFu) << 21) | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(z) & 0x1FFFFFu));
}

void PhotonMap::clear() {
    m_photons.clear();
    m_cells.clear();
}

void PhotonMap::build(const Context& ctx, const PhotonOptions& o) {
    clear();
    m_radius = std::max(o.radius, 1e-4f);
    m_inv_cell = 1.0f / (m_radius * 2.0f);
    if (o.count == 0 || ctx.c->lights.empty()) return;
    const unsigned workers = thread_count(o.threads);
    std::vector<std::vector<Photon>> local(workers);
    const std::size_t emitted = o.count;
    const int bundle = std::max(o.bundle, 1);
    parallel_for(emitted, workers, [&](std::size_t begin, std::size_t end, unsigned worker) {
        std::vector<Photon>& out = local[worker];
        std::vector<PhotonState> stack;
        for (std::size_t i = begin; i < end; ++i) {
            Rng rng(hash_combine(o.seed, i), (o.seed << 1) | 1u);
            PhotonState st;
            if (!emit_photon(ctx, rng, st, o.filter)) continue;
            st.power *= 1.0f / static_cast<float>(emitted);
            const float start_max = st.power.max_component();
            stack.clear();
            stack.push_back(st);
            while (!stack.empty()) {
                PhotonState s = stack.back();
                stack.pop_back();
                while (s.depth < o.max_bounces) {
                    Ray ray{ s.o, s.d, kInfinity };
                    Hit h;
                    if (!intersect(*ctx.c, ray, h, 0)) break;
                    const Rgb before = s.power;
                    if (s.medium) s.power *= absorption(*s.medium, h.t);
                    const Surface surf = make_surface(ctx, ray, h);
                    const Material& m = *surf.m;
                    if (!caustic_caster(m)) {
                        if (s.specular && surf.front) {
                            Rgb p = s.power;
                            if (s.split && s.wave.spectral) p = spectral::wavelength_weight(s.wave.nm) * (s.power.r * s.wave_weight);
                            out.push_back(Photon{ surf.p, -s.d, surf.ns, p });
                        }
                        break;
                    }
                    if (!s.split && m.is_dispersive() && o.dispersion) {
                        if (o.spectral) {
                            const float jitter = rng.uniform();
                            for (int k = 0; k < bundle; ++k) {
                                float pdf = 1.0f;
                                const float nm = spectral::sample_wavelength((static_cast<float>(k) + jitter) / static_cast<float>(bundle), pdf);
                                PhotonState c = s;
                                c.split = true;
                                c.wave.spectral = true;
                                c.wave.nm = nm;
                                c.wave_weight = 1.0f / (pdf * static_cast<float>(bundle));
                                c.power = Rgb(spectral::uplift(before, nm));
                                stack.push_back(c);
                            }
                        } else {
                            for (int k = 0; k < 3; ++k) {
                                PhotonState c = s;
                                c.split = true;
                                c.wave.channel = k;
                                c.power = Rgb();
                                c.power[k] = before[k];
                                if (!c.power.is_black()) stack.push_back(c);
                            }
                        }
                        s.depth = o.max_bounces;
                        break;
                    }
                    const Bsdf b = surface_bsdf(ctx, surf, s.wave, o.dispersion);
                    const Frame f(surf.ns);
                    BsdfSample bs;
                    if (!bsdf_sample(b, f.to_local(surf.wo), rng.uniform(), rng.uniform(), rng.uniform(), rng.uniform(), bs)) break;
                    s.power *= bs.weight;
                    if (s.power.max_component() <= 0.0f) break;
                    s.specular = s.specular || bs.specular || m.is_transmissive();
                    const Vec3f nd = normalize(f.to_world(bs.wi));
                    if (bs.transmitted && !m.thin_walled) s.medium = surf.front ? &m : nullptr;
                    s.o = offset_ray(surf, nd);
                    s.d = nd;
                    ++s.depth;
                    if (s.depth >= 4) {
                        const float ratio = s.power.max_component() / std::max(start_max, 1e-30f);
                        const float q = std::clamp(1.0f - ratio, 0.1f, 0.95f);
                        if (rng.uniform() < q) break;
                        s.power *= 1.0f / (1.0f - q);
                    }
                }
            }
        }
    });
    std::size_t total = 0;
    for (const auto& v : local) total += v.size();
    m_photons.reserve(total);
    for (auto& v : local) m_photons.insert(m_photons.end(), v.begin(), v.end());
    std::vector<std::pair<std::uint64_t, std::uint32_t>> keys(m_photons.size());
    for (std::size_t i = 0; i < m_photons.size(); ++i) {
        const Vec3f& p = m_photons[i].p;
        keys[i] = { key(static_cast<int>(std::floor(p.x * m_inv_cell)), static_cast<int>(std::floor(p.y * m_inv_cell)), static_cast<int>(std::floor(p.z * m_inv_cell))), static_cast<std::uint32_t>(i) };
    }
    std::sort(keys.begin(), keys.end());
    std::vector<Photon> sorted(m_photons.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        sorted[i] = m_photons[keys[i].second];
        auto it = m_cells.find(keys[i].first);
        if (it == m_cells.end()) m_cells.emplace(keys[i].first, std::make_pair(static_cast<std::uint32_t>(i), 1u));
        else ++it->second.second;
    }
    m_photons.swap(sorted);
}

Rgb PhotonMap::gather(const Surface& s, const Bsdf& b, const Frame& frame, const Vec3f& wo) const {
    if (m_photons.empty()) return Rgb();
    const float r2 = m_radius * m_radius;
    const int x0 = static_cast<int>(std::floor((s.p.x - m_radius) * m_inv_cell)), x1 = static_cast<int>(std::floor((s.p.x + m_radius) * m_inv_cell));
    const int y0 = static_cast<int>(std::floor((s.p.y - m_radius) * m_inv_cell)), y1 = static_cast<int>(std::floor((s.p.y + m_radius) * m_inv_cell));
    const int z0 = static_cast<int>(std::floor((s.p.z - m_radius) * m_inv_cell)), z1 = static_cast<int>(std::floor((s.p.z + m_radius) * m_inv_cell));
    Rgb sum;
    for (int z = z0; z <= z1; ++z) {
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const auto it = m_cells.find(key(x, y, z));
                if (it == m_cells.end()) continue;
                for (std::uint32_t i = it->second.first; i < it->second.first + it->second.second; ++i) {
                    const Photon& ph = m_photons[i];
                    const Vec3f d = ph.p - s.p;
                    const float dd = length_squared(d);
                    if (dd > r2) continue;
                    if (dot(ph.n, s.ns) < 0.6f) continue;
                    if (std::fabs(dot(d, s.ns)) > m_radius * 0.25f) continue;
                    const Vec3f wi = frame.to_local(ph.wi);
                    if (wi.z <= 0.0f) continue;
                    const float k = 1.0f - std::sqrt(dd / r2);
                    sum += bsdf_eval(b, wo, wi).total() * ph.power * (3.0f * k);
                }
            }
        }
    }
    return sum / (kPi * r2);
}

} // namespace detail
} // namespace lighting
} // namespace fizmo
