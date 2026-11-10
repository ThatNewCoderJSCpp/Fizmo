#include "fizmo_library.hpp"
#include "convex_3d.hpp"
#include "simulation_3d.hpp"

namespace fizmo {
namespace physics {

namespace {

using V3 = RVec3;

constexpr Real kTiny = 1e-12_r;
constexpr Real kEps = std::numeric_limits<Real>::epsilon();

constexpr Real at_least(Real wanted, Real floor_value) noexcept { return wanted > floor_value ? wanted : floor_value; }

V3 safe_normalize(const V3& v, const V3& fallback) noexcept {
    const Real l = vec3::length(v);
    return l > 1e-14_r ? v * (1.0_r / l) : fallback;
}

V3 any_perpendicular(const V3& n) noexcept {
    V3 t1, t2;
    vec3::orthonormal_basis(safe_normalize(n, V3{ 0.0_r, 1.0_r, 0.0_r }), t1, t2);
    return t1;
}

struct Barycentric {
    Real l[4] = { 1.0_r, 0.0_r, 0.0_r, 0.0_r };
};

void keep(GjkSimplex3D& s, const int* idx, int n) noexcept {
    GjkSimplex3D r;
    for (int i = 0; i < n; ++i) {
        r.w[i] = s.w[idx[i]];
        r.a[i] = s.a[idx[i]];
        r.b[i] = s.b[idx[i]];
    }
    r.count = n;
    s = r;
}

V3 closest_segment(GjkSimplex3D& s, Barycentric& bc) noexcept {
    const V3 a = s.w[0], b = s.w[1];
    const V3 ab = b - a;
    const Real len = vec3::dot(ab, ab);
    const Real t = len > kTiny ? -vec3::dot(a, ab) / len : 0.0_r;
    if (t <= 0.0_r) {
        const int i[1] = { 0 };
        keep(s, i, 1);
        bc = Barycentric{};
        return s.w[0];
    }
    if (t >= 1.0_r) {
        const int i[1] = { 1 };
        keep(s, i, 1);
        bc = Barycentric{};
        return s.w[0];
    }
    bc.l[0] = 1.0_r - t;
    bc.l[1] = t;
    return a + ab * t;
}

V3 closest_triangle(GjkSimplex3D& s, Barycentric& bc) noexcept {
    const V3 a = s.w[0], b = s.w[1], c = s.w[2];
    const V3 ab = b - a, ac = c - a, ap = a * -1.0_r;
    const Real d1 = vec3::dot(ab, ap), d2 = vec3::dot(ac, ap);
    if (d1 <= 0.0_r && d2 <= 0.0_r) {
        const int i[1] = { 0 };
        keep(s, i, 1);
        bc = Barycentric{};
        return a;
    }
    const V3 bp = b * -1.0_r;
    const Real d3 = vec3::dot(ab, bp), d4 = vec3::dot(ac, bp);
    if (d3 >= 0.0_r && d4 <= d3) {
        const int i[1] = { 1 };
        keep(s, i, 1);
        bc = Barycentric{};
        return b;
    }
    const Real vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0_r && d1 >= 0.0_r && d3 <= 0.0_r) {
        const Real v = d1 / (d1 - d3);
        const int i[2] = { 0, 1 };
        keep(s, i, 2);
        bc.l[0] = 1.0_r - v;
        bc.l[1] = v;
        return a + ab * v;
    }
    const V3 cp = c * -1.0_r;
    const Real d5 = vec3::dot(ab, cp), d6 = vec3::dot(ac, cp);
    if (d6 >= 0.0_r && d5 <= d6) {
        const int i[1] = { 2 };
        keep(s, i, 1);
        bc = Barycentric{};
        return c;
    }
    const Real vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0_r && d2 >= 0.0_r && d6 <= 0.0_r) {
        const Real w = d2 / (d2 - d6);
        const int i[2] = { 0, 2 };
        keep(s, i, 2);
        bc.l[0] = 1.0_r - w;
        bc.l[1] = w;
        return a + ac * w;
    }
    const Real va = d3 * d6 - d5 * d4;
    if (va <= 0.0_r && (d4 - d3) >= 0.0_r && (d5 - d6) >= 0.0_r) {
        const Real w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        const int i[2] = { 1, 2 };
        keep(s, i, 2);
        bc.l[0] = 1.0_r - w;
        bc.l[1] = w;
        return b + (c - b) * w;
    }
    const Real denom = 1.0_r / (va + vb + vc);
    const Real v = vb * denom, w = vc * denom;
    bc.l[0] = 1.0_r - v - w;
    bc.l[1] = v;
    bc.l[2] = w;
    return a + ab * v + ac * w;
}

bool origin_outside_plane(const V3& a, const V3& b, const V3& c, const V3& d) noexcept {
    const V3 n = vec3::cross(b - a, c - a);
    const Real sp = vec3::dot(a * -1.0_r, n);
    const Real sd = vec3::dot(d - a, n);
    return sp * sd < 0.0_r;
}

V3 closest_tetrahedron(GjkSimplex3D& s, Barycentric& bc, bool& inside) noexcept {
    inside = false;
    const V3 p[4] = { s.w[0], s.w[1], s.w[2], s.w[3] };
    const int faces[4][4] = { { 0, 1, 2, 3 }, { 0, 2, 3, 1 }, { 0, 3, 1, 2 }, { 1, 3, 2, 0 } };
    Real best = std::numeric_limits<Real>::max();
    V3 best_point{};
    GjkSimplex3D best_simplex = s;
    Barycentric best_bc;
    bool any = false;
    for (const auto& f : faces) {
        if (!origin_outside_plane(p[f[0]], p[f[1]], p[f[2]], p[f[3]])) continue;
        any = true;
        GjkSimplex3D t;
        const int idx[3] = { f[0], f[1], f[2] };
        t = s;
        keep(t, idx, 3);
        Barycentric b;
        const V3 q = closest_triangle(t, b);
        const Real d = vec3::dot(q, q);
        if (d < best) {
            best = d;
            best_point = q;
            best_simplex = t;
            best_bc = b;
        }
    }
    if (!any) {
        inside = true;
        return V3{};
    }
    s = best_simplex;
    bc = best_bc;
    return best_point;
}

V3 closest_flat_tetrahedron(GjkSimplex3D& s, Barycentric& bc) noexcept {
    const int faces[4][3] = { { 0, 1, 2 }, { 0, 1, 3 }, { 0, 2, 3 }, { 1, 2, 3 } };
    Real best = std::numeric_limits<Real>::max();
    V3 best_point{};
    GjkSimplex3D best_simplex = s;
    Barycentric best_bc;
    for (const auto& f : faces) {
        GjkSimplex3D t = s;
        keep(t, f, 3);
        Barycentric b;
        const V3 q = closest_triangle(t, b);
        const Real d = vec3::dot(q, q);
        if (d < best) {
            best = d;
            best_point = q;
            best_simplex = t;
            best_bc = b;
        }
    }
    s = best_simplex;
    bc = best_bc;
    return best_point;
}

void witness(const GjkSimplex3D& s, const Barycentric& bc, V3& pa, V3& pb) noexcept {
    pa = V3{};
    pb = V3{};
    for (int i = 0; i < s.count; ++i) {
        pa += s.a[i] * bc.l[i];
        pb += s.b[i] * bc.l[i];
    }
}

void push_support(const ConvexProxy3D& a, const ConvexProxy3D& b, const V3& d, GjkSimplex3D& s) noexcept {
    const V3 sa = a.support(d);
    const V3 sb = b.support(d * -1.0_r);
    s.a[s.count] = sa;
    s.b[s.count] = sb;
    s.w[s.count] = sa - sb;
    ++s.count;
}

}

