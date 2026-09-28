#ifndef FIZMO_RAY_3D_HPP
#define FIZMO_RAY_3D_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include "mesh.hpp"
#include "../Matrices/square_matrices.hpp"
#include "../Vectors/vectors.hpp"

namespace fizmo {
namespace geometry {

struct Ray3D {
    vector3d origin{};
    vector3d direction{};

    vector3d at(double t) const noexcept { return origin + direction * t; }

    static Ray3D toward(const vector3d& origin, const vector3d& direction) noexcept { return { origin, direction.unit_vector() }; }
    static Ray3D between(const vector3d& from, const vector3d& to) noexcept { return { from, (to - from).unit_vector() }; }
};

inline constexpr double kRayInfinity = std::numeric_limits<double>::infinity();
 
struct RayHit {
    double   distance = 0.0;  
    vector3d point{};
    vector3d normal{};        
    bool     inside   = false;
};
 
struct TriangleHit : RayHit {
    double u = 0.0, v = 0.0;  
    bool   front = true;      
};
 
struct MeshHit : TriangleHit {
    std::size_t triangle = 0; 
};
 
enum class TriangleCull : std::uint8_t { None, Back, Front };
 
namespace detail {
    constexpr double kEps = 1e-12;
    inline double comp(const vector3d& v, int a) noexcept { return a == 0 ? v.x : (a == 1 ? v.y : v.z); }
    inline vector3d axis_vector(int a, double s) noexcept { return { a == 0 ? s : 0.0, a == 1 ? s : 0.0, a == 2 ? s : 0.0 }; }
 
