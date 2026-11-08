#include "scene_internal.hpp"

namespace fizmo {
namespace lighting {
namespace detail {

namespace {

float extent_of(const PreparedLight& l) noexcept {
    switch (l.kind) {
    case LightKind::sphere: return l.radius;
    case LightKind::disk: return l.radius;
    case LightKind::rect: return std::sqrt(l.half_width * l.half_width + l.half_height * l.half_height);
    case LightKind::tube: return l.half_length + l.radius;
    case LightKind::triangle: return std::max(length(l.e1), length(l.e2));
    default: return 0.0f;
    }
}

float attenuation(const PreparedLight& l, float d) noexcept {
    d = std::max(d, 0.01f);
    float window = 1.0f;
    if (l.range > 0.0f) {
        const float q = d / l.range;
        const float q4 = q * q * q * q;
        window = clamp01(1.0f - q4);
        window *= window;
    }
    switch (l.falloff) {
    case Falloff::physical: return window;
    case Falloff::custom: return window * std::pow(d, 2.0f - l.exponent);
    case Falloff::linear:
        if (l.range <= 0.0f) return 1.0f;
        return std::max(0.0f, 1.0f - d / l.range) * d * d;
    case Falloff::smooth: {
        if (l.range <= 0.0f) return 1.0f;
        const float q = clamp01(1.0f - (d / l.range) * (d / l.range));
        return q * q * d * d;
    }
    case Falloff::constant:
        if (l.range > 0.0f && d > l.range) return 0.0f;
        return d * d;
    }
    return window;
}

bool hit_sphere(const Vec3f& c, float r, const Ray& ray, float& t) noexcept {
    const Vec3f oc = ray.o - c;
    const float b = dot(oc, ray.d);
    const float cc = dot(oc, oc) - r * r;
    const float h = b * b - cc;
    if (h < 0.0f) return false;
    const float sq = std::sqrt(h);
    float tt = -b - sq;
    if (tt <= 1e-5f) tt = -b + sq;
    if (tt <= 1e-5f || tt >= ray.tmax) return false;
    t = tt;
    return true;
}

bool hit_capsule(const Vec3f& pa, const Vec3f& pb, float r, const Ray& ray, float& t, Vec3f& normal) noexcept {
    const Vec3f ba = pb - pa;
    const Vec3f oa = ray.o - pa;
    const float baba = dot(ba, ba);
    const float bard = dot(ba, ray.d);
    const float baoa = dot(ba, oa);
    const float rdoa = dot(ray.d, oa);
    const float oaoa = dot(oa, oa);
    const float a = baba - bard * bard;
    float b = baba * rdoa - baoa * bard;
    float c = baba * oaoa - baoa * baoa - r * r * baba;
    float h = b * b - a * c;
    float best = kInfinity;
    if (h >= 0.0f && a > 1e-12f) {
        const float tt = (-b - std::sqrt(h)) / a;
        const float y = baoa + tt * bard;
        if (y > 0.0f && y < baba && tt > 1e-5f && tt < ray.tmax) {
            best = tt;
            const Vec3f p = ray.o + ray.d * tt;
            normal = normalize(p - (pa + ba * (y / baba)));
        }
    }
    float ts;
    if (hit_sphere(pa, r, ray, ts) && ts < best) {
        const Vec3f p = ray.o + ray.d * ts;
        if (dot(p - pa, ba) <= 0.0f || baba <= 1e-12f) {
            best = ts;
            normal = normalize(p - pa);
        }
    }
    if (hit_sphere(pb, r, ray, ts) && ts < best) {
        const Vec3f p = ray.o + ray.d * ts;
        if (dot(p - pb, ba) >= 0.0f) {
            best = ts;
            normal = normalize(p - pb);
        }
    }
    if (best == kInfinity) return false;
    t = best;
    return true;
}

} // namespace

bool light_reaches(const PreparedLight& l, const Vec3f& p) noexcept {
    if (l.kind == LightKind::directional || l.range <= 0.0f) return true;
    const float reach = l.range + extent_of(l);
    return length_squared(p - l.p) <= reach * reach;
}

Rgb light_factor(const PreparedLight& l, const Vec3f& p, bool use_ies, bool use_cookie) {
    if (l.kind == LightKind::directional) {
        if (use_cookie && l.cookie) {
            const float u = dot(p, l.t) / l.cookie_size, v = dot(p, l.b) / l.cookie_size;
            return l.cookie->sample(u, v);
        }
        return Rgb(1.0f);
    }
    if (l.kind == LightKind::triangle) return Rgb(1.0f);
    const Vec3f d = p - l.p;
    const float dist = length(d);
    const Vec3f from = dist > 0.0f ? d / dist : l.n;
    float f = attenuation(l, dist);
    if (f <= 0.0f) return Rgb();
    if (l.cone) {
        const float c = dot(from, l.n);
        f *= smoothstep(l.cos_outer, l.cos_inner, c);
        if (f <= 0.0f) return Rgb();
    }
    if (use_ies && l.ies) {
        const float vertical = std::acos(std::clamp(dot(from, l.n), -1.0f, 1.0f)) / kDegToRad;
        float horizontal = std::atan2(dot(from, l.b), dot(from, l.t)) / kDegToRad;
        if (horizontal < 0.0f) horizontal += 360.0f;
        f *= l.ies->normalized(vertical, horizontal);
    }
    Rgb out(f);
    if (use_cookie && l.cookie) {
        const float z = dot(from, l.n);
        if (z <= 0.0f) return Rgb();
        const float x = dot(from, l.t) / z, y = dot(from, l.b) / z;
        out *= l.cookie->sample(0.5f + 0.5f * x / l.tan_outer, 0.5f + 0.5f * y / l.tan_outer);
    }
    return out;
}

bool sample_light(const PreparedLight& l, const Vec3f& p, float u1, float u2, bool area, bool use_ies, bool use_cookie, LightSample& s) {
    s.delta = false;
    s.pdf = 1.0f;
    if (l.kind == LightKind::directional) {
        if (area && l.cos_sun < 1.0f && l.sun_solid_angle > 0.0f) {
            s.wi = normalize(Frame(-l.n).to_world(sample_uniform_cone(u1, u2, l.cos_sun)));
            s.pdf = 1.0f / l.sun_solid_angle;
            s.radiance = l.emit / l.sun_solid_angle;
        } else {
            s.wi = -l.n;
            s.delta = true;
            s.radiance = l.emit;
        }
        s.distance = kInfinity;
        s.point = p + s.wi * 1e6f;
        s.normal = l.n;
        s.radiance *= light_factor(l, p, use_ies, use_cookie);
        return !s.radiance.is_black();
    }
    if (!area || l.kind == LightKind::point) {
        const Vec3f d = l.p - p;
        const float d2 = std::max(length_squared(d), 1e-8f);
        const float dist = std::sqrt(d2);
        s.wi = d / dist;
        s.distance = dist;
        s.point = l.p;
        s.normal = -s.wi;
        s.delta = true;
        Rgb intensity = l.emit;
        switch (l.kind) {
        case LightKind::sphere: intensity = l.emit * (kPi * l.radius * l.radius); break;
        case LightKind::disk:
        case LightKind::rect:
        case LightKind::triangle: {
            const float c = dot(-s.wi, l.n);
            const float cc = l.two_sided ? std::fabs(c) : std::max(c, 0.0f);
            intensity = l.emit * (l.area * cc);
            break;
        }
        case LightKind::tube: intensity = l.emit * (2.0f * l.radius * 2.0f * l.half_length + kPi * l.radius * l.radius); break;
        default: break;
        }
        s.radiance = intensity * light_factor(l, p, use_ies, use_cookie) / d2;
        return !s.radiance.is_black();
    }
    Vec3f q, nq;
    switch (l.kind) {
    case LightKind::sphere: {
        const Vec3f dc = l.p - p;
        const float dc2 = length_squared(dc);
        const float r2 = l.radius * l.radius;
        if (dc2 <= r2 * 1.0001f) {
            nq = sample_uniform_sphere(u1, u2);
            q = l.p + nq * l.radius;
            break;
        }
        const float dist_c = std::sqrt(dc2);
        const float cos_max = std::sqrt(std::max(0.0f, 1.0f - r2 / dc2));
        const Vec3f axis = dc / dist_c;
        s.wi = normalize(Frame(axis).to_world(sample_uniform_cone(u1, u2, cos_max)));
        const float proj = dot(dc, s.wi);
        const float disc = std::max(0.0f, r2 - (dc2 - proj * proj));
        s.distance = std::max(proj - std::sqrt(disc), 1e-5f);
        s.point = p + s.wi * s.distance;
        s.normal = normalize(s.point - l.p);
        s.pdf = 1.0f / (kTwoPi * std::max(1.0f - cos_max, 1e-7f));
        s.radiance = l.emit * light_factor(l, p, use_ies, use_cookie);
        return !s.radiance.is_black();
    }
    case LightKind::disk: {
        float x, y;
        sample_uniform_disk(u1, u2, x, y);
        q = l.p + l.t * (x * l.radius) + l.b * (y * l.radius);
        nq = l.n;
        break;
    }
    case LightKind::rect:
        q = l.p + l.t * ((2.0f * u1 - 1.0f) * l.half_width) + l.b * ((2.0f * u2 - 1.0f) * l.half_height);
        nq = l.n;
        break;
    case LightKind::tube: {
        const float pc = l.area > 0.0f ? l.cyl_area / l.area : 0.0f;
        if (u1 < pc) {
            const float a = u1 / pc;
            const float phi = kTwoPi * u2;
            const Vec3f radial = l.t * std::cos(phi) + l.b * std::sin(phi);
            q = l.p + l.n * ((2.0f * a - 1.0f) * l.half_length) + radial * l.radius;
            nq = radial;
        } else {
            const float a = pc < 1.0f ? (u1 - pc) / (1.0f - pc) : 0.5f;
            const Vec3f w = sample_uniform_sphere(a, u2);
            const Vec3f world = l.t * w.x + l.b * w.y + l.n * w.z;
            const Vec3f end = l.p + l.n * (w.z >= 0.0f ? l.half_length : -l.half_length);
            q = end + world * l.radius;
            nq = world;
        }
        break;
    }
    case LightKind::triangle: {
        const float su = std::sqrt(u1);
        q = l.p + l.e1 * (su * (1.0f - u2)) + l.e2 * (su * u2);
        nq = l.n;
        break;
    }
    default: return false;
    }
    const Vec3f d = q - p;
    const float d2 = length_squared(d);
    if (d2 <= 1e-10f) return false;
    const float dist = std::sqrt(d2);
    s.wi = d / dist;
    float cl = dot(-s.wi, nq);
    const bool one_sided = !(l.two_sided && (l.kind == LightKind::disk || l.kind == LightKind::rect || l.kind == LightKind::triangle));
    if (one_sided && cl <= 0.0f) return false;
    cl = std::fabs(cl);
    if (cl <= 1e-6f) return false;
    s.distance = dist;
    s.point = q;
    s.normal = nq;
    s.pdf = d2 / (cl * l.area);
    s.radiance = l.emit * light_factor(l, p, use_ies, use_cookie);
    return !s.radiance.is_black();
}

bool intersect_light(const PreparedLight& l, const Ray& ray, float& t, Vec3f& point, Vec3f& normal) {
    switch (l.kind) {
    case LightKind::sphere: {
        if (!hit_sphere(l.p, l.radius, ray, t)) return false;
        point = ray.o + ray.d * t;
        normal = normalize(point - l.p);
        return true;
    }
    case LightKind::disk:
    case LightKind::rect: {
        const float denom = dot(ray.d, l.n);
        if (std::fabs(denom) < 1e-8f) return false;
        const float tt = dot(l.p - ray.o, l.n) / denom;
        if (tt <= 1e-5f || tt >= ray.tmax) return false;
        const Vec3f q = ray.o + ray.d * tt;
        const Vec3f rel = q - l.p;
        const float x = dot(rel, l.t), y = dot(rel, l.b);
        if (l.kind == LightKind::disk) {
            if (x * x + y * y > l.radius * l.radius) return false;
        } else if (std::fabs(x) > l.half_width || std::fabs(y) > l.half_height) {
            return false;
        }
        t = tt;
        point = q;
        normal = l.n;
        return true;
    }
    case LightKind::tube: {
        const Vec3f a = l.p - l.n * l.half_length, b = l.p + l.n * l.half_length;
        if (!hit_capsule(a, b, l.radius, ray, t, normal)) return false;
        point = ray.o + ray.d * t;
        return true;
    }
    default: return false;
    }
}

float light_pdf(const PreparedLight& l, const Vec3f& p, const Vec3f& wi, float t, const Vec3f& point, const Vec3f& normal) {
    (void)point;
    switch (l.kind) {
    case LightKind::sphere: {
        const float dc2 = length_squared(l.p - p);
        const float r2 = l.radius * l.radius;
        if (dc2 <= r2 * 1.0001f) {
            const float c = std::fabs(dot(-wi, normal));
            return c > 0.0f ? t * t / (c * l.area) : 0.0f;
        }
        const float cos_max = std::sqrt(std::max(0.0f, 1.0f - r2 / dc2));
        return 1.0f / (kTwoPi * std::max(1.0f - cos_max, 1e-7f));
    }
    case LightKind::disk:
    case LightKind::rect:
    case LightKind::triangle:
    case LightKind::tube: {
        float c = dot(-wi, normal);
        const bool one_sided = !(l.two_sided && l.kind != LightKind::tube);
        if (one_sided && c <= 0.0f) return 0.0f;
        c = std::fabs(c);
        return c > 1e-6f ? t * t / (c * l.area) : 0.0f;
    }
    default: return 0.0f;
    }
}

Rgb light_hit_radiance(const PreparedLight& l, const Vec3f& point, const Vec3f& normal, const Vec3f& viewer, bool use_ies, bool use_cookie) {
    const Vec3f to_viewer = normalize(viewer - point);
    const float c = dot(to_viewer, normal);
    const bool one_sided = !(l.two_sided && l.kind != LightKind::tube && l.kind != LightKind::sphere);
    if (one_sided && c <= 0.0f) return Rgb();
    return l.emit * light_factor(l, viewer, use_ies, use_cookie);
}

float light_importance(const PreparedLight& l, const Vec3f& p, const Vec3f& n) noexcept {
    if (l.kind == LightKind::directional) return std::max(l.emit.luminance(), 1e-8f) * std::max(dot(n, -l.n), 0.05f);
    const Vec3f d = l.p - p;
    const float ext = extent_of(l);
    const float d2 = std::max(length_squared(d), ext * ext + 1e-4f);
    if (l.range > 0.0f && d2 > (l.range + ext) * (l.range + ext)) return 0.0f;
    const float dist = std::sqrt(d2);
    const float facing = dot(n, d / dist);
    const float slack = ext / dist;
    const float cosine = std::clamp(facing + slack, 0.05f, 1.0f);
    return l.power * cosine / (4.0f * kPi * d2);
}

} // namespace detail
} // namespace lighting
} // namespace fizmo