V3 ConvexProxy3D::support_local(const V3& d) const noexcept {
    switch (kind) {
    case Kind::Points: {
        if (count == 0) return V3{};
        std::size_t best = 0;
        Real bd = vec3::dot(points[0], d);
        for (std::size_t i = 1; i < count; ++i) {
            const Real v = vec3::dot(points[i], d);
            if (v > bd) {
                bd = v;
                best = i;
            }
        }
        return points[best];
    }
    case Kind::Box:
        return { d.x >= 0.0_r ? half_extents.x : -half_extents.x, d.y >= 0.0_r ? half_extents.y : -half_extents.y, d.z >= 0.0_r ? half_extents.z : -half_extents.z };
    case Kind::Cylinder: {
        const Real r = std::sqrt(d.x * d.x + d.z * d.z);
        const Real y = d.y >= 0.0_r ? half_height : -half_height;
        if (r < 1e-14_r) return { 0.0_r, y, 0.0_r };
        return { d.x * core_radius / r, y, d.z * core_radius / r };
    }
    case Kind::Cone: {
        const Real r = std::sqrt(d.x * d.x + d.z * d.z);
        const V3 apex{ 0.0_r, half_height, 0.0_r };
        const V3 rim = r < 1e-14_r ? V3{ 0.0_r, -half_height, 0.0_r } : V3{ d.x * core_radius / r, -half_height, d.z * core_radius / r };
        return vec3::dot(apex, d) >= vec3::dot(rim, d) ? apex : rim;
    }
    }
    return V3{};
}

V3 ConvexProxy3D::support(const V3& world_dir) const noexcept {
    return xf.apply(support_local(xf.rotate_inverse(world_dir)));
}

V3 ConvexProxy3D::center() const noexcept {
    if (kind == Kind::Points && count > 0) {
        V3 c{};
        for (std::size_t i = 0; i < count; ++i) c += points[i];
        return xf.apply(c * (1.0_r / static_cast<Real>(count)));
    }
    return xf.position;
}

int ConvexProxy3D::feature(const V3& world_dir, V3* out, int capacity) const noexcept {
    const V3 d = xf.rotate_inverse(safe_normalize(world_dir, V3{ 0.0_r, 1.0_r, 0.0_r }));
    int n = 0;
    auto put = [&](const V3& local) {
        if (n < capacity) out[n++] = xf.apply(local);
    };
    switch (kind) {
    case Kind::Points: {
        if (count == 0) return 0;
        if (hull) {
            int best = -1;
            Real bd = -2.0_r;
            for (std::size_t i = 0; i < hull->faces.size(); ++i) {
                const Real v = vec3::dot(hull->faces[i].normal, d);
                if (v > bd) {
                    bd = v;
                    best = static_cast<int>(i);
                }
            }
            if (best >= 0 && bd >= 0.995_r) {
                for (std::uint16_t idx : hull->faces[static_cast<std::size_t>(best)].indices) put(hull->vertices[idx]);
                return n;
            }
        }
        if (count == 3 && !hull) {
            const V3 tn = safe_normalize(vec3::cross(points[1] - points[0], points[2] - points[0]), V3{ 0.0_r, 1.0_r, 0.0_r });
            if (std::abs(vec3::dot(tn, d)) >= 0.995_r) {
                put(points[0]);
                put(points[1]);
                put(points[2]);
                return n;
            }
        }
        Real top = -std::numeric_limits<Real>::max();
        Real lo = std::numeric_limits<Real>::max();
        for (std::size_t i = 0; i < count; ++i) {
            const Real v = vec3::dot(points[i], d);
            top = std::max(top, v);
            lo = std::min(lo, v);
        }
        const Real tol = std::max(1e-9_r, (top - lo) * 0.02_r + 1e-6_r);
        int idx[2] = { -1, -1 };
        int k = 0;
        for (std::size_t i = 0; i < count && k < 3; ++i) {
            if (vec3::dot(points[i], d) >= top - tol) {
                if (k < 2) idx[k] = static_cast<int>(i);
                ++k;
            }
        }
        if (k == 2) {
            put(points[static_cast<std::size_t>(idx[0])]);
            put(points[static_cast<std::size_t>(idx[1])]);
            return n;
        }
        put(support_local(d));
        return n;
    }
    case Kind::Box: {
        const Real ax[3] = { std::abs(d.x), std::abs(d.y), std::abs(d.z) };
        const int axis = ax[0] >= ax[1] && ax[0] >= ax[2] ? 0 : (ax[1] >= ax[2] ? 1 : 2);
        const Real h[3] = { half_extents.x, half_extents.y, half_extents.z };
        const Real dd[3] = { d.x, d.y, d.z };
        if (ax[axis] >= 0.995_r) {
            const int u = (axis + 1) % 3, v = (axis + 2) % 3;
            const Real sgn = dd[axis] >= 0.0_r ? 1.0_r : -1.0_r;
            const Real us[4] = { -1.0_r, 1.0_r, 1.0_r, -1.0_r }, vs[4] = { -1.0_r, -1.0_r, 1.0_r, 1.0_r };
            for (int i = 0; i < 4; ++i) {
                Real p[3];
                p[axis] = sgn * h[axis];
                p[u] = us[i] * h[u];
                p[v] = vs[i] * h[v];
                put(V3{ p[0], p[1], p[2] });
            }
            return n;
        }
        int thinnest = 0;
        for (int i = 1; i < 3; ++i) {
            if (ax[i] < ax[thinnest]) thinnest = i;
        }
        if (ax[thinnest] <= 0.01_r) {
            Real p[3], q[3];
            for (int i = 0; i < 3; ++i) {
                p[i] = q[i] = dd[i] >= 0.0_r ? h[i] : -h[i];
            }
            p[thinnest] = -h[thinnest];
            q[thinnest] = h[thinnest];
            put(V3{ p[0], p[1], p[2] });
            put(V3{ q[0], q[1], q[2] });
            return n;
        }
        put(support_local(d));
        return n;
    }
    case Kind::Cylinder: {
        if (std::abs(d.y) >= 0.995_r) {
            const Real y = d.y > 0.0_r ? half_height : -half_height;
            for (int i = 0; i < 8; ++i) {
                const Real a = 6.283185307179586_r * i / 8.0_r;
                put(V3{ std::cos(a) * core_radius, y, std::sin(a) * core_radius });
            }
            return n;
        }
        if (std::abs(d.y) <= 0.01_r) {
            const Real r = std::sqrt(d.x * d.x + d.z * d.z);
            const V3 radial = r > 1e-14_r ? V3{ d.x * core_radius / r, 0.0_r, d.z * core_radius / r } : V3{ core_radius, 0.0_r, 0.0_r };
            put(radial + V3{ 0.0_r, -half_height, 0.0_r });
            put(radial + V3{ 0.0_r, half_height, 0.0_r });
            return n;
        }
        put(support_local(d));
        return n;
    }
    case Kind::Cone: {
        if (d.y <= -0.995_r) {
            for (int i = 0; i < 8; ++i) {
                const Real a = 6.283185307179586_r * i / 8.0_r;
                put(V3{ std::cos(a) * core_radius, -half_height, std::sin(a) * core_radius });
            }
            return n;
        }
        const Real r = std::sqrt(d.x * d.x + d.z * d.z);
        if (r > 1e-14_r) {
            const V3 radial{ d.x / r, 0.0_r, d.z / r };
            const V3 side = safe_normalize(radial * (2.0_r * half_height) + V3{ 0.0_r, core_radius, 0.0_r }, V3{ 0.0_r, 1.0_r, 0.0_r });
            if (vec3::dot(side, d) >= 0.9995_r) {
                put(V3{ 0.0_r, half_height, 0.0_r });
                put(radial * core_radius + V3{ 0.0_r, -half_height, 0.0_r });
                return n;
            }
        }
        put(support_local(d));
        return n;
    }
    }
    return n;
}

