#ifndef FIZMO_CAMERA3D_HPP
#define FIZMO_CAMERA3D_HPP

#include <array>
#include <cmath>
#include <limits>
#include "../Basic/constants.hpp"
#include "../Matrices/square_matrices.hpp"
#include "../Quaternions/quaternion.hpp"
#include "../Standard Overloads/abs.hpp"
#include "../Standard Overloads/min_max.hpp"
#include "../Standard Overloads/sqrt.hpp"
#include "../Standard Overloads/trig.hpp"
#include "../Vectors/vectors.hpp"
#include "ray.hpp"

namespace fizmo {
namespace graphics {

enum class Projection3D : unsigned char { Perspective, Orthographic };

enum class DepthRange : unsigned char { ZeroToOne, NegativeOneToOne };

struct Plane3D {
    vector3d normal{};
    double   d = 0.0;
    double signed_distance(const vector3d& p) const noexcept { return normal.dot(p) + d; }
};

struct Frustum3D {
    enum Side : unsigned char { Left = 0, Right, Bottom, Top, Near, Far };
    std::array<Plane3D, 6> planes{};

    bool contains(const vector3d& p) const noexcept {
        for (const auto& pl : planes) if (pl.signed_distance(p) < 0.0) return false;
        return true;
    }

    bool intersects_sphere(const vector3d& center, double radius) const noexcept {
        for (const auto& pl : planes) if (pl.signed_distance(center) < -radius) return false;
        return true;
    }

    bool intersects_aabb(const vector3d& box_min, const vector3d& box_max) const noexcept {
        for (const auto& pl : planes) {
            const vector3d positive{
                pl.normal.x >= 0.0 ? box_max.x : box_min.x,
                pl.normal.y >= 0.0 ? box_max.y : box_min.y,
                pl.normal.z >= 0.0 ? box_max.z : box_min.z
            };
            if (pl.signed_distance(positive) < 0.0) return false;
        }
        return true;
    }
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

    Camera3D(const vector3d& position, unsigned int vp_w, unsigned int vp_h,
             double fov_y_degrees = DEFAULT_FOV_Y, double near_plane = DEFAULT_NEAR, double far_plane = DEFAULT_FAR) noexcept
        : m_position(position), m_vp_w(vp_w), m_vp_h(vp_h)
    {
        set_perspective(fov_y_degrees, near_plane, far_plane);
    }

    static constexpr vector3d world_up() noexcept { return vector3d{0.0, 1.0, 0.0}; }

public: 
    unsigned int viewport_width()  const noexcept { return m_vp_w; }
    unsigned int viewport_height() const noexcept { return m_vp_h; }

    void set_viewport_size(unsigned int w, unsigned int h) noexcept {
        if (w != m_vp_w || h != m_vp_h) { m_vp_w = w; m_vp_h = h; mark_projection_dirty(); }
    }

    double aspect() const noexcept {
        return (m_vp_w == 0 || m_vp_h == 0) ? 1.0 : static_cast<double>(m_vp_w) / static_cast<double>(m_vp_h);
    }

public: 
    const vector3d& position() const noexcept { return m_position; }
    void set_position(const vector3d& p) noexcept { m_position = p; mark_view_dirty(); }
    void set_position(double x, double y, double z) noexcept { set_position(vector3d{x, y, z}); }
    void translate(const vector3d& delta) noexcept { m_position += delta; mark_view_dirty(); }

    void move_local(double right_amount, double up_amount, double forward_amount) noexcept {
        rebuild_view_if_dirty();
        m_position += m_right * right_amount + m_up * up_amount + m_forward * forward_amount;
        mark_view_dirty();
    }

    void move_flat(double forward_amount, double right_amount, double up_amount = 0.0) noexcept {
        const vector3d ff = flat_forward();
        const vector3d fr = ff.cross(world_up());
        m_position += ff * forward_amount + fr * right_amount + world_up() * up_amount;
        mark_view_dirty();
    }

public: 
    const QuatD& orientation() const noexcept { return m_orientation; }

    void set_orientation(const QuatD& q) noexcept {
        if (q.magnitude_squared() <= EPS) return;
        m_orientation = q.normalized();
        mark_view_dirty();
    }

