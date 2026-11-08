#ifndef FIZMO_LIGHTING_BSDF_HPP
#define FIZMO_LIGHTING_BSDF_HPP

#include "material.hpp"
#include "math.hpp"

namespace fizmo {
namespace lighting {
namespace detail {

struct Bsdf {
    Rgb diffuse;
    Rgb f0;
    Rgb tint{ 1.0f, 1.0f, 1.0f };
    float alpha = 0.25f;
    bool delta_spec = false;
    float transmission = 0.0f;
    float eta = 1.0f / 1.5f;
    bool thin = false;
    float p_diffuse = 1.0f;
    float p_spec = 0.0f;
    float p_trans = 0.0f;
    bool diffuse_only = false;

    bool has_smooth_lobes() const noexcept { return p_diffuse > 0.0f || (p_spec > 0.0f && !delta_spec); }
    bool is_specular_only() const noexcept { return !has_smooth_lobes(); }
    float roughness() const noexcept { return std::sqrt(alpha); }
};

struct BsdfEval {
    Rgb diffuse;
    Rgb specular;
    Rgb total() const noexcept { return diffuse + specular; }
};

struct BsdfSample {
    Vec3f wi;
    Rgb weight;
    float pdf = 0.0f;
    bool specular = false;
    bool transmitted = false;
    bool dispersive = false;
};

inline float schlick_weight(float c) noexcept {
    const float m = clamp01(1.0f - c);
    const float m2 = m * m;
    return m2 * m2 * m;
}

inline Rgb schlick(const Rgb& f0, float c) noexcept {
    const float w = schlick_weight(c);
    return f0 + (Rgb(1.0f) - f0) * w;
}

inline float fresnel_dielectric(float cos_i, float eta) noexcept {
    cos_i = clamp01(cos_i);
    const float sin2_t = eta * eta * (1.0f - cos_i * cos_i);
    if (sin2_t >= 1.0f) return 1.0f;
    const float cos_t = std::sqrt(1.0f - sin2_t);
    const float rs = (eta * cos_i - cos_t) / (eta * cos_i + cos_t);
    const float rp = (cos_i - eta * cos_t) / (cos_i + eta * cos_t);
    return 0.5f * (rs * rs + rp * rp);
}

inline bool refract(const Vec3f& wo, const Vec3f& n, float eta, Vec3f& wt) noexcept {
    const float cos_i = dot(wo, n);
    const float sin2_t = eta * eta * std::max(0.0f, 1.0f - cos_i * cos_i);
    if (sin2_t >= 1.0f) return false;
    const float cos_t = std::sqrt(1.0f - sin2_t);
    wt = normalize(-wo * eta + n * (eta * cos_i - cos_t));
    return true;
}

inline float ggx_d(const Vec3f& h, float a) noexcept {
    const float a2 = a * a;
    const float c = h.z * h.z * (a2 - 1.0f) + 1.0f;
    return a2 / (kPi * c * c);
}

inline float ggx_lambda(const Vec3f& w, float a) noexcept {
    const float z2 = w.z * w.z;
    if (z2 <= 0.0f) return 0.0f;
    const float t2 = std::max(0.0f, 1.0f - z2) / z2;
    return 0.5f * (std::sqrt(1.0f + a * a * t2) - 1.0f);
}

inline float ggx_g1(const Vec3f& w, float a) noexcept { return 1.0f / (1.0f + ggx_lambda(w, a)); }

inline float ggx_g2(const Vec3f& wo, const Vec3f& wi, float a) noexcept { return 1.0f / (1.0f + ggx_lambda(wo, a) + ggx_lambda(wi, a)); }

inline Vec3f ggx_sample_vndf(const Vec3f& v, float a, float u1, float u2) noexcept {
    const Vec3f vh = normalize(Vec3f{ a * v.x, a * v.y, v.z });
    const float lensq = vh.x * vh.x + vh.y * vh.y;
    const Vec3f t1 = lensq > 0.0f ? Vec3f{ -vh.y, vh.x, 0.0f } / std::sqrt(lensq) : Vec3f{ 1.0f, 0.0f, 0.0f };
    const Vec3f t2 = cross(vh, t1);
    const float r = std::sqrt(u1);
    const float phi = kTwoPi * u2;
    const float p1 = r * std::cos(phi);
    float p2 = r * std::sin(phi);
    const float s = 0.5f * (1.0f + vh.z);
    p2 = (1.0f - s) * std::sqrt(std::max(0.0f, 1.0f - p1 * p1)) + s * p2;
    const Vec3f nh = t1 * p1 + t2 * p2 + vh * std::sqrt(std::max(0.0f, 1.0f - p1 * p1 - p2 * p2));
    return normalize(Vec3f{ a * nh.x, a * nh.y, std::max(1e-6f, nh.z) });
}

inline Bsdf make_bsdf(const Material& m, const Rgb& base, float cos_o, bool front, float ior) noexcept {
    Bsdf b;
    const float metal = clamp01(m.metallic);
    const float trans = m.transmission > 0.0f ? clamp01(m.transmission) * (1.0f - metal) : 0.0f;
    const float rough = clamp01(m.roughness);
    b.alpha = std::max(rough * rough, 1e-3f);
    b.delta_spec = rough < 0.02f;
    const float r0 = (ior - 1.0f) / (ior + 1.0f);
    const float dielectric = std::min(r0 * r0 * 2.0f * clamp01(m.specular), 1.0f);
    b.f0 = Rgb(dielectric) * (1.0f - metal) + base * metal;
    b.diffuse = base * ((1.0f - metal) * (1.0f - trans));
    b.tint = base;
    b.transmission = trans;
    b.thin = m.thin_walled;
    b.eta = front ? 1.0f / ior : ior;
    if (m.thin_walled) b.eta = 1.0f / ior;
    const float fsel = schlick(b.f0, cos_o).luminance();
    const float wd = b.diffuse.luminance() * (1.0f - fsel);
    const float ws = (1.0f - trans) * fsel;
    const float opaque = wd + ws;
    b.p_trans = trans;
    if (opaque > 0.0f) {
        b.p_diffuse = (1.0f - trans) * wd / opaque;
        b.p_spec = (1.0f - trans) * ws / opaque;
    } else {
        b.p_diffuse = 0.0f;
        b.p_spec = 1.0f - trans;
    }
    return b;
}

inline BsdfEval bsdf_eval(const Bsdf& b, const Vec3f& wo, const Vec3f& wi) noexcept {
    BsdfEval e;
    if (wo.z <= 0.0f || wi.z <= 0.0f) return e;
    const float keep = 1.0f - schlick(b.f0, wo.z).luminance();
    e.diffuse = b.diffuse * (keep * kInvPi);
    if (!b.delta_spec && !b.diffuse_only) {
        const Vec3f h = normalize(wo + wi);
        const Rgb f = schlick(b.f0, dot(wi, h));
        const float d = ggx_d(h, b.alpha);
        const float g = ggx_g2(wo, wi, b.alpha);
        e.specular = f * ((1.0f - b.transmission) * d * g / (4.0f * wo.z * wi.z));
    }
    return e;
}

inline float bsdf_pdf(const Bsdf& b, const Vec3f& wo, const Vec3f& wi) noexcept {
    if (wo.z <= 0.0f || wi.z <= 0.0f) return 0.0f;
    float pdf = b.p_diffuse * wi.z * kInvPi;
    if (!b.delta_spec && b.p_spec > 0.0f) {
        const Vec3f h = normalize(wo + wi);
        pdf += b.p_spec * ggx_g1(wo, b.alpha) * ggx_d(h, b.alpha) / (4.0f * wo.z);
    }
    return pdf;
}

inline bool bsdf_sample(const Bsdf& b, const Vec3f& wo, float u1, float u2, float u3, float u4, BsdfSample& s) noexcept {
    s = BsdfSample();
    if (wo.z <= 0.0f) return false;
    if (u3 < b.p_trans) {
        s.specular = true;
        if (b.thin) {
            const float f = fresnel_dielectric(wo.z, b.eta);
            const float fr = f < 1.0f ? 2.0f * f / (1.0f + f) : 1.0f;
            if (u1 < fr) {
                s.wi = { -wo.x, -wo.y, wo.z };
                s.weight = Rgb(1.0f);
            } else {
                s.wi = -wo;
                s.weight = b.tint;
                s.transmitted = true;
            }
            s.pdf = 1.0f;
            return true;
        }
        Vec3f m{ 0.0f, 0.0f, 1.0f };
        if (!b.delta_spec) m = ggx_sample_vndf(wo, b.alpha, u1, u2);
        const float cos_om = dot(wo, m);
        if (cos_om <= 0.0f) return false;
        const float f = fresnel_dielectric(cos_om, b.eta);
        const float pick = b.delta_spec ? u1 : u4;
        if (pick < f) {
            s.wi = m * (2.0f * cos_om) - wo;
            if (s.wi.z <= 0.0f) return false;
            s.weight = Rgb(1.0f);
        } else {
            if (!refract(wo, m, b.eta, s.wi)) return false;
            if (s.wi.z >= 0.0f) return false;
            s.weight = b.tint;
            s.transmitted = true;
        }
        if (!b.delta_spec) s.weight *= ggx_g1(Vec3f{ s.wi.x, s.wi.y, std::fabs(s.wi.z) }, b.alpha);
        s.pdf = 1.0f;
        s.dispersive = s.transmitted;
        return true;
    }
    if (u3 < b.p_trans + b.p_spec) {
        if (b.delta_spec) {
            s.wi = { -wo.x, -wo.y, wo.z };
            s.weight = schlick(b.f0, wo.z) * ((1.0f - b.transmission) / std::max(b.p_spec, 1e-6f));
            s.pdf = 1.0f;
            s.specular = true;
            return true;
        }
        const Vec3f m = ggx_sample_vndf(wo, b.alpha, u1, u2);
        s.wi = m * (2.0f * dot(wo, m)) - wo;
        if (s.wi.z <= 0.0f) return false;
    } else {
        if (b.p_diffuse <= 0.0f) return false;
        s.wi = sample_cosine_hemisphere(u1, u2);
        if (s.wi.z <= 0.0f) return false;
    }
    s.pdf = bsdf_pdf(b, wo, s.wi);
    if (s.pdf <= 0.0f) return false;
    s.weight = bsdf_eval(b, wo, s.wi).total() * (s.wi.z / s.pdf);
    return true;
}

inline Rgb env_brdf_approx(const Rgb& f0, float roughness, float n_dot_v) noexcept {
    const float c0[4] = { -1.0f, -0.0275f, -0.572f, 0.022f };
    const float c1[4] = { 1.0f, 0.0425f, 1.04f, -0.04f };
    float r[4];
    for (int i = 0; i < 4; ++i) r[i] = roughness * c0[i] + c1[i];
    const float a004 = std::min(r[0] * r[0], std::exp2(-9.28f * n_dot_v)) * r[0] + r[1];
    const float a = a004 * -1.04f + r[2];
    const float b = a004 * 1.04f + r[3];
    return f0 * a + Rgb(b);
}

} // namespace detail
} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_BSDF_HPP
