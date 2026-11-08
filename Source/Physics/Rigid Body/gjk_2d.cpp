#include "fizmo_library.hpp"
#include "gjk_2d.hpp"

namespace fizmo {
namespace physics {

namespace {

using V2 = RVec2;

Real dot2(const V2& a, const V2& b) noexcept { return a.x * b.x + a.y * b.y; }
Real cross2(const V2& a, const V2& b) noexcept { return a.x * b.y - a.y * b.x; }
Real len_sq(const V2& a) noexcept { return dot2(a, a); }

struct SimplexVertex {
    V2 a{}, b{}, w{};
    Real u = 1.0_r;
};

struct Simplex2D {
    SimplexVertex v[3];
    int count = 0;

    V2 closest() const noexcept {
        V2 p{};
        for (int i = 0; i < count; ++i) p = p + v[i].w * v[i].u;
        return p;
    }

    void witness(V2& pa, V2& pb) const noexcept {
        pa = V2{};
        pb = V2{};
        for (int i = 0; i < count; ++i) {
            pa = pa + v[i].a * v[i].u;
            pb = pb + v[i].b * v[i].u;
        }
    }

    void solve2() noexcept {
        const V2 w1 = v[0].w, w2 = v[1].w;
        const V2 e = w2 - w1;
        const Real d12_2 = -dot2(w1, e);
        if (d12_2 <= 0.0_r) {
            v[0].u = 1.0_r;
            count = 1;
            return;
        }
        const Real d12_1 = dot2(w2, e);
        if (d12_1 <= 0.0_r) {
            v[0] = v[1];
            v[0].u = 1.0_r;
            count = 1;
            return;
        }
        const Real inv = 1.0_r / (d12_1 + d12_2);
        v[0].u = d12_1 * inv;
        v[1].u = d12_2 * inv;
        count = 2;
    }

