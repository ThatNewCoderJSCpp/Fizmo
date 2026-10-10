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
 
std::optional<RayHit> ray_plane(
    const Ray3D& ray, const vector3d& normal, const vector3d& point_on_plane,
    double max_distance = kRayInfinity, bool two_sided = true
) noexcept;
 
bool ray_aabb_interval(
    const Ray3D& ray, const vector3d& box_min, const vector3d& box_max,
    double& t_enter, double& t_exit, int* enter_axis = nullptr
) noexcept;
 
std::optional<RayHit> ray_aabb(
    const Ray3D& ray, const vector3d& box_min, const vector3d& box_max,
    double max_distance = kRayInfinity
) noexcept;
 
std::optional<RayHit> ray_sphere(
    const Ray3D& ray, const vector3d& center, double radius,
    double max_distance = kRayInfinity
) noexcept;
 
std::optional<RayHit> ray_capsule(
    const Ray3D& ray, const vector3d& a, const vector3d& b, double radius,
    double max_distance = kRayInfinity
) noexcept;
 
std::optional<TriangleHit> ray_triangle(
    const Ray3D& ray, const vector3d& a, const vector3d& b, const vector3d& c,
    double max_distance = kRayInfinity, TriangleCull cull = TriangleCull::None
) noexcept;
 
std::optional<MeshHit> ray_mesh(
    const Ray3D& ray, const graphics::Mesh3D& mesh, double max_distance = kRayInfinity,
    const math::Matrix4d* model = nullptr, TriangleCull cull = TriangleCull::None
) noexcept;
 
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
;
 
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
 
    void step() noexcept;
 
    GridHit hit() const noexcept;
 
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