ConvexProxy3D ConvexProxy3D::point(const V3& p, Real radius) noexcept {
    ConvexProxy3D c;
    c.kind = Kind::Points;
    c.local[0] = p;
    c.points = c.local;
    c.count = 1;
    c.radius = radius;
    return c;
}

ConvexProxy3D ConvexProxy3D::triangle(const V3& a, const V3& b, const V3& c) noexcept {
    ConvexProxy3D t;
    t.kind = Kind::Points;
    t.local[0] = a;
    t.local[1] = b;
    t.local[2] = c;
    t.points = t.local;
    t.count = 3;
    return t;
}

bool make_convex_proxy(const Shape3D& shape, const Transform3D& xf, ConvexProxy3D& out) noexcept {
    out = ConvexProxy3D{};
    out.xf = xf;
    switch (shape.type) {
    case ShapeType3D::Sphere:
        out.kind = ConvexProxy3D::Kind::Points;
        out.local[0] = shape.sphere.center;
        out.points = out.local;
        out.count = 1;
        out.radius = shape.sphere.radius;
        return true;
    case ShapeType3D::Capsule:
        out.kind = ConvexProxy3D::Kind::Points;
        out.local[0] = shape.capsule.point1;
        out.local[1] = shape.capsule.point2;
        out.points = out.local;
        out.count = 2;
        out.radius = shape.capsule.radius;
        return true;
    case ShapeType3D::Hull:
        if (!shape.hull || shape.hull->vertices.empty()) return false;
        out.kind = ConvexProxy3D::Kind::Points;
        out.points = shape.hull->vertices.data();
        out.count = shape.hull->vertices.size();
        out.hull = shape.hull.get();
        return true;
    case ShapeType3D::Cylinder:
        out.kind = ConvexProxy3D::Kind::Cylinder;
        out.half_height = shape.cylinder.half_height;
        out.core_radius = shape.cylinder.radius;
        return true;
    case ShapeType3D::Cone:
        out.kind = ConvexProxy3D::Kind::Cone;
        out.half_height = shape.cone.half_height;
        out.core_radius = shape.cone.radius;
        return true;
    case ShapeType3D::RoundedBox:
        out.kind = ConvexProxy3D::Kind::Box;
        out.half_extents = shape.rounded_box.half_extents;
        out.radius = shape.rounded_box.radius;
        return true;
    case ShapeType3D::Plane:
    case ShapeType3D::Heightfield:
    case ShapeType3D::Mesh:
        return false;
    }
    return false;
}

GjkOutput3D gjk_distance(const ConvexProxy3D& a, const ConvexProxy3D& b, int max_iterations) noexcept {
    GjkOutput3D out;
    GjkSimplex3D& s = out.simplex;
    V3 v = a.center() - b.center();
    if (vec3::length_squared(v) < kTiny) v = V3{ 1.0_r, 0.0_r, 0.0_r };
    Barycentric bc;
    Real scale = 1.0_r;
    for (int iter = 0; iter < max_iterations; ++iter) {
        out.iterations = iter + 1;
        const V3 dir = v * -1.0_r;
        const V3 sa = a.support(dir);
        const V3 sb = b.support(v);
        const V3 w = sa - sb;
        scale = std::max(scale, vec3::length_squared(w));
        if (s.count > 0) {
            const Real vv = vec3::dot(v, v);
            if (vv - vec3::dot(v, w) <= at_least(1e-10_r, 16.0_r * kEps) * vv + 1e-24_r) break;
            bool dup = false;
            for (int i = 0; i < s.count; ++i) {
                if (vec3::length_squared(s.w[i] - w) <= at_least(1e-24_r, kEps * kEps) * scale) dup = true;
            }
            if (dup) break;
        }
        s.a[s.count] = sa;
        s.b[s.count] = sb;
        s.w[s.count] = w;
        ++s.count;
        switch (s.count) {
        case 1:
            v = s.w[0];
            bc = Barycentric{};
            break;
        case 2:
            v = closest_segment(s, bc);
            break;
        case 3:
            v = closest_triangle(s, bc);
            break;
        default: {
            const V3 e1 = s.w[1] - s.w[0];
            const V3 e2 = s.w[2] - s.w[0];
            const V3 e3 = s.w[3] - s.w[0];
            const Real volume = std::abs(vec3::dot(e3, vec3::cross(e1, e2)));
            const Real extent = std::sqrt(vec3::length_squared(e1) * vec3::length_squared(e2) * vec3::length_squared(e3));
            if (volume <= at_least(1e-9_r, 256.0_r * kEps) * extent) {
                v = closest_flat_tetrahedron(s, bc);
                break;
            }
            bool inside = false;
            v = closest_tetrahedron(s, bc, inside);
            if (inside) {
                out.overlap = true;
                out.distance = 0.0_r;
                return out;
            }
            break;
        }
        }
        if (vec3::length_squared(v) <= at_least(1e-20_r, 4.0_r * kEps * kEps) * scale) {
            out.overlap = true;
            out.distance = 0.0_r;
            witness(s, bc, out.point_a, out.point_b);
            return out;
        }
    }
    witness(s, bc, out.point_a, out.point_b);
    out.distance = vec3::length(v);
    return out;
}