    void set_rotation(double yaw_degrees, double pitch_degrees, double roll_degrees = 0.0) noexcept {
        m_orientation = (axis_angle(world_up(), yaw_degrees)
                       * axis_angle(vector3d{1.0, 0.0, 0.0}, pitch_degrees)
                       * axis_angle(vector3d{0.0, 0.0, 1.0}, roll_degrees)).normalized();
        mark_view_dirty();
    }

    double yaw() const noexcept {
        const vector3d ff = flat_forward();
        return to_degrees(math::atan2_constexpr(-ff.x, -ff.z));
    }

    double pitch() const noexcept {
        rebuild_view_if_dirty();
        return to_degrees(math::asin_constexpr(clamp(m_forward.y, -1.0, 1.0)));
    }

    double roll() const noexcept {
        rebuild_view_if_dirty();
        const vector3d level_right = flat_forward().cross(world_up());
        const vector3d back = -m_forward;
        return to_degrees(math::atan2_constexpr(level_right.cross(m_right).dot(back), level_right.dot(m_right)));
    }

    void add_yaw_pitch(double dyaw_degrees, double dpitch_degrees) noexcept {
        const double new_pitch = clamp(pitch() + dpitch_degrees, -m_pitch_limit, m_pitch_limit);
        set_rotation(yaw() + dyaw_degrees, new_pitch, roll());
    }

    double pitch_limit() const noexcept { return m_pitch_limit; }
    void set_pitch_limit(double degrees) noexcept { m_pitch_limit = clamp(degrees, 0.0, 90.0); }

    void rotate(const vector3d& world_axis, double degrees) noexcept {
        m_orientation = (axis_angle(world_axis, degrees) * m_orientation).normalized();
        mark_view_dirty();
    }

    void rotate_local(const vector3d& local_axis, double degrees) noexcept {
        m_orientation = (m_orientation * axis_angle(local_axis, degrees)).normalized();
        mark_view_dirty();
    }

    void look_at(const vector3d& target, const vector3d& up_hint = world_up()) noexcept {
        look_in(target - m_position, up_hint);
    }

    void look_in(const vector3d& direction, const vector3d& up_hint = world_up()) noexcept {
        const double len = direction.magnitude();
        if (!(len > EPS)) return;
        const vector3d f = direction / len;
        vector3d r = f.cross(up_hint);
        double r_len = r.magnitude();

        if (r_len <= EPS) { 
            const vector3d alt = abs_constexpr(f.z) < 0.9 ? vector3d{0.0, 0.0, 1.0} : vector3d{1.0, 0.0, 0.0};
            r = f.cross(alt);
            r_len = r.magnitude();
        }

        r = r / r_len;
        const vector3d u = r.cross(f);
        m_orientation = quat_from_basis(r, u, -f);
        mark_view_dirty();
    }

    const vector3d& forward() const noexcept { rebuild_view_if_dirty(); return m_forward; }
    const vector3d& right()   const noexcept { rebuild_view_if_dirty(); return m_right; }
    const vector3d& up()      const noexcept { rebuild_view_if_dirty(); return m_up; }

    vector3d flat_forward() const noexcept {
        rebuild_view_if_dirty();
        vector3d ff{m_forward.x, 0.0, m_forward.z};
        if (ff.magnitude_squared() <= EPS) {
            ff = m_forward.y < 0.0 ? vector3d{m_up.x, 0.0, m_up.z} : vector3d{-m_up.x, 0.0, -m_up.z};
        }
        return ff.unit_vector();
    }

public: 
    Projection3D projection_type() const noexcept { return m_projection; }
    DepthRange depth_range() const noexcept { return m_depth_range; }

    void set_depth_range(DepthRange r) noexcept {
        if (r != m_depth_range) { m_depth_range = r; mark_projection_dirty(); }
    }

    void set_perspective(double fov_y_degrees, double near_plane, double far_plane) noexcept {
        m_projection = Projection3D::Perspective;
        set_fov_y(fov_y_degrees);
        set_clip_planes(near_plane, far_plane);
        mark_projection_dirty();
    }

