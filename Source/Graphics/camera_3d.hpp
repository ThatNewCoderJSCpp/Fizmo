#ifndef FIZMO_CAMERA3D_HPP
#define FIZMO_CAMERA3D_HPP

#include <array>
#include <cmath>
#include <limits>
#include "../Basic/constants.hpp"
#include "../Matrices/square_matrices.hpp"
#include "../Quaternions/quaternion.hpp"
#include <cmath>
#include "../Vectors/vectors.hpp"
#include "../Vectors/axes.hpp"
#include "ray.hpp"

namespace fizmo {
namespace graphics {

enum class Projection3D : unsigned char { Perspective, Orthographic };

enum class DepthRange : unsigned char { ZeroToOne, NegativeOneToOne };

struct Plane3D {
    linalg::vector3d normal{};
    double   d = 0.0;
    double signed_distance(const linalg::vector3d& p) const noexcept { return normal.dot(p) + d; }
};

struct Frustum3D {
    enum Side : unsigned char { Left = 0, Right, Bottom, Top, Near, Far };
    std::array<Plane3D, 6> planes{};

    bool contains(const linalg::vector3d& p) const noexcept {
        for (const auto& pl : planes) if (pl.signed_distance(p) < 0.0) return false;
        return true;
    }

    bool intersects_sphere(const linalg::vector3d& center, double radius) const noexcept {
        for (const auto& pl : planes) if (pl.signed_distance(center) < -radius) return false;
        return true;
    }

    bool intersects_aabb(const linalg::vector3d& box_min, const linalg::vector3d& box_max) const noexcept;
};

struct ScreenPoint3D {
    double x = 0.0;          
    double y = 0.0;          
    double depth = 0.0;      
    bool   in_front = false; 
    bool   on_screen = false;
};

class Camera3D {
public:
    static constexpr double DEFAULT_FOV_Y       = 70.0;  
    static constexpr double DEFAULT_NEAR        = 0.05;
    static constexpr double DEFAULT_FAR         = 1000.0;
    static constexpr double DEFAULT_PITCH_LIMIT = 89.9;  

    Camera3D() noexcept = default;

    Camera3D(const linalg::vector3d& position, unsigned int vp_w, unsigned int vp_h,
             double fov_y_degrees = DEFAULT_FOV_Y, double near_plane = DEFAULT_NEAR, double far_plane = DEFAULT_FAR) noexcept
;

    static linalg::vector3d world_up() { return linalg::vector3d{0.0, 1.0, 0.0}; }

    const Axes3D& axes() const noexcept { return m_axes; }
    void set_axes(const Axes3D& axes) noexcept { if (axes != m_axes) { m_axes = axes; mark_view_dirty(); } }
    linalg::vector3d up_direction() const noexcept { return m_axes.up(); }

public: 
    unsigned int viewport_width()  const noexcept { return m_vp_w; }
    unsigned int viewport_height() const noexcept { return m_vp_h; }

    void set_viewport_size(unsigned int w, unsigned int h) noexcept {
        if (w != m_vp_w || h != m_vp_h) { m_vp_w = w; m_vp_h = h; mark_projection_dirty(); }
    }

    double aspect() const noexcept;

public: 
    const linalg::vector3d& position() const noexcept { return m_position; }
    void set_position(const linalg::vector3d& p) noexcept { m_position = p; mark_view_dirty(); }
    void set_position(double x, double y, double z) noexcept { set_position(linalg::vector3d{x, y, z}); }
    void translate(const linalg::vector3d& delta) noexcept { m_position += delta; mark_view_dirty(); }

    void move_local(double right_amount, double up_amount, double forward_amount) noexcept;

    void move_flat(double forward_amount, double right_amount, double up_amount = 0.0) noexcept;

public: 
    const linalg::QuatD& orientation() const noexcept { return m_orientation; }

    void set_orientation(const linalg::QuatD& q) noexcept {
        if (q.magnitude_squared() <= EPS) return;
        m_orientation = q.normalized();
        mark_view_dirty();
    }

    void set_rotation(double yaw_degrees, double pitch_degrees, double roll_degrees = 0.0) noexcept;