bool epa_penetration(const ConvexProxy3D& a, const ConvexProxy3D& b, const GjkSimplex3D& start, PenetrationOutput3D& out, int max_iterations) {
    struct Vert {
        V3 w, a, b;
    };
    struct Face {
        int i[3];
        V3 n;
        Real d;
        bool alive;
    };
    std::vector<Vert> verts;
    verts.reserve(64);
    for (int i = 0; i < start.count; ++i) verts.push_back({ start.w[i], start.a[i], start.b[i] });
    auto add_support = [&](const V3& d) {
        const V3 sa = a.support(d);
        const V3 sb = b.support(d * -1.0_r);
        verts.push_back({ sa - sb, sa, sb });
    };
    const V3 axes[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    if (verts.empty()) add_support(V3{ 1.0_r, 0.0_r, 0.0_r });
    if (verts.size() == 1) {
        for (const V3& ax : axes) {
            add_support(ax);
            if (vec3::length_squared(verts[1].w - verts[0].w) > 1e-18_r) break;
            verts.pop_back();
        }
        if (verts.size() < 2) return false;
    }
    if (verts.size() == 2) {
        const V3 d = verts[1].w - verts[0].w;
        V3 t1, t2;
        vec3::orthonormal_basis(safe_normalize(d, V3{ 1.0_r, 0.0_r, 0.0_r }), t1, t2);
        const V3 tries[4] = { t1, t1 * -1.0_r, t2, t2 * -1.0_r };
        bool ok = false;
        for (const V3& t : tries) {
            add_support(t);
            if (vec3::length_squared(vec3::cross(verts[1].w - verts[0].w, verts[2].w - verts[0].w)) > 1e-24_r) {
                ok = true;
                break;
            }
            verts.pop_back();
        }
        if (!ok) return false;
    }
    if (verts.size() == 3) {
        const V3 n = vec3::cross(verts[1].w - verts[0].w, verts[2].w - verts[0].w);
        add_support(n);
        if (std::abs(vec3::dot(verts[3].w - verts[0].w, n)) < 1e-18_r) {
            verts.pop_back();
            add_support(n * -1.0_r);
            if (std::abs(vec3::dot(verts[3].w - verts[0].w, n)) < 1e-18_r) return false;
        }
    }
    Real extent = 1.0_r;
    for (const Vert& v : verts) extent = std::max(extent, vec3::length(v.w));
    const Real flat = at_least(1e-30_r, kEps * kEps * extent * extent);
    const Real seen = at_least(1e-14_r, 8.0_r * kEps * extent);
    const Real settle = at_least(1e-9_r, 16.0_r * kEps);
    const std::size_t max_faces = 1024;
    std::vector<Face> faces;
    faces.reserve(128);
    auto make_face = [&](int i0, int i1, int i2) {
        Face f;
        f.i[0] = i0;
        f.i[1] = i1;
        f.i[2] = i2;
        const V3 n = vec3::cross(verts[static_cast<std::size_t>(i1)].w - verts[static_cast<std::size_t>(i0)].w, verts[static_cast<std::size_t>(i2)].w - verts[static_cast<std::size_t>(i0)].w);
        const Real l = vec3::length(n);
        f.n = l > flat ? n * (1.0_r / l) : V3{ 0.0_r, 1.0_r, 0.0_r };
        f.d = vec3::dot(f.n, verts[static_cast<std::size_t>(i0)].w);
        if (f.d < 0.0_r) {
            std::swap(f.i[1], f.i[2]);
            f.n = f.n * -1.0_r;
            f.d = -f.d;
        }
        f.alive = l > flat;
        return f;
    };
    {
        const V3 c = (verts[0].w + verts[1].w + verts[2].w + verts[3].w) * 0.25_r;
        const int tf[4][3] = { { 0, 1, 2 }, { 0, 3, 1 }, { 0, 2, 3 }, { 1, 3, 2 } };
        for (const auto& t : tf) {
            Face f = make_face(t[0], t[1], t[2]);
            if (vec3::dot(f.n, verts[static_cast<std::size_t>(f.i[0])].w - c) < 0.0_r) {
                std::swap(f.i[1], f.i[2]);
                f.n = f.n * -1.0_r;
                f.d = -f.d;
            }
            faces.push_back(f);
        }
    }
    int best = -1;
    for (int iter = 0; iter < max_iterations; ++iter) {
        best = -1;
        Real bd = std::numeric_limits<Real>::max();
        for (std::size_t i = 0; i < faces.size(); ++i) {
            if (faces[i].alive && faces[i].d < bd) {
                bd = faces[i].d;
                best = static_cast<int>(i);
            }
        }
        if (best < 0) return false;
        const Face f = faces[static_cast<std::size_t>(best)];
        const V3 sa = a.support(f.n);
        const V3 sb = b.support(f.n * -1.0_r);
        const V3 w = sa - sb;
        const Real gain = vec3::dot(w, f.n) - f.d;
        if (gain <= settle * std::max(1.0_r, f.d) + 1e-12_r || verts.size() > 250 || faces.size() > max_faces) break;
        const int vi = static_cast<int>(verts.size());
        verts.push_back({ w, sa, sb });
        std::vector<std::pair<int, int>> horizon;
        for (Face& g : faces) {
            if (!g.alive) continue;
            if (vec3::dot(g.n, w - verts[static_cast<std::size_t>(g.i[0])].w) > seen) {
                g.alive = false;
                for (int e = 0; e < 3; ++e) {
                    const std::pair<int, int> edge{ g.i[e], g.i[(e + 1) % 3] };
                    bool found = false;
                    for (auto it = horizon.begin(); it != horizon.end(); ++it) {
                        if (it->first == edge.second && it->second == edge.first) {
                            horizon.erase(it);
                            found = true;
                            break;
                        }
                    }
                    if (!found) horizon.push_back(edge);
                }
            }
        }
        for (const auto& e : horizon) {
            Face nf = make_face(e.first, e.second, vi);
            if (nf.alive) faces.push_back(nf);
        }
        faces.erase(std::remove_if(faces.begin(), faces.end(), [](const Face& g) { return !g.alive; }), faces.end());
        if (faces.empty()) return false;
    }
    best = -1;
    Real bd = std::numeric_limits<Real>::max();
    for (std::size_t i = 0; i < faces.size(); ++i) {
        if (faces[i].alive && faces[i].d < bd) {
            bd = faces[i].d;
            best = static_cast<int>(i);
        }
    }
    if (best < 0) return false;
    const Face& f = faces[static_cast<std::size_t>(best)];
    const V3 p = f.n * f.d;
    const V3 a0 = verts[static_cast<std::size_t>(f.i[0])].w, a1 = verts[static_cast<std::size_t>(f.i[1])].w, a2 = verts[static_cast<std::size_t>(f.i[2])].w;
    const V3 v0 = a1 - a0, v1 = a2 - a0, v2 = p - a0;
    const Real d00 = vec3::dot(v0, v0), d01 = vec3::dot(v0, v1), d11 = vec3::dot(v1, v1), d20 = vec3::dot(v2, v0), d21 = vec3::dot(v2, v1);
    const Real den = d00 * d11 - d01 * d01;
    Real l1 = 0.0_r, l2 = 0.0_r;
    if (std::abs(den) > 1e-30_r) {
        l1 = (d11 * d20 - d01 * d21) / den;
        l2 = (d00 * d21 - d01 * d20) / den;
    }
    const Real l0 = 1.0_r - l1 - l2;
    out.normal = f.n;
    out.depth = f.d;
    out.point_a = verts[static_cast<std::size_t>(f.i[0])].a * l0 + verts[static_cast<std::size_t>(f.i[1])].a * l1 + verts[static_cast<std::size_t>(f.i[2])].a * l2;
    out.point_b = verts[static_cast<std::size_t>(f.i[0])].b * l0 + verts[static_cast<std::size_t>(f.i[1])].b * l1 + verts[static_cast<std::size_t>(f.i[2])].b * l2;
    return true;
}

ConvexSeparation3D convex_separation(const ConvexProxy3D& a, const ConvexProxy3D& b) noexcept {
    ConvexSeparation3D r;
    const GjkOutput3D g = gjk_distance(a, b);
    if (!g.overlap && g.distance > 1e-10_r) {
        r.normal = (g.point_b - g.point_a) * (1.0_r / g.distance);
        r.separation = g.distance - a.radius - b.radius;
        r.point_a = g.point_a + r.normal * a.radius;
        r.point_b = g.point_b - r.normal * b.radius;
        r.valid = true;
        return r;
    }
    PenetrationOutput3D p;
    bool ok = false;
    try {
        ok = epa_penetration(a, b, g.simplex, p);
    } catch (...) {
        ok = false;
    }
    if (ok) {
        r.normal = p.normal;
        r.separation = -p.depth - a.radius - b.radius;
        r.point_a = p.point_a + r.normal * a.radius;
        r.point_b = p.point_b - r.normal * b.radius;
        r.valid = true;
        return r;
    }
    r.normal = safe_normalize(b.center() - a.center(), V3{ 0.0_r, 1.0_r, 0.0_r });
    r.separation = -(a.radius + b.radius);
    r.point_a = a.center();
    r.point_b = b.center();
    r.valid = true;
    return r;
}

namespace {

void clip_against(std::vector<V3>& poly, const V3& n, Real d) {
    if (poly.empty()) return;
    std::vector<V3> out;
    out.reserve(poly.size() + 4);
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const V3& p = poly[i];
        const V3& q = poly[(i + 1) % poly.size()];
        const Real dp = vec3::dot(n, p) - d;
        const Real dq = vec3::dot(n, q) - d;
        if (dp <= 0.0_r) out.push_back(p);
        if ((dp < 0.0_r && dq > 0.0_r) || (dp > 0.0_r && dq < 0.0_r)) out.push_back(p + (q - p) * (dp / (dp - dq)));
    }
    if (poly.size() == 2 && out.size() > 2) out.resize(2);
    poly.swap(out);
}

void clip_by_polygon(std::vector<V3>& subject, const V3* ref, int count, const V3& normal) {
    V3 c{};
    for (int i = 0; i < count; ++i) c += ref[i];
    c = c * (1.0_r / count);
    for (int i = 0; i < count && !subject.empty(); ++i) {
        const V3& p = ref[i];
        const V3& q = ref[(i + 1) % count];
        V3 side = vec3::cross(q - p, normal);
        if (vec3::length_squared(side) < 1e-24_r) continue;
        side = vec3::normalize(side);
        if (vec3::dot(side, c - p) > 0.0_r) side = side * -1.0_r;
        clip_against(subject, side, vec3::dot(side, p));
    }
}

}