    void set_orthographic(double height, double near_plane, double far_plane) noexcept {
        m_projection = Projection3D::Orthographic;
        set_ortho_height(height);
        set_clip_planes(near_plane, far_plane);
        mark_projection_dirty();
    }

    double fov_y() const noexcept { return m_fov_y; }

    double fov_x() const noexcept {
        const double half = to_radians(m_fov_y) * 0.5;
        return to_degrees(2.0 * math::atan2_constexpr(tan_of(half) * aspect(), 1.0));
    }

    void set_fov_y(double degrees) noexcept {
        if (degrees > 0.0 && degrees < 180.0) { m_fov_y = degrees; mark_projection_dirty(); }
    }

    double ortho_height() const noexcept { return m_ortho_height; }
    void set_ortho_height(double h) noexcept { if (h > 0.0) { m_ortho_height = h; mark_projection_dirty(); } }

    double near_plane() const noexcept { return m_near; }
    double far_plane()  const noexcept { return m_far; }

    void set_clip_planes(double near_plane, double far_plane) noexcept {
        if (near_plane > 0.0 && far_plane > near_plane) { m_near = near_plane; m_far = far_plane; mark_projection_dirty(); }
    }

public: 
    const math::Matrix4d& view_matrix()                    const noexcept { rebuild_view_if_dirty(); return m_view; }
    const math::Matrix4d& inverse_view_matrix()            const noexcept { rebuild_view_if_dirty(); return m_inv_view; }
    const math::Matrix4d& projection_matrix()              const noexcept { rebuild_projection_if_dirty(); return m_proj; }
    const math::Matrix4d& inverse_projection_matrix()      const noexcept { rebuild_projection_if_dirty(); return m_inv_proj; }
    const math::Matrix4d& view_projection_matrix()         const noexcept { rebuild_combined_if_dirty(); return m_view_proj; }
    const math::Matrix4d& inverse_view_projection_matrix() const noexcept { rebuild_combined_if_dirty(); return m_inv_view_proj; }
    const Frustum3D&      frustum()                        const noexcept { rebuild_combined_if_dirty(); return m_frustum; }

public:
    ScreenPoint3D world_to_screen(const vector3d& p) const noexcept {
        const vector4d clip = view_projection_matrix() * vector4d{p.x, p.y, p.z, 1.0};
        ScreenPoint3D sp;
        sp.in_front = clip.w > EPS;
        if (abs_constexpr(clip.w) <= EPS) return sp;
        const double inv_w = 1.0 / clip.w;
        const double nx = clip.x * inv_w, ny = clip.y * inv_w, nz = clip.z * inv_w;
        sp.x = (nx * 0.5 + 0.5) * static_cast<double>(m_vp_w);
        sp.y = (0.5 - ny * 0.5) * static_cast<double>(m_vp_h);
        sp.depth = nz;
        sp.on_screen = sp.in_front && nx >= -1.0 && nx <= 1.0 && ny >= -1.0 && ny <= 1.0 && nz >= depth_min() && nz <= 1.0;
        return sp;
    }

    ScreenPoint3D world_to_screen(double x, double y, double z) const noexcept { return world_to_screen(vector3d{x, y, z}); }

    geometry::Ray3D screen_to_ray(double sx, double sy) const noexcept {
        rebuild_view_if_dirty();
        const double nx = screen_to_ndc_x(sx), ny = screen_to_ndc_y(sy);

        if (m_projection == Projection3D::Perspective) {
            const double ty = tan_of(to_radians(m_fov_y) * 0.5);
            const vector3d dir = m_right * (nx * ty * aspect()) + m_up * (ny * ty) + m_forward;
            return { m_position, dir.unit_vector() };
        }

        const double hh = m_ortho_height * 0.5, hw = hh * aspect();
        return { m_position + m_right * (nx * hw) + m_up * (ny * hh), m_forward };
    }

    geometry::Ray3D center_ray() const noexcept {
        rebuild_view_if_dirty();
        return { m_position, m_forward };
    }