    double yaw() const noexcept {
        rebuild_view_if_dirty();
        linalg::vector3d ff{m_forward_r.x, 0.0, m_forward_r.z};
        if (ff.magnitude_squared() <= EPS) ff = m_forward_r.y < 0.0 ? linalg::vector3d{m_up_r.x, 0.0, m_up_r.z} : linalg::vector3d{-m_up_r.x, 0.0, -m_up_r.z};
        return to_degrees(std::atan2(-ff.x, -ff.z)) * m_axes.sign();
    }

    double pitch() const noexcept;

    double roll() const noexcept;

    void add_yaw_pitch(double dyaw_degrees, double dpitch_degrees) noexcept;

    double pitch_limit() const noexcept { return m_pitch_limit; }
    void set_pitch_limit(double degrees) noexcept { m_pitch_limit = clamp(degrees, 0.0, 90.0); }

    void rotate(const linalg::vector3d& world_axis, double degrees) noexcept;

    void rotate_local(const linalg::vector3d& local_axis, double degrees) noexcept;

    void look_at(const linalg::vector3d& target, const linalg::vector3d& up_hint) noexcept {
        look_in(target - m_position, up_hint);
    }

    void look_at(const linalg::vector3d& target) noexcept { look_in(target - m_position, m_axes.up()); }

    void look_in(const linalg::vector3d& direction, const linalg::vector3d& up_hint) noexcept;

    void look_in(const linalg::vector3d& direction) noexcept { look_in(direction, m_axes.up()); }

    void orbit(const linalg::vector3d& target, double yaw_degrees, double pitch_degrees, double distance) noexcept;

    const linalg::vector3d& forward() const noexcept { rebuild_view_if_dirty(); return m_forward; }
    const linalg::vector3d& right()   const noexcept { rebuild_view_if_dirty(); return m_right; }
    const linalg::vector3d& up()      const noexcept { rebuild_view_if_dirty(); return m_up; }

    linalg::vector3d flat_forward() const noexcept;

public: 
    Projection3D projection_type() const noexcept { return m_projection; }
    DepthRange depth_range() const noexcept { return m_depth_range; }

    void set_depth_range(DepthRange r) noexcept {
        if (r != m_depth_range) { m_depth_range = r; mark_projection_dirty(); }
    }

    void set_perspective(double fov_y_degrees, double near_plane, double far_plane) noexcept;

    void set_orthographic(double height, double near_plane, double far_plane) noexcept;

    double fov_y() const noexcept { return m_fov_y; }

    double fov_x() const noexcept;

    void set_fov_y(double degrees) noexcept {
        if (degrees > 0.0 && degrees < 180.0) { m_fov_y = degrees; mark_projection_dirty(); }
    }

    double ortho_height() const noexcept { return m_ortho_height; }
    void set_ortho_height(double h) noexcept { if (h > 0.0) { m_ortho_height = h; mark_projection_dirty(); } }

    double near_plane() const noexcept { return m_near; }
    double far_plane()  const noexcept { return m_far; }

    void set_clip_planes(double near_plane, double far_plane) noexcept;

public: 
    const linalg::Matrix4d& view_matrix()                    const noexcept { rebuild_view_if_dirty(); return m_view; }
    const linalg::Matrix4d& inverse_view_matrix()            const noexcept { rebuild_view_if_dirty(); return m_inv_view; }
    const linalg::Matrix4d& projection_matrix()              const noexcept { rebuild_projection_if_dirty(); return m_proj; }
    const linalg::Matrix4d& inverse_projection_matrix()      const noexcept { rebuild_projection_if_dirty(); return m_inv_proj; }
    const linalg::Matrix4d& view_projection_matrix()         const noexcept { rebuild_combined_if_dirty(); return m_view_proj; }
    const linalg::Matrix4d& inverse_view_projection_matrix() const noexcept { rebuild_combined_if_dirty(); return m_inv_view_proj; }
    const Frustum3D&      frustum()                        const noexcept { rebuild_combined_if_dirty(); return m_frustum; }

public:
    ScreenPoint3D world_to_screen(const linalg::vector3d& p) const noexcept;

    ScreenPoint3D world_to_screen(double x, double y, double z) const noexcept { return world_to_screen(linalg::vector3d{x, y, z}); }

    geometry::Ray3D screen_to_ray(double sx, double sy) const noexcept;