    inline RayHit inside_hit(const Ray3D& ray) noexcept {
        RayHit h; h.distance = 0.0; h.point = ray.origin; h.inside = true; return h;
    }
}
 
inline std::optional<RayHit> ray_plane(
    const Ray3D& ray, const vector3d& normal, const vector3d& point_on_plane,
    double max_distance = kRayInfinity, bool two_sided = true
) noexcept {
    const double len = normal.magnitude();
    if (len <= detail::kEps) return std::nullopt;
    const vector3d n = normal / len;
    const double denom = n.dot(ray.direction);
    if (std::fabs(denom) <= detail::kEps) return std::nullopt;       
    if (!two_sided && denom > 0.0) return std::nullopt;               
    const double t = n.dot(point_on_plane - ray.origin) / denom;
    if (t < 0.0 || t > max_distance) return std::nullopt;
    RayHit h;
    h.distance = t;
    h.point    = ray.at(t);
    h.normal   = denom < 0.0 ? n : -n;
    return h;
}
 
inline bool ray_aabb_interval(
    const Ray3D& ray, const vector3d& box_min, const vector3d& box_max,
    double& t_enter, double& t_exit, int* enter_axis = nullptr
) noexcept {
    t_enter = -kRayInfinity;
    t_exit  = kRayInfinity;
    int axis = -1;
 
    for (int a = 0; a < 3; ++a) {
        const double o = detail::comp(ray.origin, a), d = detail::comp(ray.direction, a);
        const double lo = detail::comp(box_min, a), hi = detail::comp(box_max, a);
 
        if (std::fabs(d) <= detail::kEps) {
            if (o < lo || o > hi) return false;
            continue;
        }
 
        double t1 = (lo - o) / d, t2 = (hi - o) / d;
        if (t1 > t2) std::swap(t1, t2);
        if (t1 > t_enter) { t_enter = t1; axis = a; }
        if (t2 < t_exit) t_exit = t2;
        if (t_enter > t_exit) return false;
    }
 
    if (enter_axis) *enter_axis = axis;
    return t_exit >= 0.0;
}
 
inline std::optional<RayHit> ray_aabb(
    const Ray3D& ray, const vector3d& box_min, const vector3d& box_max,
    double max_distance = kRayInfinity
) noexcept {
    double t0, t1;
    int axis = -1;
    if (!ray_aabb_interval(ray, box_min, box_max, t0, t1, &axis)) return std::nullopt;
    if (t0 < 0.0) return detail::inside_hit(ray);
    if (t0 > max_distance || axis < 0) return std::nullopt;
    RayHit h;
    h.distance = t0;
    h.point    = ray.at(t0);
    h.normal   = detail::axis_vector(axis, detail::comp(ray.direction, axis) > 0.0 ? -1.0 : 1.0);
    return h;
}
 
inline std::optional<RayHit> ray_sphere(
    const Ray3D& ray, const vector3d& center, double radius,
    double max_distance = kRayInfinity
) noexcept {
    const vector3d oc = ray.origin - center;
    const double c = oc.dot(oc) - radius * radius;
    if (c <= 0.0) return detail::inside_hit(ray);
    const double a = ray.direction.dot(ray.direction);
    if (a <= detail::kEps) return std::nullopt;
    const double b = oc.dot(ray.direction);
    if (b > 0.0) return std::nullopt;                                 
    const double disc = b * b - a * c;
    if (disc < 0.0) return std::nullopt;
    const double t = (-b - std::sqrt(disc)) / a;
    if (t < 0.0 || t > max_distance) return std::nullopt;
    RayHit h;
    h.distance = t;
    h.point    = ray.at(t);
    h.normal   = (h.point - center) / radius;
    return h;
}
 
inline std::optional<RayHit> ray_capsule(
    const Ray3D& ray, const vector3d& a, const vector3d& b, double radius,
    double max_distance = kRayInfinity
) noexcept {
    const vector3d ab = b - a;
    const double abab = ab.dot(ab);
 
    auto closest_on_segment = [&](const vector3d& p) {
        if (abab <= detail::kEps) return a;
        double s = (p - a).dot(ab) / abab;
        s = s < 0.0 ? 0.0 : (s > 1.0 ? 1.0 : s);
        return a + ab * s;
    };
 
    if ((ray.origin - closest_on_segment(ray.origin)).magnitude_squared() <= radius * radius) return detail::inside_hit(ray);
    std::optional<RayHit> best;

    auto consider = [&](double t) {
        if (t < 0.0 || t > max_distance || (best && t >= best->distance)) return;
        RayHit h;
        h.distance = t;
        h.point    = ray.at(t);
        h.normal   = (h.point - closest_on_segment(h.point)) / radius;
        best = h;
    };
 
    if (abab > detail::kEps) {
        const vector3d ao = ray.origin - a;
        const double abd = ab.dot(ray.direction), abao = ab.dot(ao);
        const double A = abab * ray.direction.dot(ray.direction) - abd * abd;
        const double B = abab * ray.direction.dot(ao) - abao * abd;
        const double C = abab * ao.dot(ao) - abao * abao - radius * radius * abab;
        const double disc = B * B - A * C;
 
        if (A > detail::kEps && disc >= 0.0) {
            const double t = (-B - std::sqrt(disc)) / A;
            const double y = abao + t * abd;
            if (y > 0.0 && y < abab) consider(t);
        }
    }
 
    if (auto s = ray_sphere(ray, a, radius, max_distance)) consider(s->distance);
    if (auto s = ray_sphere(ray, b, radius, max_distance)) consider(s->distance);
    return best;
}
 
inline std::optional<TriangleHit> ray_triangle(
    const Ray3D& ray, const vector3d& a, const vector3d& b, const vector3d& c,
    double max_distance = kRayInfinity, TriangleCull cull = TriangleCull::None
) noexcept {
    const vector3d e1 = b - a, e2 = c - a;
    const vector3d p = ray.direction.cross(e2);
    const double det = e1.dot(p);   
    if (std::fabs(det) <= detail::kEps) return std::nullopt;
    const bool front = det > 0.0;
    if ((cull == TriangleCull::Back && !front) || (cull == TriangleCull::Front && front)) return std::nullopt;
    const double inv = 1.0 / det;
    const vector3d s = ray.origin - a;
    const double u = s.dot(p) * inv;
    if (u < 0.0 || u > 1.0) return std::nullopt;
    const vector3d q = s.cross(e1);
    const double v = ray.direction.dot(q) * inv;
    if (v < 0.0 || u + v > 1.0) return std::nullopt;
    const double t = e2.dot(q) * inv;
    if (t < 0.0 || t > max_distance) return std::nullopt;
    TriangleHit h;
    h.distance = t;
    h.point    = ray.at(t);
    const vector3d n = e1.cross(e2).unit_vector();
    h.normal   = front ? n : -n;
    h.u = u; h.v = v; h.front = front;
    return h;
}
 
inline std::optional<MeshHit> ray_mesh(
    const Ray3D& ray, const graphics::Mesh3D& mesh, double max_distance = kRayInfinity,
    const math::Matrix4d* model = nullptr, TriangleCull cull = TriangleCull::None
) noexcept {
    const auto& verts = mesh.vertices();
    const auto& idx   = mesh.indices();
    const std::size_t count = mesh.indexed() ? idx.size() : verts.size();
 
    auto vertex = [&](std::size_t i) {
        const vector3d p = verts[i].position();
        if (!model) return p;
        const auto& m = model->data;

        return vector3d{ m[0] * p.x + m[1] * p.y + m[2]  * p.z + m[3],
                         m[4] * p.x + m[5] * p.y + m[6]  * p.z + m[7],
                         m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11] };
    };
 