bool collide_convex(const ConvexProxy3D& a, const ConvexProxy3D& b, ContactManifold3D& m, Real margin) {
    const ConvexSeparation3D sep = convex_separation(a, b);
    if (!sep.valid || sep.separation > margin) return false;
    V3 n = sep.normal;
    V3 fa[8], fb[8];
    const int na = a.feature(n, fa, 8);
    const int nb = b.feature(n * -1.0_r, fb, 8);
    auto face_normal = [](const V3* f, int count, const V3& toward) {
        V3 best{};
        Real best_len = 0.0_r;
        for (int i = 1; i + 1 < count; ++i) {
            const V3 c = vec3::cross(f[i] - f[0], f[i + 1] - f[0]);
            const Real len = vec3::length_squared(c);
            if (len > best_len) {
                best_len = len;
                best = c;
            }
        }
        if (best_len <= 1e-30_r) return V3{};
        best = best * (1.0_r / std::sqrt(best_len));
        return vec3::dot(best, toward) < 0.0_r ? best * -1.0_r : best;
    };
    if (na >= 3) {
        const V3 fn = face_normal(fa, na, n);
        if (vec3::dot(fn, n) > 0.995_r) n = fn;
    } else if (nb >= 3) {
        const V3 fn = face_normal(fb, nb, n * -1.0_r) * -1.0_r;
        if (vec3::dot(fn, n) > 0.995_r) n = fn;
    }
    std::vector<narrowphase3d::Candidate> cands;
    Real top_a = -std::numeric_limits<Real>::max();
    for (int i = 0; i < na; ++i) top_a = std::max(top_a, vec3::dot(n, fa[i]));
    Real low_b = std::numeric_limits<Real>::max();
    for (int i = 0; i < nb; ++i) low_b = std::min(low_b, vec3::dot(n, fb[i]));
    const Real ra = a.radius, rb = b.radius;
    auto add_b_point = [&](const V3& q) {
        const Real s = vec3::dot(n, q) - top_a - ra - rb;
        if (s > margin) return;
        const V3 pb = q - n * rb;
        cands.push_back({ pb - n * (0.5_r * s), s });
    };
    auto add_a_point = [&](const V3& p) {
        const Real s = low_b - vec3::dot(n, p) - ra - rb;
        if (s > margin) return;
        const V3 pa = p + n * ra;
        cands.push_back({ pa + n * (0.5_r * s), s });
    };
    if (na >= 3 && nb >= 2) {
        std::vector<V3> poly(fb, fb + nb);
        clip_by_polygon(poly, fa, na, n);
        for (const V3& q : poly) add_b_point(q);
    } else if (nb >= 3 && na == 2) {
        std::vector<V3> seg(fa, fa + 2);
        clip_by_polygon(seg, fb, nb, n);
        for (const V3& p : seg) add_a_point(p);
    } else if (na == 2 && nb == 2) {
        const V3 da = fa[1] - fa[0], db = fb[1] - fb[0];
        const Real la = vec3::length_squared(da);
        if (la > 1e-20_r && vec3::length_squared(vec3::cross(da, db)) <= 1e-4_r * la * vec3::length_squared(db)) {
            Real t0 = vec3::dot(fb[0] - fa[0], da) / la;
            Real t1 = vec3::dot(fb[1] - fa[0], da) / la;
            if (t0 > t1) std::swap(t0, t1);
            t0 = std::max(t0, 0.0_r);
            t1 = std::min(t1, 1.0_r);
            if (t0 <= t1) {
                const Real ts[2] = { t0, t1 };
                for (Real t : ts) {
                    const V3 p = fa[0] + da * t;
                    const V3 off = p - fa[0];
                    const V3 q_line = fb[0] + db * (vec3::dot(off + fa[0] - fb[0], db) / std::max(vec3::length_squared(db), 1e-30_r));
                    const Real s = vec3::dot(n, q_line - p) - ra - rb;
                    if (s <= margin) cands.push_back({ (p + n * ra + q_line - n * rb) * 0.5_r, s });
                }
            }
        }
    }
    if (cands.empty()) cands.push_back({ (sep.point_a + sep.point_b) * 0.5_r, sep.separation });
    m.normal = n;
    narrowphase3d::reduce_contacts(m, cands, n);
    return m.point_count > 0;
}