    geometry::Ray3D center_ray() const noexcept {
        rebuild_view_if_dirty();
        return { m_position, m_forward };
    }

    linalg::vector3d screen_to_world(double sx, double sy, double ndc_depth) const noexcept;

public: 
    bool is_visible(const linalg::vector3d& p) const noexcept { return frustum().contains(p); }
    bool is_visible(const linalg::vector3d& center, double radius) const noexcept { return frustum().intersects_sphere(center, radius); }
    bool is_visible(const linalg::vector3d& box_min, const linalg::vector3d& box_max) const noexcept { return frustum().intersects_aabb(box_min, box_max); }

private:
    static constexpr double EPS = 1e-12;

    linalg::vector3d     m_position{};
    linalg::QuatD        m_orientation{1.0, 0.0, 0.0, 0.0};
    double       m_pitch_limit = DEFAULT_PITCH_LIMIT;

    Projection3D m_projection   = Projection3D::Perspective;
    DepthRange   m_depth_range  = DepthRange::ZeroToOne;
    double       m_fov_y        = DEFAULT_FOV_Y;
    double       m_ortho_height = 10.0;
    double       m_near         = DEFAULT_NEAR;
    double       m_far          = DEFAULT_FAR;
    unsigned int m_vp_w = 0;
    unsigned int m_vp_h = 0;

    Axes3D       m_axes = default_axes3d();

    mutable linalg::vector3d m_forward{0.0, 0.0, -1.0};
    mutable linalg::vector3d m_right{1.0, 0.0, 0.0};
    mutable linalg::vector3d m_up{0.0, 1.0, 0.0};
    mutable linalg::vector3d m_forward_r{0.0, 0.0, -1.0};
    mutable linalg::vector3d m_right_r{1.0, 0.0, 0.0};
    mutable linalg::vector3d m_up_r{0.0, 1.0, 0.0};

    mutable linalg::Matrix4d m_view          = linalg::Matrix4d::identity();
    mutable linalg::Matrix4d m_inv_view      = linalg::Matrix4d::identity();
    mutable linalg::Matrix4d m_proj          = linalg::Matrix4d::identity();
    mutable linalg::Matrix4d m_inv_proj      = linalg::Matrix4d::identity();
    mutable linalg::Matrix4d m_view_proj     = linalg::Matrix4d::identity();
    mutable linalg::Matrix4d m_inv_view_proj = linalg::Matrix4d::identity();
    mutable Frustum3D      m_frustum{};

    mutable bool m_view_dirty     = true;
    mutable bool m_proj_dirty     = true;
    mutable bool m_combined_dirty = true;

    void mark_view_dirty() noexcept       { m_view_dirty = true; m_combined_dirty = true; }
    void mark_projection_dirty() noexcept { m_proj_dirty = true; m_combined_dirty = true; }

    double depth_min() const noexcept { return m_depth_range == DepthRange::ZeroToOne ? 0.0 : -1.0; }

    double screen_to_ndc_x(double sx) const noexcept { return m_vp_w ? 2.0 * sx / static_cast<double>(m_vp_w) - 1.0 : 0.0; }
    double screen_to_ndc_y(double sy) const noexcept { return m_vp_h ? 1.0 - 2.0 * sy / static_cast<double>(m_vp_h) : 0.0; }

    static double to_radians(double degrees) noexcept { return degrees * constants::PI_180<double>; }
    static double to_degrees(double radians) noexcept { return radians / constants::PI_180<double>; }
    static double tan_of(double radians) noexcept { return std::tan(radians); }
    static double clamp(double v, double lo, double hi) noexcept { return std::max(lo, std::min(v, hi)); }

    static linalg::QuatD axis_angle(const linalg::vector3d& axis, double degrees) noexcept {
        return linalg::QuatD::from_axis_angle(axis.x, axis.y, axis.z, to_radians(degrees));
    }

    static linalg::QuatD quat_from_basis(const linalg::vector3d& x, const linalg::vector3d& y, const linalg::vector3d& z) noexcept;

    void rebuild_view_if_dirty() const noexcept;

    void rebuild_projection_if_dirty() const noexcept;

    void rebuild_combined_if_dirty() const noexcept;

    void extract_frustum() const noexcept;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_CAMERA3D_HPP