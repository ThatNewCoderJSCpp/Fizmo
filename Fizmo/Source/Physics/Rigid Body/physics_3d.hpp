#ifndef FIZMO_PHYSICS_3D_HPP
#define FIZMO_PHYSICS_3D_HPP

#include "common.hpp"

namespace fizmo {
namespace physics {

namespace vec3 {

inline double dot(const vector3d& a, const vector3d& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline vector3d cross(const vector3d& a, const vector3d& b) noexcept {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline double length_squared(const vector3d& a) noexcept { return dot(a, a); }
inline double length(const vector3d& a) noexcept { return std::sqrt(dot(a, a)); }

inline vector3d normalize(const vector3d& a, const vector3d& fallback = vector3d{0.0, 1.0, 0.0}) noexcept {
    double l = length(a);
    return l > constants::middle_epsilon() ? a * (1.0 / l) : fallback;
}

inline vector3d negate(const vector3d& a) noexcept { return a * -1.0; }
inline bool is_zero(const vector3d& a) noexcept { return a.x == 0.0 && a.y == 0.0 && a.z == 0.0; }

inline vector3d min(const vector3d& a, const vector3d& b) noexcept { return { std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) }; }
inline vector3d max(const vector3d& a, const vector3d& b) noexcept { return { std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) }; }

inline double component(const vector3d& v, int axis) noexcept { return axis == 0 ? v.x : (axis == 1 ? v.y : v.z); }

inline vector3d mul(const math::Matrix3d& m, const vector3d& v) noexcept {
    const auto& d = m.data;
    return { d[0] * v.x + d[1] * v.y + d[2] * v.z,
             d[3] * v.x + d[4] * v.y + d[5] * v.z,
             d[6] * v.x + d[7] * v.y + d[8] * v.z };
}

inline vector3d mul_transpose(const math::Matrix3d& m, const vector3d& v) noexcept {
    const auto& d = m.data;
    return { d[0] * v.x + d[3] * v.y + d[6] * v.z,
             d[1] * v.x + d[4] * v.y + d[7] * v.z,
             d[2] * v.x + d[5] * v.y + d[8] * v.z };
}

inline math::Matrix3d outer(const vector3d& a, const vector3d& b) noexcept {
    return { a.x * b.x, a.x * b.y, a.x * b.z,
             a.y * b.x, a.y * b.y, a.y * b.z,
             a.z * b.x, a.z * b.y, a.z * b.z };
}

inline void orthonormal_basis(const vector3d& n, vector3d& t1, vector3d& t2) noexcept {
    const double sign = std::copysign(1.0, n.z);
    const double a = -1.0 / (sign + n.z);
    const double b = n.x * n.y * a;
    t1 = { 1.0 + sign * n.x * n.x * a, sign * b, -sign * n.x };
    t2 = { b, sign + n.y * n.y * a, -n.y };
}

inline QuatD integrate_rotation(const QuatD& q, const vector3d& w, double dt) noexcept {
    const double speed = length(w);
    if (speed * dt <= 1e-15) return q;
    QuatD dq = QuatD::from_axis_angle(w.x, w.y, w.z, speed * dt);
    return (dq * q).normalized();
}

} // namespace vec3

struct AABB3D {
    vector3d min{}; // meters
    vector3d max{}; // meters

    constexpr AABB3D() noexcept = default;
    constexpr AABB3D(const vector3d& lo, const vector3d& hi) noexcept : min(lo), max(hi) {}

    vector3d center()  const noexcept { return (min + max) * 0.5; }
    vector3d extents() const noexcept { return (max - min) * 0.5; }

    double surface_area() const noexcept {
        double w = max.x - min.x, h = max.y - min.y, d = max.z - min.z;
        return 2.0 * (w * h + h * d + d * w);
    }

    double volume() const noexcept { return (max.x - min.x) * (max.y - min.y) * (max.z - min.z); }

    bool overlaps(const AABB3D& o) const noexcept {
        return min.x <= o.max.x && max.x >= o.min.x &&
               min.y <= o.max.y && max.y >= o.min.y &&
               min.z <= o.max.z && max.z >= o.min.z;
    }

    bool contains(const vector3d& p) const noexcept {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
    }

    bool contains(const AABB3D& o) const noexcept {
        return min.x <= o.min.x && min.y <= o.min.y && min.z <= o.min.z &&
               max.x >= o.max.x && max.y >= o.max.y && max.z >= o.max.z;
    }

    // margin in meters
    AABB3D fatten(double margin) const noexcept {
        return { { min.x - margin, min.y - margin, min.z - margin }, { max.x + margin, max.y + margin, max.z + margin } };
    }

    static AABB3D merge(const AABB3D& a, const AABB3D& b) noexcept { return { vec3::min(a.min, b.min), vec3::max(a.max, b.max) }; }
};

struct Transform3D {
    vector3d       position{};                              // meters
    QuatD          orientation{1.0, 0.0, 0.0, 0.0};         // unit quaternion
    math::Matrix3d rotation = math::Matrix3d::identity();   // cached from orientation

    Transform3D() noexcept = default;

    Transform3D(const vector3d& pos, const QuatD& q) noexcept
        : position(pos), orientation(q.normalized()), rotation(quat_to_mat3(orientation)) {}

    static Transform3D identity() noexcept { return {}; }

    vector3d apply(const vector3d& local) const noexcept { return vec3::mul(rotation, local) + position; }
    vector3d apply_inverse(const vector3d& world) const noexcept { return vec3::mul_transpose(rotation, world - position); }
    vector3d rotate(const vector3d& dir) const noexcept { return vec3::mul(rotation, dir); }
    vector3d rotate_inverse(const vector3d& dir) const noexcept { return vec3::mul_transpose(rotation, dir); }

    Transform3D compose(const Transform3D& other) const noexcept {
        return Transform3D(apply(other.position), orientation * other.orientation);
    }

    void set_orientation(const QuatD& q) noexcept {
        orientation = q.normalized();
        rotation = quat_to_mat3(orientation);
    }
};

struct Sweep3D {
    vector3d center_local{}; // meters
    vector3d position0{};    // meters (center of mass)
    vector3d position1{};    // meters (center of mass)
    QuatD    orientation0{1.0, 0.0, 0.0, 0.0};
    QuatD    orientation1{1.0, 0.0, 0.0, 0.0};

    Transform3D get_transform(double t) const noexcept { // t: unitless [0,1]
        vector3d p = position0 * (1.0 - t) + position1 * t;
        Transform3D xf(p, QuatD::nlerp(orientation0, orientation1, t));
        xf.position = p - xf.rotate(center_local);
        return xf;
    }
};

struct SphereShape {
    vector3d center{};   // meters, local body space
    double radius = 0.0; // meters
};

struct CapsuleShape3D {
    vector3d point1{};   // meters, local
    vector3d point2{};   // meters, local
    double radius = 0.0; // meters
    vector3d center() const noexcept { return (point1 + point2) * 0.5; }
    double half_length() const noexcept { return vec3::length(point2 - point1) * 0.5; } // m
};

struct PlaneShape {
    vector3d normal{0.0, 1.0, 0.0}; // unit, points out of the solid
    double   offset = 0.0;          // meters
};

struct HullFace {
    vector3d normal{};                  
    double   offset = 0.0;              
    std::vector<std::uint16_t> indices; // counter-clockwise when viewed from outside
};

struct HullShape {
    std::vector<vector3d> vertices;                    // meters, local
    std::vector<HullFace> faces;
    std::vector<std::array<std::uint16_t, 2>> edges;   // unique edges
    std::vector<vector3d> edge_directions;             // unique (up to sign) unit edge directions
    vector3d centroid{};                               // volume centroid (m, local)
    double   volume = 0.0;                             // m3