    vector3d screen_to_world(double sx, double sy, double ndc_depth) const noexcept {
        const vector4d v = inverse_view_projection_matrix() * vector4d{screen_to_ndc_x(sx), screen_to_ndc_y(sy), ndc_depth, 1.0};
        if (abs_constexpr(v.w) <= EPS) return m_position;
        return vector3d{v.x / v.w, v.y / v.w, v.z / v.w};
    }

public: 
    bool is_visible(const vector3d& p) const noexcept { return frustum().contains(p); }
    bool is_visible(const vector3d& center, double radius) const noexcept { return frustum().intersects_sphere(center, radius); }
    bool is_visible(const vector3d& box_min, const vector3d& box_max) const noexcept { return frustum().intersects_aabb(box_min, box_max); }

private:
    static constexpr double EPS = 1e-12;

    vector3d     m_position{};
    QuatD        m_orientation{1.0, 0.0, 0.0, 0.0};
    double       m_pitch_limit = DEFAULT_PITCH_LIMIT;

    Projection3D m_projection   = Projection3D::Perspective;
    DepthRange   m_depth_range  = DepthRange::ZeroToOne;
    double       m_fov_y        = DEFAULT_FOV_Y;
    double       m_ortho_height = 10.0;
    double       m_near         = DEFAULT_NEAR;
    double       m_far          = DEFAULT_FAR;
    unsigned int m_vp_w = 0;
    unsigned int m_vp_h = 0;

    mutable vector3d m_forward{0.0, 0.0, -1.0};
    mutable vector3d m_right{1.0, 0.0, 0.0};
    mutable vector3d m_up{0.0, 1.0, 0.0};

    mutable math::Matrix4d m_view          = math::Matrix4d::identity();
    mutable math::Matrix4d m_inv_view      = math::Matrix4d::identity();
    mutable math::Matrix4d m_proj          = math::Matrix4d::identity();
    mutable math::Matrix4d m_inv_proj      = math::Matrix4d::identity();
    mutable math::Matrix4d m_view_proj     = math::Matrix4d::identity();
    mutable math::Matrix4d m_inv_view_proj = math::Matrix4d::identity();
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
    static double tan_of(double radians) noexcept { return math::sin_constexpr(radians) / math::cos_constexpr(radians); }
    static double clamp(double v, double lo, double hi) noexcept { return max_constexpr(lo, min_constexpr(v, hi)); }

    static QuatD axis_angle(const vector3d& axis, double degrees) noexcept {
        return QuatD::from_axis_angle(axis.x, axis.y, axis.z, to_radians(degrees));
    }