    bool solve3() noexcept {
        const V2 w1 = v[0].w, w2 = v[1].w, w3 = v[2].w;
        const V2 e12 = w2 - w1;
        const Real d12_1 = dot2(w2, e12);
        const Real d12_2 = -dot2(w1, e12);
        const V2 e13 = w3 - w1;
        const Real d13_1 = dot2(w3, e13);
        const Real d13_2 = -dot2(w1, e13);
        const V2 e23 = w3 - w2;
        const Real d23_1 = dot2(w3, e23);
        const Real d23_2 = -dot2(w2, e23);
        const Real n123 = cross2(e12, e13);
        const Real d123_1 = n123 * cross2(w2, w3);
        const Real d123_2 = n123 * cross2(w3, w1);
        const Real d123_3 = n123 * cross2(w1, w2);
        if (d12_2 <= 0.0_r && d13_2 <= 0.0_r) {
            v[0].u = 1.0_r;
            count = 1;
            return false;
        }
        if (d12_1 > 0.0_r && d12_2 > 0.0_r && d123_3 <= 0.0_r) {
            const Real inv = 1.0_r / (d12_1 + d12_2);
            v[0].u = d12_1 * inv;
            v[1].u = d12_2 * inv;
            count = 2;
            return false;
        }
        if (d13_1 > 0.0_r && d13_2 > 0.0_r && d123_2 <= 0.0_r) {
            const Real inv = 1.0_r / (d13_1 + d13_2);
            v[0].u = d13_1 * inv;
            v[2].u = d13_2 * inv;
            v[1] = v[2];
            count = 2;
            return false;
        }
        if (d12_1 <= 0.0_r && d23_2 <= 0.0_r) {
            v[0] = v[1];
            v[0].u = 1.0_r;
            count = 1;
            return false;
        }
        if (d13_1 <= 0.0_r && d23_1 <= 0.0_r) {
            v[0] = v[2];
            v[0].u = 1.0_r;
            count = 1;
            return false;
        }
        if (d23_1 > 0.0_r && d23_2 > 0.0_r && d123_1 <= 0.0_r) {
            const Real inv = 1.0_r / (d23_1 + d23_2);
            SimplexVertex p = v[1], q = v[2];
            p.u = d23_1 * inv;
            q.u = d23_2 * inv;
            v[0] = p;
            v[1] = q;
            count = 2;
            return false;
        }
        const Real inv = 1.0_r / (d123_1 + d123_2 + d123_3);
        v[0].u = d123_1 * inv;
        v[1].u = d123_2 * inv;
        v[2].u = d123_3 * inv;
        count = 3;
        return true;
    }
};

ConvexProxy2D translated(const ConvexProxy2D& p, const V2& d) noexcept {
    ConvexProxy2D out = p;
    for (std::size_t i = 0; i < out.count; ++i) out.points[i] = out.points[i] + d;
    return out;
}

Real proxy_reach(const ConvexProxy2D& p, const V2& center) noexcept {
    Real r = 0.0_r;
    for (std::size_t i = 0; i < p.count; ++i) r = std::max(r, len_sq(p.points[i] - center));
    return std::sqrt(r) + p.radius;
}

}

std::size_t ConvexProxy2D::support_index(const V2& dir) const noexcept {
    std::size_t best = 0;
    Real best_d = dot2(points[0], dir);
    for (std::size_t i = 1; i < count; ++i) {
        const Real d = dot2(points[i], dir);
        if (d > best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

V2 ConvexProxy2D::support(const V2& dir) const noexcept {
    return points[support_index(dir)];
}

V2 ConvexProxy2D::center() const noexcept {
    V2 c{};
    for (std::size_t i = 0; i < count; ++i) c = c + points[i];
    return count ? c * (1.0_r / static_cast<Real>(count)) : c;
}

ConvexProxy2D ConvexProxy2D::point(const V2& p, Real radius) noexcept {
    ConvexProxy2D out;
    out.points[0] = p;
    out.count = 1;
    out.radius = radius;
    return out;
}

ConvexProxy2D make_convex_proxy(const Shape2D& shape, const Transform2D& xf) noexcept {
    ConvexProxy2D out;
    switch (shape.type) {
    case ShapeType::Circle:
        out.points[0] = xf.apply(shape.circle.center);
        out.count = 1;
        out.radius = shape.circle.radius;
        break;
    case ShapeType::Polygon:
        out.count = shape.polygon.count;
        for (std::size_t i = 0; i < out.count; ++i) out.points[i] = xf.apply(shape.polygon.vertices[i]);
        break;
    case ShapeType::Capsule:
        out.points[0] = xf.apply(shape.capsule.point1);
        out.points[1] = xf.apply(shape.capsule.point2);
        out.count = 2;
        out.radius = shape.capsule.radius;
        break;
    case ShapeType::Edge:
        out.points[0] = xf.apply(shape.edge.v1);
        out.points[1] = xf.apply(shape.edge.v2);
        out.count = 2;
        break;
    }
    return out;
}

DistanceOutput2D gjk_distance(const ConvexProxy2D& a, const ConvexProxy2D& b, int max_iterations) noexcept {
    DistanceOutput2D out;
    if (a.count == 0 || b.count == 0) return out;
    Simplex2D s;
    s.count = 1;
    s.v[0].a = a.points[0];
    s.v[0].b = b.points[0];
    s.v[0].w = s.v[0].b - s.v[0].a;
    s.v[0].u = 1.0_r;
    bool inside = false;
    int it = 0;
    for (; it < max_iterations; ++it) {
        SimplexVertex saved[3];
        const int saved_count = s.count;
        for (int i = 0; i < saved_count; ++i) saved[i] = s.v[i];
        if (s.count == 2) s.solve2();
        else if (s.count == 3) inside = s.solve3();
        if (inside) break;
        const V2 p = s.closest();
        if (len_sq(p) < 1e-24_r) {
            inside = true;
            break;
        }
        const V2 dir = p * -1.0_r;
        SimplexVertex nv;
        nv.b = b.support(dir);
        nv.a = a.support(p);
        nv.w = nv.b - nv.a;
        bool duplicate = false;
        for (int i = 0; i < saved_count; ++i) {
            if (len_sq(saved[i].w - nv.w) < 1e-24_r) duplicate = true;
        }
        if (duplicate || dot2(p, p) - dot2(p, nv.w) <= 1e-12_r * std::max(1.0_r, dot2(p, p))) break;
        s.v[s.count++] = nv;
    }
    out.iterations = it;
    V2 pa, pb;
    s.witness(pa, pb);
    if (inside) {
        out.overlap = true;
        out.point_a = out.point_b = pa;
        out.distance = 0.0_r;
        out.normal = V2{ 0.0_r, 1.0_r };
        return out;
    }
    const Real dist = std::sqrt(len_sq(pb - pa));
    out.normal = dist > 1e-15_r ? (pb - pa) * (1.0_r / dist) : V2{ 0.0_r, 1.0_r };
    const Real r = a.radius + b.radius;
    if (dist <= r) {
        out.overlap = true;
        const V2 mid = pa + out.normal * (a.radius + 0.5_r * (dist - r));
        out.point_a = out.point_b = mid;
        out.distance = 0.0_r;
        return out;
    }
    out.distance = dist - r;
    out.point_a = pa + out.normal * a.radius;
    out.point_b = pb - out.normal * b.radius;
    return out;
}

CastOutput2D convex_cast(const ConvexProxy2D& a, const V2& translation_a, const ConvexProxy2D& b, Real target, int max_iterations) noexcept {
    CastOutput2D out;
    const Real tolerance = 1e-6_r;
    Real t = 0.0_r;
    for (int it = 0; it < max_iterations; ++it) {
        const DistanceOutput2D d = gjk_distance(translated(a, translation_a * t), b);
        if (d.overlap) {
            if (t == 0.0_r) {
                out.hit = true;
                out.fraction = 0.0_r;
                out.point = d.point_a;
                const V2 c = b.center() - a.center();
                out.normal = len_sq(c) > 1e-24_r ? c * (-1.0_r / std::sqrt(len_sq(c))) : V2{ 0.0_r, 1.0_r };
                return out;
            }
            break;
        }
        if (d.distance <= target + tolerance) {
            out.hit = true;
            out.fraction = t;
            out.point = d.point_b;
            out.normal = d.normal * -1.0_r;
            return out;
        }
        const Real closing = dot2(translation_a, d.normal);
        if (closing <= 1e-15_r) return out;
        t += (d.distance - target) / closing;
        if (t > 1.0_r) return out;
        out.normal = d.normal * -1.0_r;
        out.point = d.point_b;
    }
    out.hit = t <= 1.0_r;
    out.fraction = std::min(t, 1.0_r);
    return out;
}

bool raycast_shape(const Shape2D& shape, const Transform2D& xf, const V2& origin, const V2& direction, Real max_distance, ShapeRayHit2D& out) noexcept {
    const Real len = std::sqrt(len_sq(direction));
    if (len <= 1e-30_r || max_distance <= 0.0_r) return false;
    const V2 dir = direction * (1.0_r / len);
    if (shape.type == ShapeType::Circle) {
        const V2 c = xf.apply(shape.circle.center);
        const V2 m = origin - c;
        const Real b = dot2(m, dir);
        const Real cc = len_sq(m) - shape.circle.radius * shape.circle.radius;
        if (cc > 0.0_r && b > 0.0_r) return false;
        const Real disc = b * b - cc;
        if (disc < 0.0_r) return false;
        const Real t = std::max(0.0_r, -b - std::sqrt(disc));
        if (t > max_distance) return false;
        out.t = t;
        out.point = origin + dir * t;
        const V2 n = out.point - c;
        out.normal = len_sq(n) > 1e-24_r ? n * (1.0_r / std::sqrt(len_sq(n))) : dir * -1.0_r;
        return true;
    }
    if (shape.type == ShapeType::Polygon) {
        Real lower = 0.0_r, upper = max_distance;
        int index = -1;
        const V2 p = xf.apply_inverse(origin);
        const V2 d = xf.rotate_inverse(dir);
        for (std::size_t i = 0; i < shape.polygon.count; ++i) {
            const V2 n = shape.polygon.normals[i];
            const Real num = dot2(n, shape.polygon.vertices[i] - p);
            const Real den = dot2(n, d);
            if (den == 0.0_r) {
                if (num < 0.0_r) return false;
                continue;
            }
            const Real t = num / den;
            if (den < 0.0_r && t > lower) {
                lower = t;
                index = static_cast<int>(i);
            } else if (den > 0.0_r && t < upper) {
                upper = t;
            }
            if (upper < lower) return false;
        }
        if (index < 0) return false;
        out.t = lower;
        out.point = origin + dir * lower;
        out.normal = xf.rotate(shape.polygon.normals[static_cast<std::size_t>(index)]);
        return true;
    }
    const ConvexProxy2D target = make_convex_proxy(shape, xf);
    const CastOutput2D c = convex_cast(ConvexProxy2D::point(origin), dir * max_distance, target);
    if (!c.hit) return false;
    out.t = c.fraction * max_distance;
    out.point = c.point;
    out.normal = c.normal;
    return true;
}

bool overlap_shapes(const Shape2D& a, const Transform2D& xa, const Shape2D& b, const Transform2D& xb) noexcept {
    return gjk_distance(make_convex_proxy(a, xa), make_convex_proxy(b, xb)).overlap;
}

bool cast_shape(const Shape2D& moving, const Transform2D& start, const V2& translation, const Shape2D& target, const Transform2D& xt, Real& fraction, V2& normal, V2& point) noexcept {
    const CastOutput2D c = convex_cast(make_convex_proxy(moving, start), translation, make_convex_proxy(target, xt));
    if (!c.hit) return false;
    fraction = c.fraction;
    normal = c.normal;
    point = c.point;
    return true;
}

Real time_of_impact(const Shape2D& sa, const Sweep2D& wa, const Shape2D& sb, const Sweep2D& wb, const Transform2D& local_a, const Transform2D& local_b, Real target, int max_iterations) noexcept {
    V2 normal{};
    return time_of_impact(sa, wa, sb, wb, local_a, local_b, target, normal, max_iterations);
}

Real time_of_impact(const Shape2D& sa, const Sweep2D& wa, const Shape2D& sb, const Sweep2D& wb, const Transform2D& local_a, const Transform2D& local_b, Real target, V2& normal, int max_iterations) noexcept {
    auto pose = [](const Sweep2D& s, const Transform2D& local, Real t) {
        return s.get_transform(t).compose(local);
    };
    const ConvexProxy2D a0 = make_convex_proxy(sa, pose(wa, local_a, 0.0_r));
    const ConvexProxy2D b0 = make_convex_proxy(sb, pose(wb, local_b, 0.0_r));
    const Real ra = proxy_reach(a0, wa.position0);
    const Real rb = proxy_reach(b0, wb.position0);
    const V2 dva = wa.position1 - wa.position0;
    const V2 dvb = wb.position1 - wb.position0;
    const Real dwa = std::abs(wa.angle1 - wa.angle0);
    const Real dwb = std::abs(wb.angle1 - wb.angle0);
    const Real tolerance = 0.25_r * std::max(target, 1e-4_r);
    normal = (b0.center() - a0.center());
    normal = len_sq(normal) > 1e-24_r ? normal * (1.0_r / std::sqrt(len_sq(normal))) : V2{ 0.0_r, 1.0_r };
    Real t = 0.0_r;
    for (int it = 0; it < max_iterations; ++it) {
        const DistanceOutput2D d = gjk_distance(make_convex_proxy(sa, pose(wa, local_a, t)), make_convex_proxy(sb, pose(wb, local_b, t)));
        if (d.overlap) return t;
        normal = d.normal;
        if (d.distance <= target + tolerance) return t;
        const Real bound = dot2(dva - dvb, d.normal) + dwa * ra + dwb * rb;
        if (bound <= 1e-15_r) return 1.0_r;
        t += (d.distance - target) / bound;
        if (t >= 1.0_r) return 1.0_r;
    }
    return t;
}

} // namespace physics
} // namespace fizmo