bool collide_convex_plane(const ConvexProxy3D& a, const PlaneShape& p, const Transform3D& xp, ContactManifold3D& m, Real margin) {
    V3 pn;
    Real pd;
    narrowphase3d::plane_in_world(p, xp, pn, pd);
    const V3 deepest = a.support(pn * -1.0_r);
    const Real s0 = vec3::dot(pn, deepest) - pd - a.radius;
    if (s0 > margin) return false;
    V3 f[8];
    const int nf = a.feature(pn * -1.0_r, f, 8);
    std::vector<narrowphase3d::Candidate> cands;
    for (int i = 0; i < nf; ++i) {
        const Real s = vec3::dot(pn, f[i]) - pd - a.radius;
        if (s > margin) continue;
        const V3 pa = f[i] - pn * a.radius;
        cands.push_back({ pa - pn * (0.5_r * s), s });
    }
    if (cands.empty()) cands.push_back({ deepest - pn * (a.radius + 0.5_r * s0), s0 });
    m.normal = pn * -1.0_r;
    narrowphase3d::reduce_contacts(m, cands, m.normal);
    return m.point_count > 0;
}

bool convex_raycast(const ConvexProxy3D& shape, const V3& origin, const V3& dir_in, Real max_t, Real& t, V3& normal) noexcept {
    const Real len = vec3::length(dir_in);
    if (len <= 1e-30_r) return false;
    const V3 dir = dir_in * (1.0_r / len);
    const Real limit = max_t * len;
    Real s = 0.0_r;
    for (int iter = 0; iter < 96; ++iter) {
        const ConvexProxy3D probe = ConvexProxy3D::point(origin + dir * s);
        const GjkOutput3D g = gjk_distance(probe, shape);
        const Real dist = g.overlap ? -1.0_r : g.distance - shape.radius;
        if (dist <= 1e-7_r * std::max(1.0_r, s)) {
            if (iter == 0 && dist < 0.0_r) {
                t = 0.0_r;
                normal = dir * -1.0_r;
                return true;
            }
            t = s / len;
            const V3 core_to_point = g.point_a - g.point_b;
            normal = safe_normalize(core_to_point, dir * -1.0_r);
            return true;
        }
        s += dist;
        if (s > limit) return false;
    }
    return false;
}

bool convex_cast(const ConvexProxy3D& moving, const V3& translation, const ConvexProxy3D& target, Real& fraction, V3& normal, V3& point) noexcept {
    Real lambda = 0.0_r;
    ConvexProxy3D a = moving;
    for (int iter = 0; iter < 64; ++iter) {
        a.xf.position = moving.xf.position + translation * lambda;
        const ConvexSeparation3D s = convex_separation(a, target);
        if (!s.valid) return false;
        if (s.separation <= 1e-6_r) {
            fraction = lambda;
            normal = s.normal * -1.0_r;
            point = s.point_b;
            return true;
        }
        const Real vn = vec3::dot(translation, s.normal);
        if (vn <= 1e-14_r) return false;
        lambda += s.separation / vn;
        if (lambda > 1.0_r) return false;
    }
    return false;
}

bool convex_overlap(const ConvexProxy3D& a, const ConvexProxy3D& b, Real tolerance) noexcept {
    const GjkOutput3D g = gjk_distance(a, b);
    if (g.overlap) return true;
    return g.distance <= a.radius + b.radius + tolerance;
}

namespace {

bool ray_triangle(const V3& o, const V3& d, const V3& a, const V3& b, const V3& c, bool two_sided, Real max_t, Real& t, V3& n) {
    const V3 e1 = b - a, e2 = c - a;
    const V3 p = vec3::cross(d, e2);
    const Real det = vec3::dot(e1, p);
    if (std::abs(det) < 1e-14_r) return false;
    if (!two_sided && det < 0.0_r) return false;
    const Real id = 1.0_r / det;
    const V3 s = o - a;
    const Real u = vec3::dot(s, p) * id;
    if (u < 0.0_r || u > 1.0_r) return false;
    const V3 q = vec3::cross(s, e1);
    const Real v = vec3::dot(d, q) * id;
    if (v < 0.0_r || u + v > 1.0_r) return false;
    const Real tt = vec3::dot(e2, q) * id;
    if (tt < 0.0_r || tt > max_t) return false;
    t = tt;
    n = safe_normalize(vec3::cross(e1, e2), V3{ 0.0_r, 1.0_r, 0.0_r });
    if (vec3::dot(n, d) > 0.0_r) n = n * -1.0_r;
    return true;
}

bool ray_heightfield(const HeightfieldShape& h, const V3& o, const V3& d, Real max_t, Real& t, V3& n, std::uint32_t& feature) {
    if (!h.valid()) return false;
    const AABB3D box = h.bounds();
    Real t0 = 0.0_r, t1 = max_t;
    const Real oo[3] = { o.x, o.y, o.z }, dd[3] = { d.x, d.y, d.z };
    const Real lo[3] = { box.min.x, box.min.y - 1e-6_r, box.min.z }, hi[3] = { box.max.x, box.max.y + 1e-6_r, box.max.z };
    for (int i = 0; i < 3; ++i) {
        if (std::abs(dd[i]) < 1e-30_r) {
            if (oo[i] < lo[i] || oo[i] > hi[i]) return false;
            continue;
        }
        Real a = (lo[i] - oo[i]) / dd[i], b = (hi[i] - oo[i]) / dd[i];
        if (a > b) std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
        if (t0 > t1) return false;
    }
    const Real x0 = -0.5_r * h.width(), z0 = -0.5_r * h.depth();
    const V3 start = o + d * t0;
    int c = std::clamp(static_cast<int>(std::floor((start.x - x0) / h.cell_x)), 0, h.columns - 2);
    int r = std::clamp(static_cast<int>(std::floor((start.z - z0) / h.cell_z)), 0, h.rows - 2);
    const int step_c = d.x > 0.0_r ? 1 : -1, step_r = d.z > 0.0_r ? 1 : -1;
    const Real next_x = x0 + (c + (step_c > 0 ? 1 : 0)) * h.cell_x;
    const Real next_z = z0 + (r + (step_r > 0 ? 1 : 0)) * h.cell_z;
    Real tmx = std::abs(d.x) > 1e-30_r ? (next_x - o.x) / d.x : std::numeric_limits<Real>::max();
    Real tmz = std::abs(d.z) > 1e-30_r ? (next_z - o.z) / d.z : std::numeric_limits<Real>::max();
    const Real dtx = std::abs(d.x) > 1e-30_r ? h.cell_x / std::abs(d.x) : std::numeric_limits<Real>::max();
    const Real dtz = std::abs(d.z) > 1e-30_r ? h.cell_z / std::abs(d.z) : std::numeric_limits<Real>::max();
    for (int guard = 0; guard < 4 * (h.columns + h.rows) + 8; ++guard) {
        const V3 a = h.vertex(c, r), b = h.vertex(c + 1, r), dv = h.vertex(c, r + 1), e = h.vertex(c + 1, r + 1);
        Real best = std::numeric_limits<Real>::max();
        bool hit = false;
        Real tt;
        V3 nn;
        if (ray_triangle(o, d, a, dv, b, true, t1, tt, nn) && tt < best) {
            best = tt;
            n = nn;
            feature = static_cast<std::uint32_t>((r * (h.columns - 1) + c) * 2);
            hit = true;
        }
        if (ray_triangle(o, d, b, dv, e, true, t1, tt, nn) && tt < best) {
            best = tt;
            n = nn;
            feature = static_cast<std::uint32_t>((r * (h.columns - 1) + c) * 2 + 1);
            hit = true;
        }
        if (hit) {
            t = best;
            return true;
        }
        if (tmx < tmz) {
            if (tmx > t1) return false;
            c += step_c;
            tmx += dtx;
        } else {
            if (tmz > t1) return false;
            r += step_r;
            tmz += dtz;
        }
        if (c < 0 || r < 0 || c > h.columns - 2 || r > h.rows - 2) return false;
    }
    return false;
}

AABB3D local_box_of(const AABB3D& world, const Transform3D& xf) {
    const V3 c = xf.apply_inverse(world.center());
    const V3 he = world.extents();
    const auto& d = xf.rotation.data;
    const V3 e{ std::abs(d[0]) * he.x + std::abs(d[3]) * he.y + std::abs(d[6]) * he.z,
                std::abs(d[1]) * he.x + std::abs(d[4]) * he.y + std::abs(d[7]) * he.z,
                std::abs(d[2]) * he.x + std::abs(d[5]) * he.y + std::abs(d[8]) * he.z };
    return { c - e, c + e };
}

template <class Fn>
void for_each_triangle_of(const Shape3D& s, const AABB3D& local_region, Fn&& fn) {
    if (s.type == ShapeType3D::Heightfield && s.heightfield) s.heightfield->for_each_triangle(local_region, fn);
    else if (s.type == ShapeType3D::Mesh && s.mesh) s.mesh->for_each_triangle(local_region, fn);
}

}