    bool valid() const noexcept { return faces.size() >= 4 && volume > 0.0; }

    static HullShape box(double hx, double hy, double hz) { // meter half-extents
        const vector3d pts[8] = {
            {-hx, -hy, -hz}, { hx, -hy, -hz}, { hx,  hy, -hz}, {-hx,  hy, -hz},
            {-hx, -hy,  hz}, { hx, -hy,  hz}, { hx,  hy,  hz}, {-hx,  hy,  hz}
        };
        return from_points(pts, 8);
    }

    static HullShape from_points(const vector3d* pts, std::size_t n) {
        HullShape h;
        std::vector<vector3d> p;
        p.reserve(n);
        vector3d lo = n ? pts[0] : vector3d{}, hi = lo;
        for (std::size_t i = 0; i < n; ++i) { lo = vec3::min(lo, pts[i]); hi = vec3::max(hi, pts[i]); }
        const double extent = std::max({ hi.x - lo.x, hi.y - lo.y, hi.z - lo.z });
        if (!(extent > 0.0)) return h;
        const double tol = 1e-7 * extent;

        for (std::size_t i = 0; i < n; ++i) {
            bool dup = false;
            for (const auto& q : p) { if (vec3::length_squared(q - pts[i]) <= tol * tol) { dup = true; break; } }
            if (!dup) p.push_back(pts[i]);
        }

        if (p.size() < 4 || p.size() > 0xFFFF) return h;
        const std::size_t m = p.size();
        struct Plane { vector3d n; double d; };
        std::vector<Plane> planes;

        for (std::size_t i = 0; i < m; ++i) {
            for (std::size_t j = i + 1; j < m; ++j) {
                for (std::size_t k = j + 1; k < m; ++k) {
                    vector3d nrm = vec3::cross(p[j] - p[i], p[k] - p[i]);
                    double len = vec3::length(nrm);
                    if (len <= tol * extent) continue; 
                    nrm = nrm * (1.0 / len);
                    double d = vec3::dot(nrm, p[i]);
                    bool above = false, below = false;

                    for (std::size_t q = 0; q < m && !(above && below); ++q) {
                        double s = vec3::dot(nrm, p[q]) - d;
                        if (s > tol) above = true;
                        else if (s < -tol) below = true;
                    }

                    if (above == below) continue; 
                    if (above) { nrm = nrm * -1.0; d = -d; }
                    bool dup = false;

                    for (const auto& pl : planes) {
                        if (vec3::dot(pl.n, nrm) > 1.0 - 1e-9 && std::abs(pl.d - d) <= tol) { dup = true; break; }
                    }

                    if (!dup) planes.push_back({ nrm, d });
                }
            }
        }

        std::vector<int> remap(m, -1);

        for (const auto& pl : planes) {
            vector3d u, w;
            vec3::orthonormal_basis(pl.n, u, w);
            struct P2 { double x, y; std::size_t idx; };
            std::vector<P2> on;

            for (std::size_t q = 0; q < m; ++q) {
                if (std::abs(vec3::dot(pl.n, p[q]) - pl.d) <= tol) on.push_back({ vec3::dot(u, p[q]), vec3::dot(w, p[q]), q });
            }

            if (on.size() < 3) continue;
            std::sort(on.begin(), on.end(), [](const P2& a, const P2& b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
            auto turn = [](const P2& o, const P2& a, const P2& b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };
            std::vector<P2> ring(2 * on.size());
            std::size_t k = 0;
            const double area_tol = tol * extent;

            for (std::size_t q = 0; q < on.size(); ++q) {
                while (k >= 2 && turn(ring[k - 2], ring[k - 1], on[q]) <= area_tol) --k;
                ring[k++] = on[q];
            }

            for (std::size_t q = on.size() - 1, t = k + 1; q-- > 0; ) {
                while (k >= t && turn(ring[k - 2], ring[k - 1], on[q]) <= area_tol) --k;
                ring[k++] = on[q];
            }

            ring.resize(k > 0 ? k - 1 : 0);
            if (ring.size() < 3) continue;
            HullFace f;
            f.normal = pl.n;

            for (const auto& r : ring) {
                if (remap[r.idx] < 0) { remap[r.idx] = static_cast<int>(h.vertices.size()); h.vertices.push_back(p[r.idx]); }
                f.indices.push_back(static_cast<std::uint16_t>(remap[r.idx]));
            }

            h.faces.push_back(std::move(f));
        }

        h.finalize();
        return h;
    }

    static HullShape from_points(const std::vector<vector3d>& pts) { return from_points(pts.data(), pts.size()); }

    void finalize() {
        edges.clear();
        edge_directions.clear();
        volume = 0.0;
        centroid;
        if (vertices.empty() || faces.empty()) return;
        for (auto& f : faces) f.offset = vec3::dot(f.normal, vertices[f.indices[0]]);

        for (const auto& f : faces) {
            for (std::size_t i = 0; i < f.indices.size(); ++i) {
                std::uint16_t a = f.indices[i], b = f.indices[(i + 1) % f.indices.size()];
                std::array<std::uint16_t, 2> e = { std::min(a, b), std::max(a, b) };
                if (std::find(edges.begin(), edges.end(), e) == edges.end()) edges.push_back(e);
            }
        }

        for (const auto& e : edges) {
            vector3d d = vec3::normalize(vertices[e[1]] - vertices[e[0]]);
            bool dup = false;
            for (const auto& q : edge_directions) { if (std::abs(vec3::dot(q, d)) > 1.0 - 1e-9) { dup = true; break; } }
            if (!dup) edge_directions.push_back(d);
        }

        vector3d o{};
        for (const auto& v : vertices) o += v;
        o = o * (1.0 / static_cast<double>(vertices.size()));
        vector3d c{};

        for (const auto& f : faces) {
            const vector3d& a = vertices[f.indices[0]];

            for (std::size_t i = 1; i + 1 < f.indices.size(); ++i) {
                const vector3d& b = vertices[f.indices[i]];
                const vector3d& d = vertices[f.indices[i + 1]];
                double vol = vec3::dot(a - o, vec3::cross(b - o, d - o)) / 6.0;
                volume += vol;
                c += (o + a + b + d) * (vol * 0.25);
            }
        }

        centroid = (volume > 0.0) ? c * (1.0 / volume) : o;
    }

    void scale(double s) {
        for (auto& v : vertices) v = v * s;
        finalize();
    }

    AABB3D bounds() const noexcept {
        if (vertices.empty()) return {};
        AABB3D b(vertices[0], vertices[0]);
        for (const auto& v : vertices) { b.min = vec3::min(b.min, v); b.max = vec3::max(b.max, v); }
        return b;
    }
};

enum class ShapeType3D : std::uint8_t {
    Sphere = 0,
    Capsule,
    Hull,
    Plane
};

struct Shape3D {
    ShapeType3D type = ShapeType3D::Sphere;

    SphereShape                      sphere{};
    CapsuleShape3D                   capsule{};
    std::shared_ptr<const HullShape> hull;     
    PlaneShape                       plane{};

    double bounding_radius = 0.0; // meters (infinite for planes)
    AABB3D local_aabb{};

    static Shape3D make_sphere(const vector3d& center, double radius) {
        Shape3D s;
        s.type = ShapeType3D::Sphere;
        s.sphere = { center, radius };
        s.rebuild_bounds();
        return s;
    }

    static Shape3D make_capsule(const vector3d& p1, const vector3d& p2, double radius) {
        Shape3D s;
        s.type = ShapeType3D::Capsule;
        s.capsule = { p1, p2, radius };
        s.rebuild_bounds();
        return s;
    }

    static Shape3D make_hull(HullShape h) {
        assert(h.valid());
        Shape3D s;
        s.type = ShapeType3D::Hull;
        s.hull = std::make_shared<const HullShape>(std::move(h));
        s.rebuild_bounds();
        return s;
    }

    static Shape3D make_box(double hx, double hy, double hz) { return make_hull(HullShape::box(hx, hy, hz)); } // meter half-extents
    static Shape3D make_convex(const vector3d* pts, std::size_t n) { return make_hull(HullShape::from_points(pts, n)); }
    static Shape3D make_convex(const std::vector<vector3d>& pts) { return make_hull(HullShape::from_points(pts)); }

    static Shape3D make_plane(const vector3d& normal, double offset) {
        Shape3D s;
        s.type = ShapeType3D::Plane;
        s.plane = { vec3::normalize(normal), offset };
        s.rebuild_bounds();
        return s;
    }

    void rebuild_bounds() {
        const double big = PLANE_EXTENT;

        switch (type) {
            case ShapeType3D::Sphere: {
                double r = sphere.radius;
                bounding_radius = vec3::length(sphere.center) + r;
                local_aabb = { sphere.center - vector3d{r, r, r}, sphere.center + vector3d{r, r, r} };
                break;
            }
            case ShapeType3D::Capsule: {
                double r = capsule.radius;
                bounding_radius = std::max(vec3::length(capsule.point1), vec3::length(capsule.point2)) + r;
                local_aabb = { vec3::min(capsule.point1, capsule.point2) - vector3d{r, r, r}, vec3::max(capsule.point1, capsule.point2) + vector3d{r, r, r} };
                break;
            }
            case ShapeType3D::Hull: {
                bounding_radius = 0.0;
                if (hull) { for (const auto& v : hull->vertices) bounding_radius = std::max(bounding_radius, vec3::length(v)); }
                local_aabb = hull ? hull->bounds() : AABB3D{};
                break;
            }
            case ShapeType3D::Plane:
                bounding_radius = big;
                local_aabb = { {-big, -big, -big}, {big, big, big} };
                break;
        }
    }

    AABB3D compute_aabb(const Transform3D& xf) const noexcept {
        switch (type) {
            case ShapeType3D::Sphere: {
                vector3d c = xf.apply(sphere.center);
                double r = sphere.radius;
                return { c - vector3d{r, r, r}, c + vector3d{r, r, r} };
            }
            case ShapeType3D::Capsule: {
                vector3d p1 = xf.apply(capsule.point1);
                vector3d p2 = xf.apply(capsule.point2);
                double r = capsule.radius;
                return { vec3::min(p1, p2) - vector3d{r, r, r}, vec3::max(p1, p2) + vector3d{r, r, r} };
            }
            case ShapeType3D::Hull: {
                if (!hull || hull->vertices.empty()) return { xf.position, xf.position };
                vector3d v = xf.apply(hull->vertices[0]);
                AABB3D box(v, v);

                for (std::size_t i = 1; i < hull->vertices.size(); ++i) {
                    v = xf.apply(hull->vertices[i]);
                    box.min = vec3::min(box.min, v);
                    box.max = vec3::max(box.max, v);
                }

                return box;
            }
            case ShapeType3D::Plane: {
                const double big = PLANE_EXTENT;
                vector3d n = xf.rotate(plane.normal);
                double d = plane.offset + vec3::dot(n, xf.position);
                double lo[3] = { -big, -big, -big }, hi[3] = { big, big, big };
                const double nn[3] = { n.x, n.y, n.z };

                for (int i = 0; i < 3; ++i) {
                    if (nn[i] >  1.0 - 1e-12) hi[i] =  d;
                    if (nn[i] < -1.0 + 1e-12) lo[i] = -d;
                }

                return { { lo[0], lo[1], lo[2] }, { hi[0], hi[1], hi[2] } };
            }
        }
        return {};
    }

    static constexpr double PLANE_EXTENT = 1e15; // meters, stands in for infinity in plane AABBs
};

struct MassData3D {
    double         mass = 0.0; // kg
    math::Matrix3d inertia{};  // kg*m2, about center, local axes
    vector3d       center{};   // meters, local body space

    static MassData3D combine(const MassData3D* parts, std::size_t n) {
        MassData3D total;
        if (n == 0) return total;
        for (std::size_t i = 0; i < n; ++i) total.mass += parts[i].mass;
        if (total.mass <= 0.0) return total;
        for (std::size_t i = 0; i < n; ++i) total.center += parts[i].center * parts[i].mass;
        total.center = total.center / total.mass;

        for (std::size_t i = 0; i < n; ++i) {
            vector3d d = parts[i].center - total.center;
            total.inertia = total.inertia + parts[i].inertia + (math::Matrix3d::identity() * vec3::length_squared(d) - vec3::outer(d, d)) * parts[i].mass;
        }

        return total;
    }
};

namespace detail {

inline double sphere_volume(const SphereShape& s) { return 4.0 / 3.0 * static_cast<double>(constants::pi()) * s.radius * s.radius * s.radius; }

inline double capsule_volume(const CapsuleShape3D& c) {
    double r = c.radius;
    double h = vec3::length(c.point2 - c.point1);
    return static_cast<double>(constants::pi()) * r * r * (h + 4.0 / 3.0 * r);
}

inline MassData3D compute_sphere_mass(const SphereShape& s, double density) {
    double mass = density * sphere_volume(s);
    double I = 0.4 * mass * s.radius * s.radius;
    return { mass, math::Matrix3d::diagonal(I, I, I), s.center };
}

inline MassData3D compute_capsule_mass(const CapsuleShape3D& c, double density) {
    const double pi = static_cast<double>(constants::pi());
    double r = c.radius;
    vector3d axis = c.point2 - c.point1;
    double h = vec3::length(axis);
    vector3d a = vec3::normalize(axis);
    double m_cyl = density * pi * r * r * h;
    double m_sph = density * 4.0 / 3.0 * pi * r * r * r; 
    double I_axis = m_cyl * r * r * 0.5 + m_sph * r * r * 0.4;
    double I_perp = m_cyl * (h * h / 12.0 + r * r * 0.25) + m_sph * (r * r * 0.4 + h * h * 0.25 + 3.0 * h * r / 8.0);
    math::Matrix3d aa = vec3::outer(a, a);
    math::Matrix3d I = aa * I_axis + (math::Matrix3d::identity() - aa) * I_perp;
    return { m_cyl + m_sph, I, c.center() };
}

inline MassData3D compute_hull_mass(const HullShape& h, double density) {
    MassData3D md;
    if (!h.valid()) return md;
    vector3d o{};
    for (const auto& v : h.vertices) o += v;
    o = o * (1.0 / static_cast<double>(h.vertices.size()));
    
    const math::Matrix3d canonical(1.0 / 60.0,  1.0 / 120.0, 1.0 / 120.0,
                                   1.0 / 120.0, 1.0 / 60.0,  1.0 / 120.0,
                                   1.0 / 120.0, 1.0 / 120.0, 1.0 / 60.0
                                );

    math::Matrix3d C{};

    for (const auto& f : h.faces) {
        const vector3d a = h.vertices[f.indices[0]] - o;

        for (std::size_t i = 1; i + 1 < f.indices.size(); ++i) {
            const vector3d b = h.vertices[f.indices[i]] - o;
            const vector3d d = h.vertices[f.indices[i + 1]] - o;

            const math::Matrix3d A(a.x, b.x, d.x,
                                   a.y, b.y, d.y,
                                   a.z, b.z, d.z
                                );

            const double det = vec3::dot(a, vec3::cross(b, d));
            C = C + (A * canonical * A.transpose()) * det;
        }
    }

    md.mass = density * h.volume;
    md.center = h.centroid;
    C = C * density;
    const vector3d com = h.centroid - o;
    C = C - vec3::outer(com, com) * md.mass; 
    md.inertia = math::Matrix3d::identity() * C.trace() - C;
    return md;
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, vector3d>::type
convert_velocity_vec(const vector3d& v, FromU from, ToU to) {
    return { convert_velocity_scalar(v.x, from, to), convert_velocity_scalar(v.y, from, to), convert_velocity_scalar(v.z, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, vector3d>::type
convert_angular_velocity_vec(const vector3d& v, FromU from, ToU to) {
    return { convert_angular_velocity_scalar(v.x, from, to), convert_angular_velocity_scalar(v.y, from, to), convert_angular_velocity_scalar(v.z, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, vector3d>::type
convert_force_vec(const vector3d& v, FromU from, ToU to) {
    return { convert_force_scalar(v.x, from, to), convert_force_scalar(v.y, from, to), convert_force_scalar(v.z, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, vector3d>::type
convert_torque_vec(const vector3d& v, FromU from, ToU to) {
    return { convert_torque_scalar(v.x, from, to), convert_torque_scalar(v.y, from, to), convert_torque_scalar(v.z, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, vector3d>::type
convert_position_vec(const vector3d& v, FromU from, ToU to) {
    return { convert_distance_scalar(v.x, from, to), convert_distance_scalar(v.y, from, to), convert_distance_scalar(v.z, from, to) };
}

} // namespace detail

inline double compute_shape_volume(const Shape3D& shape) {
    switch (shape.type) {
        case ShapeType3D::Sphere:  return detail::sphere_volume(shape.sphere);
        case ShapeType3D::Capsule: return detail::capsule_volume(shape.capsule);
        case ShapeType3D::Hull:    return shape.hull ? shape.hull->volume : 0.0;
        case ShapeType3D::Plane:   return 0.0; // infinite; treated as massless
    }
    return 0.0;
}

inline MassData3D compute_mass(const Shape3D& shape, double density) {
    switch (shape.type) {
        case ShapeType3D::Sphere:  return detail::compute_sphere_mass(shape.sphere, density);
        case ShapeType3D::Capsule: return detail::compute_capsule_mass(shape.capsule, density);
        case ShapeType3D::Hull:    return shape.hull ? detail::compute_hull_mass(*shape.hull, density) : MassData3D{};
        case ShapeType3D::Plane:   return {};
    }
    return {};
}

struct MassSpec3D {
public:
    static constexpr double STANDARD_GRAVITY = constants::gravity(); // m/s2

public:
    MassMode mode  = MassMode::Density;
    double   value = 999.9749; // kg/m3 by default (water)

public:
    constexpr MassSpec3D() noexcept = default;
    constexpr MassSpec3D(MassMode m, double v) noexcept : mode(m), value(v) {}

public:
    static constexpr MassSpec3D from_density(double kg_per_m3) noexcept { return { MassMode::Density, kg_per_m3 }; }
    static constexpr MassSpec3D from_mass(double kg) noexcept { return { MassMode::Mass, kg }; }
    static constexpr MassSpec3D from_weight(double newtons) noexcept { return { MassMode::Weight, newtons }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_3D, int>::type = 0>
    static MassSpec3D from_density(double val, U unit) { return { MassMode::Density, detail::convert_weight_scalar(val, unit, (units::mass::kilogram / units::volume::cubic_meter)) }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    static MassSpec3D from_mass(double val, U unit)    { return { MassMode::Mass   , detail::convert_weight_scalar(val, unit, units::mass::kilogram) }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    static MassSpec3D from_weight(double val, U unit)  { return { MassMode::Weight , detail::convert_force_scalar (val, unit, units::force::newton ) }; }

public:
    double resolve_density(double shape_volume_m3, double gravity = STANDARD_GRAVITY) const noexcept {
        switch (mode) {
            case MassMode::Density: return value;
            case MassMode::Mass:    return (shape_volume_m3 > 0.0) ? value / shape_volume_m3 : 0.0;
            case MassMode::Weight:  return (shape_volume_m3 > 0.0 && gravity > 0.0) ? (value / gravity) / shape_volume_m3 : 0.0;
        }
        return 0.0;
    }

    double to_mass(double shape_volume_m3, double gravity = STANDARD_GRAVITY) const noexcept {
        return resolve_density(shape_volume_m3, gravity) * shape_volume_m3;
    }
};

struct Material3D {
    MassSpec3D mass_spec{};
    double     friction    = 0.3;           // unitless [0, 1]
    double     restitution = 0.0;           // unitless [0, 1]
    double     restitution_threshold = 1.0; // m/s

    constexpr Material3D() noexcept = default;
    explicit constexpr Material3D(double density) noexcept : mass_spec(MassSpec3D::from_density(density)) {}
    constexpr Material3D(const MassSpec3D& spec, double fric = 0.3, double rest = 0.0, double rest_thresh = 1.0) noexcept : mass_spec(spec), friction(fric), restitution(rest), restitution_threshold(rest_thresh) {}

    static constexpr Material3D with_density(double kg_per_m3) noexcept { return Material3D(MassSpec3D::from_density(kg_per_m3)); }
    static constexpr Material3D with_mass(double kg) noexcept { return Material3D(MassSpec3D::from_mass(kg)); }
    static constexpr Material3D with_weight(double newtons) noexcept { return Material3D(MassSpec3D::from_weight(newtons)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_3D, int>::type = 0>
    static Material3D with_density(double val, U unit) { return Material3D(MassSpec3D::from_density(val, unit)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    static Material3D with_mass(double val, U unit) { return Material3D(MassSpec3D::from_mass(val, unit)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    static Material3D with_weight(double val, U unit) { return Material3D(MassSpec3D::from_weight(val, unit)); }
};

class RigidBody3D;

struct Collider3D {
    Shape3D         shape;
    Transform3D     local_offset;
    Material3D      material;
    CollisionFilter filter;
    bool            is_sensor = false;
    RigidBody3D*    body = nullptr;

    AABB3D          world_aabb{};
    MassData3D      mass_data{};

    void recompute_mass(double gravity = MassSpec3D::STANDARD_GRAVITY) {
        const double volume  = compute_shape_volume(shape);
        const double density = material.mass_spec.resolve_density(volume, gravity);
        mass_data = compute_mass(shape, density);
        mass_data.center  = local_offset.apply(mass_data.center);
        mass_data.inertia = rotate_inertia(mass_data.inertia, local_offset.orientation);
    }

    void update_world_aabb(const Transform3D& body_xf) {
        Transform3D world_xf = body_xf.compose(local_offset);
        world_aabb = shape.compute_aabb(world_xf);
    }
};

class RigidBody3D {
public:
    RigidBody3D() = default;
    explicit RigidBody3D(PhysicsFlags preset) : m_flags(preset) {}

public:
    PhysicsFlags flags() const noexcept { return m_flags; }

    const Transform3D& transform() const noexcept { return m_transform; }
          Transform3D& transform()       noexcept { m_world_inertia_dirty = true; return m_transform; }

    const Sweep3D& sweep() const noexcept { return m_sweep; }
          Sweep3D& sweep()       noexcept { return m_sweep; }

    const std::vector<Collider3D>& colliders() const noexcept { return m_colliders; }

    // Caller should call mark_mass_dirty() after mutating
    std::vector<Collider3D>& colliders_mut() noexcept { return m_colliders; }

    std::uint64_t collision_layer() const noexcept { return m_collision_layer; }
    std::uint64_t collision_mask()  const noexcept { return m_collision_mask; }

    void set_collision_layer(std::uint64_t layer) noexcept { m_collision_layer = layer; }
    void set_collision_mask(std::uint64_t mask)   noexcept { m_collision_mask = mask; }

    void set_collision_filter(std::uint64_t layer, std::uint64_t mask) noexcept {
        m_collision_layer = layer;
        m_collision_mask  = mask;
    }

    FrozenProperty frozen_properties() const noexcept { return m_frozen; }

    // Return false if the freeze would over-constrain
    bool freeze(FrozenProperty prop) noexcept {
        FrozenProperty candidate = m_frozen | prop;
        if (!validate_frozen(candidate)) return false;
        m_frozen = candidate;
        return true;
    }

public:
    bool has(PhysicsFlags f)                    const noexcept { return has_flag(m_flags, f); }
    void set_flag(PhysicsFlags f)                     noexcept { m_flags |= f;  m_world_inertia_dirty = true; }
    void clear_flag(PhysicsFlags f)                   noexcept { m_flags &= ~f; m_world_inertia_dirty = true; }
    void toggle_flag(PhysicsFlags f)                  noexcept { m_flags ^= f;  m_world_inertia_dirty = true; }
    void set_flag_to(PhysicsFlags f, bool on)         noexcept { on ? set_flag(f) : clear_flag(f); }

    bool is_frozen(FrozenProperty prop) const noexcept { return has_frozen(m_frozen, prop); }
    void unfreeze(FrozenProperty prop)        noexcept { m_frozen &= ~prop; }
    void unfreeze_all()                       noexcept { m_frozen = FrozenProperty::None; }

public:
    // Call after externally mutating colliders, materials, offsets, or flags
    void mark_mass_dirty() const noexcept { m_mass_dirty = true; }

    bool can_collide_with(const RigidBody3D& other) const noexcept {
        if (!has(PhysicsFlags::CollisionResponse) && !other.has(PhysicsFlags::CollisionResponse)) return false;
        if (has(PhysicsFlags::Intangible) && other.has(PhysicsFlags::Intangible)) return false;
        if ((m_collision_layer & other.m_collision_mask) == 0) return false;
        if ((other.m_collision_layer & m_collision_mask) == 0) return false;
        return true;
    }

    bool is_mass_dirty() const noexcept { return m_mass_dirty; }

    void ensure_mass_uptodate(double gravity = MassSpec3D::STANDARD_GRAVITY) const {
        if (!m_mass_dirty) return;
        recompute_mass_data(gravity);
        m_mass_dirty = false;
    }

public:
    void apply_preset(PhysicsFlags preset) noexcept {
        m_flags = preset;
        recompute_mass_data();
        m_mass_dirty = false;
    }

    Collider3D& add_collider(
        Shape3D shape,
        Material3D mat = {},
        Transform3D offset = {},
        CollisionFilter filt = {},
        bool sensor = false
    ) {
        m_colliders.emplace_back(Collider3D{
            std::move(shape),
            std::move(offset),
            std::move(mat),
            std::move(filt),
            sensor,
            this,
            {},
            {}
        });

        Collider3D& c = m_colliders.back();
        c.update_world_aabb(m_transform);
        m_mass_dirty = true;
        return c;
    }

public:
    vector3d position() const noexcept { return m_transform.position; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    vector3d position(U distance_unit) const noexcept {
        return detail::convert_position_vec(m_transform.position, units::distance::meter, distance_unit);
    }

    void set_position(const vector3d& pos) noexcept { m_transform.position = pos; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    void set_position(const vector3d& pos, U distance_unit) noexcept {
        m_transform.position = detail::convert_position_vec(pos, distance_unit, units::distance::meter);
    }

public:
    const QuatD& orientation() const noexcept { return m_transform.orientation; }

    void set_orientation(const QuatD& q) noexcept {
        m_transform.set_orientation(q);
        m_world_inertia_dirty = true;
    }

    void set_rotation(const vector3d& axis, double angle, bool in_rad = true) noexcept {
        if (!in_rad) angle /= constants::reciprocal_pi_180();
        set_orientation(QuatD::from_axis_angle(axis.x, axis.y, axis.z, angle));
    }

    // {roll (about X), pitch (about Y), yaw (about Z)}
    std::array<double, 3> euler_angles(bool in_rad = true) const noexcept {
        std::array<double, 3> e = m_transform.orientation.to_euler();
        if (!in_rad) for (auto& a : e) a *= constants::reciprocal_pi_180();
        return e;
    }

    void set_euler_angles(double roll, double pitch, double yaw, bool in_rad = true) noexcept {
        if (!in_rad) {
            roll  *= constants::pi_180();
            pitch *= constants::pi_180();
            yaw   *= constants::pi_180();
        }

        set_orientation(QuatD::from_euler(roll, pitch, yaw));
    }

public:
    vector3d linear_velocity() const noexcept { return m_linear_velocity; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    vector3d linear_velocity(U unit) const noexcept {
        return detail::convert_velocity_vec(m_linear_velocity, units::velocity::meters_per_second, unit);
    }

    void set_linear_velocity(const vector3d& v) noexcept {
        m_linear_velocity = v;
        if (!vec3::is_zero(v)) wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    void set_linear_velocity(const vector3d& v, U unit) noexcept {
        set_linear_velocity(detail::convert_velocity_vec(v, unit, units::velocity::meters_per_second));
    }

public:
    // World-space angular velocity (rad/s); its direction is the spin axis.
    vector3d angular_velocity() const noexcept { return m_angular_velocity; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    vector3d angular_velocity(U unit) const noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        return detail::convert_angular_velocity_vec(m_angular_velocity, rad_per_s, unit);
    }

    void set_angular_velocity(const vector3d& w) noexcept {
        m_angular_velocity = w;
        if (!vec3::is_zero(w)) wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    void set_angular_velocity(const vector3d& w, U unit) noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        set_angular_velocity(detail::convert_angular_velocity_vec(w, unit, rad_per_s));
    }

public:
    vector3d force() const noexcept { return m_force; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    vector3d force(U force_unit) const noexcept {
        return detail::convert_force_vec(m_force, units::force::newton, force_unit);
    }

    void set_force(const vector3d& f) noexcept { m_force = f; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    void set_force(const vector3d& f, U unit) noexcept {
        m_force = detail::convert_force_vec(f, unit, units::force::newton);
    }

public:
    vector3d torque() const noexcept { return m_torque; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    vector3d torque(U unit) const noexcept {
        return detail::convert_torque_vec(m_torque, units::torque::newton_meter, unit);
    }

    void set_torque(const vector3d& t) noexcept { m_torque = t; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    void set_torque(const vector3d& t, U torque_unit) noexcept {
        m_torque = detail::convert_torque_vec(t, torque_unit, units::torque::newton_meter);
    }

public:
    double density() const noexcept {
        ensure_mass_uptodate();
        return m_density;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_3D, int>::type = 0>
    double density(U density_unit) const noexcept {
        ensure_mass_uptodate();
        static constexpr auto si_density = units::mass::kilogram / units::volume::cubic_meter;
        return static_cast<double>(units::convert(static_cast<long double>(m_density), si_density, density_unit));
    }

    void set_density(double d) noexcept {
        if (is_frozen(FrozenProperty::Density)) return;
        ensure_mass_uptodate();
        refresh_shape_volume();
        m_density = d;
        resolve_mass_constraint_from(FrozenProperty::Density);
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_3D, int>::type = 0>
    void set_density(double d, U density_unit) noexcept {
        static constexpr auto si_density = units::mass::kilogram / units::volume::cubic_meter;
        set_density(static_cast<double>(units::convert(static_cast<long double>(d), density_unit, si_density)));
    }

public:
    double mass(double gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return m_mass;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    double mass(U mass_unit, double gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return detail::convert_weight_scalar(m_mass, units::mass::kilogram, mass_unit);
    }

    double inv_mass(double gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return m_inv_mass;
    }

    void set_mass(double kg) noexcept {
        if (is_frozen(FrozenProperty::Mass)) return;
        ensure_mass_uptodate();
        refresh_shape_volume();
        m_mass     = kg;
        m_inv_mass = (kg > 0.0) ? 1.0 / kg : 0.0;
        resolve_mass_constraint_from(FrozenProperty::Mass);
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    void set_mass(double val, U mass_unit) noexcept {
        set_mass(detail::convert_weight_scalar(val, mass_unit, units::mass::kilogram));
    }

    void set_mass_from_weight(double newtons, double gravity = MassSpec3D::STANDARD_GRAVITY) noexcept {
        set_mass((gravity > 0.0) ? newtons / gravity : 0.0);
    }

public:
    const math::Matrix3d& inertia(double gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return m_inertia;
    }

    const math::Matrix3d& inv_inertia_local(double gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return m_inv_inertia;
    }

    const math::Matrix3d& inv_inertia_world() const noexcept {
        ensure_mass_uptodate();

        if (m_world_inertia_dirty) {
            const math::Matrix3d& R = m_transform.rotation;
            math::Matrix3d W = R * m_inv_inertia * R.transpose();
            const bool lock[3] = { has(PhysicsFlags::LockRotationX), has(PhysicsFlags::LockRotationY), has(PhysicsFlags::LockRotationZ) };

            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    if (lock[r] || lock[c]) W.at(static_cast<std::size_t>(r), static_cast<std::size_t>(c)) = 0.0;
                }
            }

            m_inv_inertia_world = W;
            m_world_inertia_dirty = false;
        }

        return m_inv_inertia_world;
    }

    vector3d apply_inv_inertia(const vector3d& v) const noexcept { return vec3::mul(inv_inertia_world(), v); }

public:
    double shape_volume() const noexcept {
        ensure_mass_uptodate();
        return m_shape_volume;
    }

public:
    vector3d local_center() const noexcept {
        ensure_mass_uptodate();
        return m_local_center;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    vector3d local_center(U length_unit) const noexcept {
        ensure_mass_uptodate();
        return detail::convert_position_vec(m_local_center, units::distance::meter, length_unit);
    }

public:
    double linear_damping()  const noexcept { return m_linear_damping; }
    double angular_damping() const noexcept { return m_angular_damping; }

    void set_linear_damping(double d)  noexcept { m_linear_damping  = d; }
    void set_angular_damping(double d) noexcept { m_angular_damping = d; }

public:
    double max_linear_speed() const noexcept { return m_max_linear_speed; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    double max_linear_speed(U velocity_unit) const noexcept {
        return detail::convert_velocity_scalar(m_max_linear_speed, units::velocity::meters_per_second, velocity_unit);
    }

    void set_max_linear_speed(double s) noexcept { m_max_linear_speed = s; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    void set_max_linear_speed(double s, U velocity_unit) noexcept {
        m_max_linear_speed = detail::convert_velocity_scalar(s, velocity_unit, units::velocity::meters_per_second);
    }

public:
    double max_angular_speed() const noexcept { return m_max_angular_speed; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    double max_angular_speed(U angular_velocity_unit) const noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        return detail::convert_angular_velocity_scalar(m_max_angular_speed, rad_per_s, angular_velocity_unit);
    }

    void set_max_angular_speed(double s) noexcept { m_max_angular_speed = s; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    void set_max_angular_speed(double s, U angular_velocity_unit) noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        m_max_angular_speed = detail::convert_angular_velocity_scalar(s, angular_velocity_unit, rad_per_s);
    }

public:
    bool   is_awake()    const noexcept { return m_is_awake; }
    double sleep_timer() const noexcept { return m_sleep_timer; }

public:
    double gravity_scale() const noexcept { return m_gravity_scale; }
    void   set_gravity_scale(double s) noexcept { m_gravity_scale = s; }

public:
    void apply_force(const vector3d& f) {
        if (!has(PhysicsFlags::LinearForces)) return;
        m_force += f;
        wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    void apply_force(const vector3d& f, U unit) {
        apply_force(detail::convert_force_vec(f, unit, units::force::newton));
    }

    void apply_force_at(const vector3d& f, const vector3d& world_point) {
        if (!has(PhysicsFlags::LinearForces)) return;
        ensure_mass_uptodate();
        m_force += f;
        if (has(PhysicsFlags::Torque)) m_torque += vec3::cross(world_point - world_center(), f);
        wake();
    }

    template <class FU, class PU, typename std::enable_if<is_fizmo_unit_v<FU> && is_fizmo_unit_v<PU> && FU::dimension() == units::DIMENSION_FORCE && PU::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    void apply_force_at(const vector3d& f, FU f_unit, const vector3d& world_point, PU p_unit) {
        apply_force_at(
            detail::convert_force_vec(f, f_unit, units::force::newton),
            detail::convert_position_vec(world_point, p_unit, units::distance::meter)
        );
    }

public:
    void apply_torque(const vector3d& t) {
        if (!has(PhysicsFlags::Torque)) return;
        m_torque += t;
        wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    void apply_torque(const vector3d& t, U torque_unit) {
        apply_torque(detail::convert_torque_vec(t, torque_unit, units::torque::newton_meter));
    }

public:
    void apply_linear_impulse(const vector3d& impulse) {
        if (!has(PhysicsFlags::LinearImpulses)) return;
        ensure_mass_uptodate();
        m_linear_velocity += impulse * m_inv_mass;
        wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_IMPULSE, int>::type = 0>
    void apply_linear_impulse(const vector3d& impulse, U impulse_unit) {
        double f = detail::convert_impulse_scalar(1.0, impulse_unit, units::impulse::newton_second);
        apply_linear_impulse(impulse * f);
    }

    void apply_impulse_at(const vector3d& impulse, const vector3d& world_point) {
        if (!has(PhysicsFlags::LinearImpulses)) return;
        ensure_mass_uptodate();
        m_linear_velocity += impulse * m_inv_mass;
        if (has(PhysicsFlags::AngularImpulses)) m_angular_velocity += apply_inv_inertia(vec3::cross(world_point - world_center(), impulse));
        wake();
    }

    void apply_angular_impulse(const vector3d& impulse) {
        if (!has(PhysicsFlags::AngularImpulses)) return;
        ensure_mass_uptodate();
        m_angular_velocity += apply_inv_inertia(impulse);
        wake();
    }

public:
    void integrate_forces(double dt, const vector3d& gravity) {
        ensure_mass_uptodate();
        if (has(PhysicsFlags::Gravity))        m_linear_velocity += gravity * m_gravity_scale * dt;
        if (has(PhysicsFlags::LinearForces))   m_linear_velocity += m_force * m_inv_mass * dt;
        if (has(PhysicsFlags::Torque))         m_angular_velocity += apply_inv_inertia(m_torque) * dt;
        if (has(PhysicsFlags::LinearDamping))  m_linear_velocity *= 1.0 / (1.0 + dt * m_linear_damping);
        if (has(PhysicsFlags::AngularDamping)) m_angular_velocity *= 1.0 / (1.0 + dt * m_angular_damping);

        if (has(PhysicsFlags::VelocityClamping)) {
            double speed_sq = vec3::length_squared(m_linear_velocity);
            if (speed_sq > m_max_linear_speed * m_max_linear_speed) m_linear_velocity = m_linear_velocity * (m_max_linear_speed / std::sqrt(speed_sq));
        }

        if (has(PhysicsFlags::AngularClamping)) {
            double speed_sq = vec3::length_squared(m_angular_velocity);
            if (speed_sq > m_max_angular_speed * m_max_angular_speed) m_angular_velocity = m_angular_velocity * (m_max_angular_speed / std::sqrt(speed_sq));
        }

        m_angular_velocity = mask_rotation(m_angular_velocity);
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ACCELERATION, int>::type = 0>
    void integrate_forces(double dt, const vector3d& gravity, U grav_accel_unit) {
        vector3d g = {
            static_cast<double>(units::convert(gravity.x, grav_accel_unit, units::acceleration::meters_per_second_squared)),
            static_cast<double>(units::convert(gravity.y, grav_accel_unit, units::acceleration::meters_per_second_squared)),
            static_cast<double>(units::convert(gravity.z, grav_accel_unit, units::acceleration::meters_per_second_squared))
        };
        integrate_forces(dt, g);
    }

    void integrate_gyroscopic(double dt) {
        ensure_mass_uptodate();
        m_gyro_half;
        if (m_inv_mass <= 0.0 || !has(PhysicsFlags::AngularVelocity)) return;
        if (has(PhysicsFlags::LockRotationX) || has(PhysicsFlags::LockRotationY) || has(PhysicsFlags::LockRotationZ)) return;
        if (m_inertia.trace() <= 0.0 || vec3::is_zero(m_angular_velocity)) return;
        const math::Matrix3d& I  = m_inertia;
        const math::Matrix3d& Ii = m_inv_inertia;
        auto euler = [&](const vector3d& w) { return vec3::mul(Ii, vec3::cross(vec3::mul(I, w), w)); };
        const math::Matrix3d& R = m_transform.rotation;
        const vector3d wb = vec3::mul_transpose(R, m_angular_velocity);
        const vector3d k1 = euler(wb);
        const vector3d k2 = euler(wb + k1 * (dt * 0.5));
        const vector3d k3 = euler(wb + k2 * (dt * 0.5));
        const vector3d k4 = euler(wb + k3 * dt);
        const vector3d dw = vec3::mul(R, (k1 + k2 * 2.0 + k3 * 2.0 + k4) * (dt / 6.0));
        m_angular_velocity += dw;
        m_gyro_half = dw * 0.5;
    }

    void integrate_velocities(double dt) {
        ensure_mass_uptodate();
        vector3d center = m_transform.apply(m_local_center);
        m_sweep.position0    = center;
        m_sweep.orientation0 = m_transform.orientation;
        if (has(PhysicsFlags::LinearVelocity)) center += mask_translation(m_linear_velocity * dt);

        if (has(PhysicsFlags::AngularVelocity) && !has(PhysicsFlags::LockRotation3D)) {
            vector3d w = mask_rotation(m_angular_velocity - m_gyro_half);

            if (!vec3::is_zero(w)) {
                const vector3d w_body = m_transform.rotate_inverse(m_angular_velocity);
                set_orientation(vec3::integrate_rotation(m_transform.orientation, w, dt));
                if (!vec3::is_zero(m_gyro_half)) m_angular_velocity = m_transform.rotate(w_body); // carried with the body
            }
        }

        m_gyro_half;
        m_transform.position = center - m_transform.rotate(m_local_center);
        m_sweep.position1    = center;
        m_sweep.orientation1 = m_transform.orientation;
    }

    void apply_position_correction(const vector3d& d_center, const vector3d& d_rotation) {
        vector3d dc = mask_translation(d_center);
        vector3d dr = mask_rotation(d_rotation);
        if (vec3::is_zero(dc) && vec3::is_zero(dr)) return;
        ensure_mass_uptodate();
        vector3d center = m_transform.apply(m_local_center) + dc;
        if (!vec3::is_zero(dr)) set_orientation(vec3::integrate_rotation(m_transform.orientation, dr, 1.0));
        m_transform.position = center - m_transform.rotate(m_local_center);
    }

    void clear_forces() {
        m_force ;
        m_torque;
    }

    void wake() { if (!m_is_awake) { m_is_awake = true; m_sleep_timer = 0.0; }}

    void put_to_sleep() {
        m_is_awake         = false;
        m_sleep_timer      = 0.0;
        m_linear_velocity ;
        m_angular_velocity;
        m_force           ;
        m_torque          ;
        m_gyro_half       ;
    }

public:
    static constexpr double SLEEP_TIME_THRESHOLD    = 0.5;
    static constexpr double LINEAR_SLEEP_TOLERANCE  = 0.01;
    static constexpr double ANGULAR_SLEEP_TOLERANCE = 0.035;

    double advance_sleep_timer(double dt) {
        if (!has(PhysicsFlags::AllowSleep)) { m_sleep_timer = 0.0; return 0.0; }

        if (vec3::length_squared(m_linear_velocity) > LINEAR_SLEEP_TOLERANCE * LINEAR_SLEEP_TOLERANCE ||
            vec3::length_squared(m_angular_velocity) > ANGULAR_SLEEP_TOLERANCE * ANGULAR_SLEEP_TOLERANCE) {
            m_sleep_timer = 0.0;
        } else {
            m_sleep_timer += dt;
        }

        return m_sleep_timer;
    }

    void update_sleep(double dt) {
        if (!has(PhysicsFlags::AllowSleep)) return;
        if (advance_sleep_timer(dt) >= SLEEP_TIME_THRESHOLD) put_to_sleep();
    }

    void synchronize_colliders() { for (auto& c : m_colliders) c.update_world_aabb(m_transform); }

    AABB3D compute_body_aabb() const {
        if (m_colliders.empty()) return { m_transform.position, m_transform.position };
        AABB3D result = m_colliders[0].world_aabb;
        for (std::size_t i = 1; i < m_colliders.size(); ++i) result = AABB3D::merge(result, m_colliders[i].world_aabb);
        return result;
    }

    vector3d world_center() const noexcept {
        ensure_mass_uptodate();
        return m_transform.apply(m_local_center);
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    vector3d world_center(U distance_unit) const noexcept {
        return detail::convert_position_vec(world_center(), units::distance::meter, distance_unit);
    }

    vector3d get_world_point(const vector3d& local)  const noexcept { return m_transform.apply(local); }
    vector3d get_local_point(const vector3d& world)  const noexcept { return m_transform.apply_inverse(world); }
    vector3d get_world_vector(const vector3d& local) const noexcept { return m_transform.rotate(local); }
    vector3d get_local_vector(const vector3d& world) const noexcept { return m_transform.rotate_inverse(world); }

    vector3d get_velocity_at(const vector3d& world_point) const noexcept {
        return m_linear_velocity + vec3::cross(m_angular_velocity, world_point - world_center());
    }

    template <class PU, class VU, typename std::enable_if<is_fizmo_unit_v<PU> && is_fizmo_unit_v<VU> && PU::dimension() == units::DIMENSION_LENGTH && VU::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    vector3d get_velocity_at(const vector3d& world_point, PU position_distance_unit, VU velocity_unit) const noexcept {
        vector3d wp_si = detail::convert_position_vec(world_point, position_distance_unit, units::distance::meter);
        return detail::convert_velocity_vec(get_velocity_at(wp_si), units::velocity::meters_per_second, velocity_unit);
    }

    double kinetic_energy() const noexcept {
        ensure_mass_uptodate();
        double lin = 0.5 * m_mass * vec3::length_squared(m_linear_velocity);
        vector3d wb = m_transform.rotate_inverse(m_angular_velocity);
        double rot = 0.5 * vec3::dot(wb, vec3::mul(m_inertia, wb));
        return lin + rot;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ENERGY, int>::type = 0>
    double kinetic_energy(U energy_unit) const noexcept {
        return static_cast<double>(units::convert(static_cast<long double>(kinetic_energy()), units::energy::joule, energy_unit));
    }

private:
    vector3d mask_rotation(vector3d w) const noexcept {
        if (has(PhysicsFlags::LockRotationX)) w.x = 0.0;
        if (has(PhysicsFlags::LockRotationY)) w.y = 0.0;
        if (has(PhysicsFlags::LockRotationZ)) w.z = 0.0;
        return w;
    }

    vector3d mask_translation(vector3d d) const noexcept {
        if (has(PhysicsFlags::LockPositionX)) d.x = 0.0;
        if (has(PhysicsFlags::LockPositionY)) d.y = 0.0;
        if (has(PhysicsFlags::LockPositionZ)) d.z = 0.0;
        return d;
    }

    static bool validate_frozen(FrozenProperty f) noexcept {
        int count = 0;
        if (has_frozen(f, FrozenProperty::Mass))    ++count;
        if (has_frozen(f, FrozenProperty::Density)) ++count;
        bool area_or_vol = has_frozen(f, FrozenProperty::Area) || has_frozen(f, FrozenProperty::Volume);
        if (area_or_vol) ++count;
        return count <= 2;
    }

    void set_inertia(const math::Matrix3d& I) const noexcept {
        if (I.trace() > 0.0 && !has(PhysicsFlags::LockRotation3D)) {
            m_inertia     = I;
            m_inv_inertia = I.inverse();
        } else {
            m_inertia    ;
            m_inv_inertia;
        }

        m_world_inertia_dirty = true;
    }

    void recompute_mass_data(double gravity = MassSpec3D::STANDARD_GRAVITY) const {
        m_mass = 0.0; m_inv_mass = 0.0;
        m_inertia; m_inv_inertia;
        m_local_center;
        m_world_inertia_dirty = true;

        if (!has(PhysicsFlags::LinearForces) && !has(PhysicsFlags::LinearImpulses)) {
            m_sweep.center_local;
            return;
        }

        refresh_shape_volume();
        std::vector<MassData3D> parts;
        parts.reserve(m_colliders.size());

        for (auto& c : m_colliders) {
            if (c.is_sensor) continue;
            const double volume  = compute_shape_volume(c.shape);
            const double density = c.material.mass_spec.resolve_density(volume, gravity);
            MassData3D md = compute_mass(c.shape, density);
            md.center  = c.local_offset.apply(md.center);
            md.inertia = rotate_inertia(md.inertia, c.local_offset.orientation);
            parts.push_back(md);
        }

        MassData3D total = MassData3D::combine(parts.data(), parts.size());

        if (total.mass > 0.0) {
            m_mass     = total.mass;
            m_inv_mass = 1.0 / m_mass;
            m_local_center = total.center;
            set_inertia(total.inertia);
        } else {
            m_mass     = 1.0;
            m_inv_mass = 1.0;
            set_inertia({});
        }

        m_sweep.center_local = m_local_center;
        m_density = (m_shape_volume > 0.0) ? m_mass / m_shape_volume : 0.0;
    }

    void resolve_mass_constraint_from(FrozenProperty just_set) noexcept {
        const bool mass_frozen    = is_frozen(FrozenProperty::Mass);
        const bool density_frozen = is_frozen(FrozenProperty::Density);
        const bool volume_frozen  = is_frozen(FrozenProperty::Area) || is_frozen(FrozenProperty::Volume);

        if (just_set == FrozenProperty::Mass) {
            if (volume_frozen || !density_frozen) {
                m_density = (m_shape_volume > 0.0) ? m_mass / m_shape_volume : 0.0;
            } else {
                derive_volume_from_mass_density();
            }

            recompute_inertia_from_density();
        } else if (just_set == FrozenProperty::Density) {
            if (volume_frozen || !mass_frozen) {
                derive_mass_from_density_volume();
            } else {
                derive_volume_from_mass_density();
            }
        } else {
            if (density_frozen || !mass_frozen) {
                derive_mass_from_density_volume();
            } else {
                m_density = (m_shape_volume > 0.0) ? m_mass / m_shape_volume : 0.0;
                recompute_inertia_from_density();
            }
        }
    }

    void derive_mass_from_density_volume() noexcept {
        m_mass     = m_density * m_shape_volume;
        m_inv_mass = (m_mass > 0.0) ? 1.0 / m_mass : 0.0;
        recompute_inertia_from_density();
    }

    void derive_volume_from_mass_density() noexcept {
        if (m_density <= 0.0 || m_colliders.empty()) return;
        double target  = m_mass / m_density;
        double current = m_shape_volume;
        if (current <= 0.0) return;
        scale_collider_geometry(std::cbrt(target / current));
        refresh_shape_volume();
        recompute_inertia_from_density();
    }

    void scale_collider_geometry(double s) {
        for (auto& c : m_colliders) {
            switch (c.shape.type) {
                case ShapeType3D::Sphere:
                    c.shape.sphere.center = c.shape.sphere.center * s;
                    c.shape.sphere.radius *= s;
                    break;
                case ShapeType3D::Capsule:
                    c.shape.capsule.point1 = c.shape.capsule.point1 * s;
                    c.shape.capsule.point2 = c.shape.capsule.point2 * s;
                    c.shape.capsule.radius *= s;
                    break;
                case ShapeType3D::Hull:
                    if (c.shape.hull) {
                        HullShape h = *c.shape.hull; // copy-on-write: other colliders may share the hull
                        h.scale(s);
                        c.shape.hull = std::make_shared<const HullShape>(std::move(h));
                    }
                    break;
                case ShapeType3D::Plane:
                    break;
            }

            c.shape.rebuild_bounds();
        }
    }

    void refresh_shape_volume() const noexcept {
        m_shape_volume = 0.0;
        for (const auto& c : m_colliders) { if (!c.is_sensor) m_shape_volume += compute_shape_volume(c.shape); }
    }

    void recompute_inertia_from_density() noexcept {
        std::vector<MassData3D> parts;
        parts.reserve(m_colliders.size());

        for (auto& c : m_colliders) {
            if (c.is_sensor) continue;
            MassData3D md = compute_mass(c.shape, m_density);
            md.center  = c.local_offset.apply(md.center);
            md.inertia = rotate_inertia(md.inertia, c.local_offset.orientation);
            parts.push_back(md);
        }

        MassData3D combined = MassData3D::combine(parts.data(), parts.size());
        m_local_center = combined.center;
        set_inertia(combined.inertia);
        m_sweep.center_local = m_local_center;
    }

private:
    PhysicsFlags m_flags = PhysicsFlags::DefaultDynamic;

    std::uint64_t m_collision_layer = 1ULL;
    std::uint64_t m_collision_mask  = std::numeric_limits<std::uint64_t>::max();

    Transform3D     m_transform;
    mutable Sweep3D m_sweep;

    vector3d m_linear_velocity{};   // m/s
    vector3d m_angular_velocity{};  // rad/s, world space
    vector3d m_force{};             // N
    vector3d m_torque{};            // N*m
    vector3d m_gyro_half{};         // half of this step's gyroscopic change in w (see integrate_gyroscopic)

    mutable double  m_density      = 1000.0; // kg/m3
    FrozenProperty  m_frozen       = FrozenProperty::None;
    mutable double  m_shape_volume = 0.0;     // m3

    mutable bool           m_mass_dirty  = true;
    mutable double         m_mass        = 0.0;  // kg
    mutable double         m_inv_mass    = 0.0;  // 1/kg
    mutable math::Matrix3d m_inertia{};          // kg*m2, body axes about the center of mass
    mutable math::Matrix3d m_inv_inertia{};      // 1/(kg*m2), body axes
    mutable math::Matrix3d m_inv_inertia_world{};
    mutable bool           m_world_inertia_dirty = true;
    mutable vector3d       m_local_center{};     // m

    double m_linear_damping    = 0.0;   // 1/s
    double m_angular_damping   = 0.0;   // 1/s
    double m_max_linear_speed  = 1e6;   // m/s
    double m_max_angular_speed = 1e6;   // rad/s

    double m_sleep_timer = 0.0;         // s
    bool   m_is_awake    = true;

    double m_gravity_scale = 1.0;

    std::vector<Collider3D> m_colliders;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_3D_HPP