    std::optional<MeshHit> best;
    double limit = max_distance;
 
    for (std::size_t t = 0; t + 2 < count; t += 3) {
        const std::size_t i0 = mesh.indexed() ? idx[t] : t;
        const std::size_t i1 = mesh.indexed() ? idx[t + 1] : t + 1;
        const std::size_t i2 = mesh.indexed() ? idx[t + 2] : t + 2;
        if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size()) continue;
 
        if (auto h = ray_triangle(ray, vertex(i0), vertex(i1), vertex(i2), limit, cull)) {
            MeshHit m;
            static_cast<TriangleHit&>(m) = *h;
            m.triangle = t / 3;
            best = m;
            limit = h->distance;
        }
    }
 
    return best;
}
 
struct GridHit {
    int      x = 0, y = 0, z = 0;      
    int      nx = 0, ny = 0, nz = 0;   
    double   distance = 0.0;           
    vector3d point{};
    bool     inside = false;           
};
 
class GridWalker {
public:
    explicit GridWalker(const Ray3D& ray, double cell_size = 1.0, const vector3d& grid_origin = {}) noexcept
        : m_ray(ray)
    {
        const double size = cell_size > 0.0 ? cell_size : 1.0;
 
        for (int a = 0; a < 3; ++a) {
            const double o = (detail::comp(ray.origin, a) - detail::comp(grid_origin, a)) / size;
            const double d = detail::comp(ray.direction, a);
            m_cell[a] = static_cast<int>(std::floor(o));
 
            if (d > detail::kEps) {
                m_step[a] = 1;
                m_delta[a] = size / d;
                m_next[a] = (std::floor(o) + 1.0 - o) * size / d;
            } else if (d < -detail::kEps) {
                m_step[a] = -1;
                m_delta[a] = size / -d;
                m_next[a] = (o - std::floor(o)) * size / -d;
            } else {
                m_step[a] = 0;
                m_delta[a] = kRayInfinity;
                m_next[a] = kRayInfinity;
            }
        }
    }
 
    int x() const noexcept { return m_cell[0]; }
    int y() const noexcept { return m_cell[1]; }
    int z() const noexcept { return m_cell[2]; }
 
    double distance() const noexcept { return m_distance; }
    double exit_distance() const noexcept { return std::fmin(m_next[0], std::fmin(m_next[1], m_next[2])); }
 
    int normal_x() const noexcept { return m_normal[0]; }
    int normal_y() const noexcept { return m_normal[1]; }
    int normal_z() const noexcept { return m_normal[2]; }
 
    bool started() const noexcept { return m_steps == 0; }
    bool can_step() const noexcept { return exit_distance() < kRayInfinity; }
 
    void step() noexcept {
        int axis = 0;
        if (m_next[1] < m_next[axis]) axis = 1;
        if (m_next[2] < m_next[axis]) axis = 2;
        if (m_next[axis] == kRayInfinity) return;
        m_distance = m_next[axis];
        m_cell[axis] += m_step[axis];
        m_next[axis] += m_delta[axis];
        m_normal[0] = m_normal[1] = m_normal[2] = 0;
        m_normal[axis] = -m_step[axis];
        ++m_steps;
    }
 
    GridHit hit() const noexcept {
        GridHit h;
        h.x = m_cell[0]; h.y = m_cell[1]; h.z = m_cell[2];
        h.nx = m_normal[0]; h.ny = m_normal[1]; h.nz = m_normal[2];
        h.distance = m_distance;
        h.point = m_ray.at(m_distance);
        h.inside = m_steps == 0;
        return h;
    }
 
private:
    Ray3D       m_ray;
    int         m_cell[3]   = { 0, 0, 0 };
    int         m_step[3]   = { 0, 0, 0 };
    int         m_normal[3] = { 0, 0, 0 };
    double      m_delta[3]  = { 0.0, 0.0, 0.0 };
    double      m_next[3]   = { 0.0, 0.0, 0.0 };
    double      m_distance  = 0.0;
    std::size_t m_steps     = 0;
};
 
template <typename IsHit>
std::optional<GridHit> raycast_grid(
    const Ray3D& ray, double max_distance, IsHit&& is_hit,
    double cell_size = 1.0, const vector3d& grid_origin = {}
) {
    GridWalker walk(ray, cell_size, grid_origin);
 
    while (walk.distance() <= max_distance) {
        if (is_hit(walk.x(), walk.y(), walk.z())) return walk.hit();
        if (!walk.can_step()) break;
        walk.step();
    }
 
    return std::nullopt;
}

} // namespace geometry
} // namespace fizmo

#endif // FIZMO_RAY_3D_HPP