bool raycast_shape(const Shape3D& shape, const Transform3D& xf, const V3& origin, const V3& dir, Real max_t, ShapeRayHit3D& hit) noexcept {
    switch (shape.type) {
    case ShapeType3D::Sphere: {
        const V3 c = xf.apply(shape.sphere.center);
        const V3 m = origin - c;
        const Real a = vec3::dot(dir, dir);
        const Real b = vec3::dot(m, dir);
        const Real cc = vec3::dot(m, m) - shape.sphere.radius * shape.sphere.radius;
        if (a <= 1e-30_r) return false;
        if (cc > 0.0_r && b > 0.0_r) return false;
        const Real disc = b * b - a * cc;
        if (disc < 0.0_r) return false;
        Real t = (-b - std::sqrt(disc)) / a;
        if (t < 0.0_r) t = 0.0_r;
        if (t > max_t) return false;
        hit.t = t;
        hit.point = origin + dir * t;
        hit.normal = safe_normalize(hit.point - c, dir * -1.0_r);
        return true;
    }
    case ShapeType3D::Plane: {
        V3 n;
        Real d;
        narrowphase3d::plane_in_world(shape.plane, xf, n, d);
        const Real den = vec3::dot(n, dir);
        const Real num = d - vec3::dot(n, origin);
        if (num > 0.0_r) {
            hit.t = 0.0_r;
            hit.point = origin;
            hit.normal = n;
            return true;
        }
        if (den >= -1e-30_r) return false;
        const Real t = num / den;
        if (t < 0.0_r || t > max_t) return false;
        hit.t = t;
        hit.point = origin + dir * t;
        hit.normal = n;
        return true;
    }
    case ShapeType3D::Hull: {
        if (!shape.hull) return false;
        Real t_in = 0.0_r, t_out = max_t;
        V3 n_in = dir * -1.0_r;
        bool inside = true;
        for (const HullFace& f : shape.hull->faces) {
            const V3 n = xf.rotate(f.normal);
            const Real d = f.offset + vec3::dot(n, xf.position);
            const Real num = d - vec3::dot(n, origin);
            const Real den = vec3::dot(n, dir);
            if (num < 0.0_r) inside = false;
            if (std::abs(den) < 1e-30_r) {
                if (num < 0.0_r) return false;
                continue;
            }
            const Real t = num / den;
            if (den < 0.0_r) {
                if (t > t_in) {
                    t_in = t;
                    n_in = n;
                }
            } else if (t < t_out) {
                t_out = t;
            }
            if (t_in > t_out) return false;
        }
        if (inside) {
            hit.t = 0.0_r;
            hit.point = origin;
            hit.normal = dir * -1.0_r;
            return true;
        }
        hit.t = t_in;
        hit.point = origin + dir * t_in;
        hit.normal = n_in;
        return true;
    }
    case ShapeType3D::Heightfield:
    case ShapeType3D::Mesh: {
        const V3 lo = xf.apply_inverse(origin);
        const V3 ld = xf.rotate_inverse(dir);
        Real t = 0.0_r;
        V3 n;
        std::uint32_t feature = 0;
        bool ok = false;
        if (shape.type == ShapeType3D::Heightfield) ok = shape.heightfield && ray_heightfield(*shape.heightfield, lo, ld, max_t, t, n, feature);
        else ok = shape.mesh && shape.mesh->raycast(lo, ld, max_t, t, n, feature);
        if (!ok) return false;
        hit.t = t;
        hit.point = origin + dir * t;
        hit.normal = xf.rotate(n);
        hit.feature = feature;
        return true;
    }
    case ShapeType3D::Capsule:
    case ShapeType3D::Cylinder:
    case ShapeType3D::Cone:
    case ShapeType3D::RoundedBox: {
        ConvexProxy3D p;
        if (!make_convex_proxy(shape, xf, p)) return false;
        Real t = 0.0_r;
        V3 n;
        if (!convex_raycast(p, origin, dir, max_t, t, n)) return false;
        hit.t = t;
        hit.point = origin + dir * t;
        hit.normal = n;
        return true;
    }
    }
    return false;
}