    static QuatD quat_from_basis(const vector3d& x, const vector3d& y, const vector3d& z) noexcept {
        const double m00 = x.x, m01 = y.x, m02 = z.x;
        const double m10 = x.y, m11 = y.y, m12 = z.y;
        const double m20 = x.z, m21 = y.z, m22 = z.z;
        const double trace = m00 + m11 + m22;

        if (trace > 0.0) {
            const double s = math::sqrt_constexpr(trace + 1.0) * 2.0;
            return QuatD(0.25 * s, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s).normalized();
        }

        if (m00 > m11 && m00 > m22) {
            const double s = math::sqrt_constexpr(1.0 + m00 - m11 - m22) * 2.0;
            return QuatD((m21 - m12) / s, 0.25 * s, (m01 + m10) / s, (m02 + m20) / s).normalized();
        }

        if (m11 > m22) {
            const double s = math::sqrt_constexpr(1.0 + m11 - m00 - m22) * 2.0;
            return QuatD((m02 - m20) / s, (m01 + m10) / s, 0.25 * s, (m12 + m21) / s).normalized();
        }

        const double s = math::sqrt_constexpr(1.0 + m22 - m00 - m11) * 2.0;
        return QuatD((m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, 0.25 * s).normalized();
    }

    void rebuild_view_if_dirty() const noexcept {
        if (!m_view_dirty) return;
        m_view_dirty = false;
        const auto R = m_orientation.to_rotation_matrix(); 
        m_right   = vector3d{R[0], R[3], R[6]};
        m_up      = vector3d{R[1], R[4], R[7]};
        const vector3d back{R[2], R[5], R[8]};
        m_forward = -back;
        const vector3d& p = m_position;

        m_view = math::Matrix4d{
            m_right.x, m_right.y, m_right.z, -m_right.dot(p),
            m_up.x,    m_up.y,    m_up.z,    -m_up.dot(p),
            back.x,    back.y,    back.z,    -back.dot(p),
            0.0,       0.0,       0.0,       1.0
        };

        m_inv_view = math::Matrix4d{
            m_right.x, m_up.x, back.x, p.x,
            m_right.y, m_up.y, back.y, p.y,
            m_right.z, m_up.z, back.z, p.z,
            0.0,       0.0,    0.0,    1.0
        };
    }

    void rebuild_projection_if_dirty() const noexcept {
        if (!m_proj_dirty) return;
        m_proj_dirty = false;

        const double n = m_near, f = m_far;
        const bool zero_to_one = m_depth_range == DepthRange::ZeroToOne;

        if (m_projection == Projection3D::Perspective) {
            const double sy = 1.0 / tan_of(to_radians(m_fov_y) * 0.5);
            const double sx = sy / aspect();
            const double A = zero_to_one ? f / (n - f)       : (f + n) / (n - f);
            const double B = zero_to_one ? f * n / (n - f)   : 2.0 * f * n / (n - f);

            m_proj = math::Matrix4d{
                sx,  0.0,  0.0, 0.0,
                0.0, sy,   0.0, 0.0,
                0.0, 0.0,  A,   B,
                0.0, 0.0, -1.0, 0.0
            };

            m_inv_proj = math::Matrix4d{
                1.0 / sx, 0.0,      0.0,      0.0,
                0.0,      1.0 / sy, 0.0,      0.0,
                0.0,      0.0,      0.0,     -1.0,
                0.0,      0.0,      1.0 / B,  A / B
            };
        } else {
            const double hh = m_ortho_height * 0.5, hw = hh * aspect();
            const double C = zero_to_one ? -1.0 / (f - n) : -2.0 / (f - n);
            const double D = zero_to_one ? -n / (f - n)   : -(f + n) / (f - n);

            m_proj = math::Matrix4d{
                1.0 / hw, 0.0,      0.0, 0.0,
                0.0,      1.0 / hh, 0.0, 0.0,
                0.0,      0.0,      C,   D,
                0.0,      0.0,      0.0, 1.0
            };

            m_inv_proj = math::Matrix4d{
                hw,  0.0, 0.0,      0.0,
                0.0, hh,  0.0,      0.0,
                0.0, 0.0, 1.0 / C, -D / C,
                0.0, 0.0, 0.0,      1.0
            };
        }
    }

    void rebuild_combined_if_dirty() const noexcept {
        rebuild_view_if_dirty();
        rebuild_projection_if_dirty();
        if (!m_combined_dirty) return;
        m_combined_dirty = false;
        m_view_proj     = m_proj * m_view;
        m_inv_view_proj = m_inv_view * m_inv_proj;
        extract_frustum();
    }

    void extract_frustum() const noexcept {
        const auto& m = m_view_proj.data;
        auto row = [&m](std::size_t r, std::size_t c) { return m[r * 4 + c]; };

        auto make = [&](double s0, std::size_t r0, double s1, std::size_t r1) {
            const double a = s0 * row(r0, 0) + s1 * row(r1, 0);
            const double b = s0 * row(r0, 1) + s1 * row(r1, 1);
            const double c = s0 * row(r0, 2) + s1 * row(r1, 2);
            const double d = s0 * row(r0, 3) + s1 * row(r1, 3);
            const double len = math::sqrt_constexpr(a * a + b * b + c * c);
            const double inv = len > EPS ? 1.0 / len : 0.0;
            return Plane3D{ vector3d{a * inv, b * inv, c * inv}, d * inv };
        };

        auto& p = m_frustum.planes;
        p[Frustum3D::Left]   = make(1.0, 3,  1.0, 0);
        p[Frustum3D::Right]  = make(1.0, 3, -1.0, 0);
        p[Frustum3D::Bottom] = make(1.0, 3,  1.0, 1);
        p[Frustum3D::Top]    = make(1.0, 3, -1.0, 1);
        p[Frustum3D::Near]   = m_depth_range == DepthRange::ZeroToOne ? make(0.0, 3, 1.0, 2) : make(1.0, 3, 1.0, 2);
        p[Frustum3D::Far]    = make(1.0, 3, -1.0, 2);
    }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_CAMERA3D_HPP