bool overlap_shapes(const Shape3D& a, const Transform3D& xa, const Shape3D& b, const Transform3D& xb, Real tolerance) {
    ConvexProxy3D pa, pb;
    const bool ca = make_convex_proxy(a, xa, pa);
    const bool cb = make_convex_proxy(b, xb, pb);
    if (ca && cb) return convex_overlap(pa, pb, tolerance);
    if (!ca && !cb) return false;
    const Shape3D& other = ca ? b : a;
    const Transform3D& xo = ca ? xb : xa;
    const ConvexProxy3D& conv = ca ? pa : pb;
    if (other.type == ShapeType3D::Plane) {
        V3 n;
        Real d;
        narrowphase3d::plane_in_world(other.plane, xo, n, d);
        return vec3::dot(n, conv.support(n * -1.0_r)) - d - conv.radius <= tolerance;
    }
    const Shape3D& convex_shape = ca ? a : b;
    const Transform3D& xc = ca ? xa : xb;
    const AABB3D region = local_box_of(convex_shape.compute_aabb(xc).fatten(tolerance), xo);
    bool found = false;
    for_each_triangle_of(other, region, [&](const V3& p0, const V3& p1, const V3& p2, std::uint32_t) {
        const ConvexProxy3D tri = ConvexProxy3D::triangle(xo.apply(p0), xo.apply(p1), xo.apply(p2));
        if (convex_overlap(conv, tri, tolerance)) {
            found = true;
            return false;
        }
        return true;
    });
    return found;
}

bool cast_shape(const Shape3D& moving, const Transform3D& xa, const V3& translation, const Shape3D& target, const Transform3D& xb, Real& fraction, V3& normal, V3& point) {
    ConvexProxy3D pa;
    if (!make_convex_proxy(moving, xa, pa)) return false;
    ConvexProxy3D pb;
    if (make_convex_proxy(target, xb, pb)) return convex_cast(pa, translation, pb, fraction, normal, point);
    if (target.type == ShapeType3D::Plane) {
        V3 n;
        Real d;
        narrowphase3d::plane_in_world(target.plane, xb, n, d);
        const V3 low = pa.support(n * -1.0_r);
        const Real s = vec3::dot(n, low) - d - pa.radius;
        if (s <= 0.0_r) {
            fraction = 0.0_r;
            normal = n;
            point = low - n * pa.radius;
            return true;
        }
        const Real vn = vec3::dot(translation, n);
        if (vn >= -1e-14_r) return false;
        const Real f = s / -vn;
        if (f > 1.0_r) return false;
        fraction = f;
        normal = n;
        point = low - n * pa.radius + translation * f;
        return true;
    }
    const AABB3D start = moving.compute_aabb(xa);
    const AABB3D end(start.min + translation, start.max + translation);
    const AABB3D region = local_box_of(AABB3D::merge(start, end), xb);
    bool hit = false;
    Real best = 1.0_r;
    for_each_triangle_of(target, region, [&](const V3& p0, const V3& p1, const V3& p2, std::uint32_t) {
        const ConvexProxy3D tri = ConvexProxy3D::triangle(xb.apply(p0), xb.apply(p1), xb.apply(p2));
        Real f = 0.0_r;
        V3 nn, pp;
        if (convex_cast(pa, translation, tri, f, nn, pp) && f <= best) {
            best = f;
            fraction = f;
            normal = nn;
            point = pp;
            hit = true;
        }
        return true;
    });
    return hit;
}

bool collide_shapes_multi(const Shape3D& sa, const Transform3D& xa, const Shape3D& sb, const Transform3D& xb, Real margin, std::vector<ContactManifold3D>& out) {
    const bool mesh_a = sa.type == ShapeType3D::Heightfield || sa.type == ShapeType3D::Mesh;
    const bool mesh_b = sb.type == ShapeType3D::Heightfield || sb.type == ShapeType3D::Mesh;
    if (!mesh_a && !mesh_b) {
        ContactManifold3D m;
        if (!collide_shapes(sa, xa, sb, xb, m, margin)) return false;
        out.push_back(m);
        return true;
    }
    if (mesh_a && mesh_b) return false;
    const Shape3D& mesh = mesh_a ? sa : sb;
    const Transform3D& xm = mesh_a ? xa : xb;
    const Shape3D& conv_shape = mesh_a ? sb : sa;
    const Transform3D& xc = mesh_a ? xb : xa;
    ConvexProxy3D conv;
    if (!make_convex_proxy(conv_shape, xc, conv)) {
        if (conv_shape.type != ShapeType3D::Plane) return false;
        return false;
    }
    const bool one_sided = mesh.type == ShapeType3D::Heightfield || (mesh.mesh && !mesh.mesh->two_sided);
    const AABB3D region = local_box_of(conv_shape.compute_aabb(xc).fatten(margin + 1e-4_r), xm);
    struct Cluster {
        V3 normal;
        std::uint32_t feature;
        std::vector<narrowphase3d::Candidate> cands;
        Real deepest;
    };
    std::vector<Cluster> clusters;
    for_each_triangle_of(mesh, region, [&](const V3& p0, const V3& p1, const V3& p2, std::uint32_t tri_index) {
        const V3 w0 = xm.apply(p0), w1 = xm.apply(p1), w2 = xm.apply(p2);
        const ConvexProxy3D tri = ConvexProxy3D::triangle(w0, w1, w2);
        ContactManifold3D m;
        if (!collide_convex(conv, tri, m, margin)) return true;
        const V3 face = safe_normalize(vec3::cross(w1 - w0, w2 - w0), V3{ 0.0_r, 1.0_r, 0.0_r });
        V3 n = m.normal;
        const Real along = vec3::dot(n, face * -1.0_r);
        if (one_sided) {
            if (along < 0.7_r && vec3::dot(conv.center() - w0, face) < 0.0_r) return true;
            if (along >= 0.7_r) n = face * -1.0_r;
        } else if (std::abs(along) >= 0.7_r) {
            n = along > 0.0_r ? face * -1.0_r : face;
        }
        const Real correction = vec3::dot(n, m.normal);
        Cluster* target = nullptr;
        for (Cluster& c : clusters) {
            if (vec3::dot(c.normal, n) >= 0.95_r) {
                target = &c;
                break;
            }
        }
        if (!target) {
            clusters.push_back({ n, tri_index + 1u, {}, std::numeric_limits<Real>::max() });
            target = &clusters.back();
        }
        for (int i = 0; i < m.point_count; ++i) {
            const Real s = m.points[i].separation * correction;
            target->cands.push_back({ m.points[i].position, s });
            if (s < target->deepest) {
                target->deepest = s;
                target->feature = tri_index + 1u;
            }
        }
        return clusters.size() < 8;
    });
    bool any = false;
    for (Cluster& c : clusters) {
        if (c.cands.empty()) continue;
        ContactManifold3D m;
        m.normal = mesh_a ? c.normal * -1.0_r : c.normal;
        narrowphase3d::reduce_contacts(m, c.cands, m.normal);
        if (m.point_count == 0) continue;
        m.feature = c.feature;
        out.push_back(m);
        any = true;
    }
    return any;
}

} // namespace physics
} // namespace fizmo
